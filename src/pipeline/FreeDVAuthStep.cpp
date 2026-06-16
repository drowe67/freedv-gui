//=========================================================================
// Name:            FreeDVAuthStep.cpp
// Purpose:         OpenPGP bookend-frame signing for FreeDV transmissions.
//=========================================================================

#ifdef ENABLE_GNUPG_AUTH

#include "FreeDVAuthStep.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>

#include <gcrypt.h>

#include "../main.h"
#include "freedv_api.h"
#include "util/logging/ulog.h"

extern "C"
{
    void freedv_auth_inject(struct freedv* f, const uint8_t* frame, size_t len);
}

namespace
{
void writeBigEndianU64_(uint8_t* out, uint64_t value)
{
    for (int i = 7; i >= 0; --i)
    {
        out[7 - i] = static_cast<uint8_t>((value >> (i * 8)) & 0xff);
    }
}

void writeBigEndianU16_(uint8_t* out, uint16_t value)
{
    out[0] = static_cast<uint8_t>((value >> 8) & 0xff);
    out[1] = static_cast<uint8_t>(value & 0xff);
}

uint16_t readBigEndianU16_(const uint8_t* in)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(in[0]) << 8) | in[1]);
}

size_t gpgmeDataSize_(gpgme_data_t dh)
{
    off_t len = gpgme_data_seek(dh, 0, SEEK_END);
    if (len < 0)
    {
        return 0;
    }
    gpgme_data_seek(dh, 0, SEEK_SET);
    return static_cast<size_t>(len);
}

std::string extractCallsign_(const uint8_t* field, size_t fieldLen)
{
    size_t end = fieldLen;
    while (end > 0 && field[end - 1] == '\0')
    {
        --end;
    }
    return std::string(reinterpret_cast<const char*>(field), end);
}

const char* signingSubkeyAlgoName_(gpgme_key_t key)
{
    if (key == nullptr || key->subkeys == nullptr)
    {
        return "unknown";
    }

    for (gpgme_subkey_t subkey = key->subkeys; subkey != nullptr; subkey = subkey->next)
    {
        if (subkey->can_sign)
        {
            return gpgme_pubkey_algo_name(subkey->pubkey_algo);
        }
    }

    return gpgme_pubkey_algo_name(key->subkeys->pubkey_algo);
}

bool readGpgmeData_(gpgme_data_t data, std::vector<char>& out)
{
    if (data == nullptr)
    {
        return false;
    }

    size_t len = gpgmeDataSize_(data);
    if (len == 0)
    {
        return false;
    }

    out.resize(len);
    gpgme_data_seek(data, 0, SEEK_SET);
    if (gpgme_data_read(data, out.data(), len) < 0)
    {
        return false;
    }
    return true;
}

std::string toLowerCopy_(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool fieldContainsCallsign_(const char* field, const std::string& callsignLower)
{
    if (field == nullptr || field[0] == '\0' || callsignLower.empty())
    {
        return false;
    }
    return toLowerCopy_(field).find(callsignLower) != std::string::npos;
}

bool keyUidMatchesCallsign_(gpgme_key_t key, const std::string& callsign)
{
    if (key == nullptr || callsign.empty())
    {
        return false;
    }

    const std::string callsignLower = toLowerCopy_(callsign);
    for (gpgme_user_id_t uid = key->uids; uid != nullptr; uid = uid->next)
    {
        if (uid->revoked || uid->invalid)
        {
            continue;
        }

        if (fieldContainsCallsign_(uid->name, callsignLower)
            || fieldContainsCallsign_(uid->comment, callsignLower)
            || fieldContainsCallsign_(uid->email, callsignLower)
            || fieldContainsCallsign_(uid->address, callsignLower)
            || fieldContainsCallsign_(uid->uid, callsignLower))
        {
            return true;
        }
    }
    return false;
}

class GpgmeKeylistModeGuard_
{
public:
    GpgmeKeylistModeGuard_(gpgme_ctx_t ctx, gpgme_keylist_mode_t mode)
        : ctx_(ctx)
        , previousMode_(gpgme_get_keylist_mode(ctx))
    {
        gpgme_set_keylist_mode(ctx_, mode);
    }

    ~GpgmeKeylistModeGuard_()
    {
        gpgme_set_keylist_mode(ctx_, previousMode_);
    }

private:
    gpgme_ctx_t ctx_;
    gpgme_keylist_mode_t previousMode_;
};

bool keylistNextMatchingCallsign_(
    gpgme_ctx_t ctx,
    const std::string& callsign,
    bool requireCallsignMatch,
    gpgme_key_t* outKey)
{
    if (ctx == nullptr || outKey == nullptr)
    {
        return false;
    }

    *outKey = nullptr;
    gpgme_key_t key = nullptr;
    while (gpgme_op_keylist_next(ctx, &key) == GPG_ERR_NO_ERROR && key != nullptr)
    {
        if (!requireCallsignMatch || keyUidMatchesCallsign_(key, callsign))
        {
            *outKey = key;
            return true;
        }
        gpgme_key_unref(key);
        key = nullptr;
    }
    return false;
}

bool locateKeyByPattern_(
    gpgme_ctx_t ctx,
    const std::string& pattern,
    const std::string& callsign,
    bool requireCallsignMatch,
    gpgme_keylist_mode_t mode,
    gpgme_key_t* outKey)
{
    if (ctx == nullptr || outKey == nullptr || pattern.empty())
    {
        return false;
    }

    *outKey = nullptr;
    GpgmeKeylistModeGuard_ modeGuard(ctx, mode);

    gpgme_error_t err = gpgme_op_keylist_start(ctx, pattern.c_str(), 0);
    if (err != GPG_ERR_NO_ERROR)
    {
        log_warn("FreeDVAuthStep: key search for '%s' failed: %s", pattern.c_str(), gpgme_strerror(err));
        return false;
    }

    const bool found = keylistNextMatchingCallsign_(ctx, callsign, requireCallsignMatch, outKey);
    gpgme_op_keylist_end(ctx);
    return found;
}
} // namespace

bool FreeDVAuthStep::hasSigningSubkey_(gpgme_key_t key)
{
    if (key == nullptr || key->subkeys == nullptr)
    {
        return false;
    }

    for (gpgme_subkey_t subkey = key->subkeys; subkey != nullptr; subkey = subkey->next)
    {
        if (subkey->can_sign)
        {
            return true;
        }
    }
    return false;
}

size_t FreeDVAuthStep::parseFrameLength_(const uint8_t* data, size_t len)
{
    if (data == nullptr || len < AUTH_HEADER_LEN)
    {
        return 0;
    }

    const uint16_t sigLen = readBigEndianU16_(data + AUTH_PREFIX_LEN);
    if (sigLen > AUTH_SIG_LEN_MAX)
    {
        return 0;
    }

    return AUTH_HEADER_LEN + sigLen;
}

FreeDVAuthStep::FreeDVAuthStep(struct freedv* dv, StateChangeFn stateChangeFn)
    : dv_(dv)
    , stateChangeFn_(std::move(stateChangeFn))
    , state_(State::UNSIGNED)
    , gpgCtx_(nullptr)
    , rxStartSeen_(false)
    , rxStartFrameLen_(0)
    , rxAuthSeenThisSync_(false)
{
    memset(rxAccumHash_, 0, sizeof(rxAccumHash_));
    memset(rxStartFrame_, 0, sizeof(rxStartFrame_));
    memset(txAccumHash_, 0, sizeof(txAccumHash_));

    gpgme_check_version(nullptr);
    gcry_check_version(nullptr);

    gpgme_error_t err = gpgme_new(&gpgCtx_);
    if (err != GPG_ERR_NO_ERROR)
    {
        log_error("FreeDVAuthStep: gpgme_new failed: %s", gpgme_strerror(err));
        gpgCtx_ = nullptr;
        setState_(State::NO_KEY);
        return;
    }

    gpgme_set_protocol(gpgCtx_, GPGME_PROTOCOL_OpenPGP);
    validateConfiguredKey_();
    setState_(getIdleState());
}

FreeDVAuthStep::~FreeDVAuthStep()
{
    if (txWorker_ && txWorker_->joinable())
    {
        txWorker_->join();
    }
    txWorker_.reset();

    if (gpgCtx_ != nullptr)
    {
        gpgme_release(gpgCtx_);
        gpgCtx_ = nullptr;
    }
}

void FreeDVAuthStep::setState_(State newState)
{
    state_.store(newState, std::memory_order_release);
    if (stateChangeFn_)
    {
        stateChangeFn_(newState);
    }
}

FreeDVAuthStep::AuthConfig FreeDVAuthStep::loadConfig_() const
{
    AuthConfig config;
    config.enableSigning = wxGetApp().appConfiguration.gnupgAuthEnableSigning.getWithoutProcessing();
    config.keyFingerprint = wxGetApp().appConfiguration.gnupgAuthKeyFingerprint.getWithoutProcessing().ToStdString();

    wxString callsign = wxGetApp().appConfiguration.reportingConfiguration.reportingCallsign.getWithoutProcessing();
    if (callsign.IsEmpty())
    {
        callsign = wxGetApp().appConfiguration.reportingConfiguration.reportingFreeTextString.getWithoutProcessing();
    }
    config.callsign = callsign.ToStdString();
    return config;
}

void FreeDVAuthStep::validateConfiguredKey_()
{
    AuthConfig config = loadConfig_();
    if (!config.enableSigning || config.keyFingerprint.empty())
    {
        return;
    }

    validateSigningKeyFingerprint(config.keyFingerprint);
}

bool FreeDVAuthStep::validateSigningKeyFingerprint(const std::string& fingerprint)
{
    if (fingerprint.empty())
    {
        return false;
    }

    gpgme_ctx_t ctx = nullptr;
    if (gpgme_new(&ctx) != GPG_ERR_NO_ERROR)
    {
        log_error("FreeDVAuthStep: gpgme_new failed during key validation");
        return false;
    }

    gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);

    gpgme_key_t key = nullptr;
    gpgme_error_t err = gpgme_get_key(ctx, fingerprint.c_str(), &key, 1);
    if (err != GPG_ERR_NO_ERROR || key == nullptr)
    {
        log_error(
            "FreeDVAuthStep: signing key '%s' not found or no secret key available: %s",
            fingerprint.c_str(),
            gpgme_strerror(err));
        gpgme_release(ctx);
        return false;
    }

    if (!hasSigningSubkey_(key))
    {
        log_error(
            "FreeDVAuthStep: key '%s' (%s) has no signing-capable subkey",
            fingerprint.c_str(),
            signingSubkeyAlgoName_(key));
        gpgme_key_unref(key);
        gpgme_release(ctx);
        return false;
    }

    log_info(
        "FreeDVAuthStep: active signing key %s (%s)",
        fingerprint.c_str(),
        signingSubkeyAlgoName_(key));

    gpgme_key_unref(key);
    gpgme_release(ctx);
    return true;
}

void FreeDVAuthStep::sha256_(const uint8_t* data, size_t len, uint8_t* outHash) const
{
    gcry_md_hash_buffer(GCRY_MD_SHA256, outHash, data, len);
}

void FreeDVAuthStep::buildStartFrame_(uint8_t* frame, const AuthConfig& config)
{
    memset(frame, 0, AUTH_PREFIX_LEN);
    frame[0] = AUTH_TYPE_START;

    auto now = static_cast<uint64_t>(time(nullptr));
    writeBigEndianU64_(frame + 1, now);

    strncpy(reinterpret_cast<char*>(frame + 9), config.callsign.c_str(), AUTH_CALLSIGN_FIELD_SIZE);
    writeBigEndianU16_(frame + AUTH_PREFIX_LEN, 0);
}

void FreeDVAuthStep::buildEndFrame_(uint8_t* frame, const AuthConfig& config, const uint8_t* hash)
{
    memset(frame, 0, AUTH_PREFIX_LEN);
    frame[0] = AUTH_TYPE_END;

    auto now = static_cast<uint64_t>(time(nullptr));
    writeBigEndianU64_(frame + 1, now);

    strncpy(reinterpret_cast<char*>(frame + 9), config.callsign.c_str(), AUTH_CALLSIGN_FIELD_SIZE);
    memcpy(frame + 29, hash, AUTH_HASH_FIELD_SIZE);
    writeBigEndianU16_(frame + AUTH_PREFIX_LEN, 0);
}

bool FreeDVAuthStep::signFrame_(uint8_t* frame, size_t frameCapacity, size_t* frameLenOut, const std::string& fingerprint)
{
    if (gpgCtx_ == nullptr || fingerprint.empty() || frame == nullptr || frameLenOut == nullptr
        || frameCapacity < AUTH_HEADER_LEN)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(gpgMutex_);

    gpgme_key_t key = nullptr;
    gpgme_error_t err = gpgme_get_key(gpgCtx_, fingerprint.c_str(), &key, 1);
    if (err != GPG_ERR_NO_ERROR || key == nullptr)
    {
        log_error("FreeDVAuthStep: signing key '%s' not found: %s", fingerprint.c_str(), gpgme_strerror(err));
        return false;
    }

    if (!hasSigningSubkey_(key))
    {
        log_error(
            "FreeDVAuthStep: key '%s' (%s) has no signing-capable subkey",
            fingerprint.c_str(),
            signingSubkeyAlgoName_(key));
        gpgme_key_unref(key);
        return false;
    }

    gpgme_signers_clear(gpgCtx_);
    err = gpgme_signers_add(gpgCtx_, key);
    if (err != GPG_ERR_NO_ERROR)
    {
        log_error("FreeDVAuthStep: gpgme_signers_add failed: %s", gpgme_strerror(err));
        gpgme_key_unref(key);
        return false;
    }

    gpgme_data_t plain = nullptr;
    gpgme_data_t sig = nullptr;
    err = gpgme_data_new_from_mem(
        &plain, reinterpret_cast<const char*>(frame), AUTH_PREFIX_LEN, 0);
    if (err != GPG_ERR_NO_ERROR)
    {
        gpgme_signers_clear(gpgCtx_);
        gpgme_key_unref(key);
        return false;
    }

    err = gpgme_data_new(&sig);
    if (err != GPG_ERR_NO_ERROR)
    {
        gpgme_data_release(plain);
        gpgme_signers_clear(gpgCtx_);
        gpgme_key_unref(key);
        return false;
    }

    err = gpgme_op_sign(gpgCtx_, plain, sig, GPGME_SIG_MODE_DETACH);
    gpgme_data_release(plain);

    if (err != GPG_ERR_NO_ERROR)
    {
        log_error("FreeDVAuthStep: gpgme_op_sign failed: %s", gpgme_strerror(err));
        gpgme_data_release(sig);
        gpgme_signers_clear(gpgCtx_);
        gpgme_key_unref(key);
        return false;
    }

    gpgme_sign_result_t signResult = gpgme_op_sign_result(gpgCtx_);
    if (signResult != nullptr && signResult->invalid_signers != nullptr)
    {
        log_error("FreeDVAuthStep: signature generation failed (invalid signer)");
        gpgme_data_release(sig);
        gpgme_signers_clear(gpgCtx_);
        gpgme_key_unref(key);
        return false;
    }

    std::vector<char> sigBuf;
    if (!readGpgmeData_(sig, sigBuf))
    {
        log_error("FreeDVAuthStep: could not read detached OpenPGP signature");
        gpgme_data_release(sig);
        gpgme_signers_clear(gpgCtx_);
        gpgme_key_unref(key);
        return false;
    }

    gpgme_data_release(sig);
    gpgme_signers_clear(gpgCtx_);
    gpgme_key_unref(key);

    if (sigBuf.size() > AUTH_SIG_LEN_MAX || AUTH_HEADER_LEN + sigBuf.size() > frameCapacity)
    {
        log_error("FreeDVAuthStep: signature length %zu exceeds frame capacity", sigBuf.size());
        return false;
    }

    writeBigEndianU16_(frame + AUTH_PREFIX_LEN, static_cast<uint16_t>(sigBuf.size()));
    memcpy(frame + AUTH_HEADER_LEN, sigBuf.data(), sigBuf.size());
    *frameLenOut = AUTH_HEADER_LEN + sigBuf.size();
    return true;
}

bool FreeDVAuthStep::lookupPublicKeyFromKeyserver_(const std::string& callsign, gpgme_ctx_t ctx, gpgme_key_t* outKey)
{
    if (ctx == nullptr || outKey == nullptr || callsign.empty())
    {
        return false;
    }

    *outKey = nullptr;

    log_info("FreeDVAuthStep: searching keyservers for callsign '%s'", callsign.c_str());

    gpgme_key_t key = nullptr;
    if (!locateKeyByPattern_(ctx, callsign, callsign, true, GPGME_KEYLIST_MODE_LOCATE_EXTERNAL, &key))
    {
        return false;
    }

    log_info(
        "FreeDVAuthStep: imported public key for '%s' from keyserver (fingerprint %s)",
        callsign.c_str(),
        key->subkeys != nullptr ? key->subkeys->fpr : "unknown");

    *outKey = key;
    return true;
}

bool FreeDVAuthStep::lookupPublicKey_(const std::string& callsign, gpgme_ctx_t ctx, gpgme_key_t* outKey)
{
    if (ctx == nullptr || outKey == nullptr || callsign.empty())
    {
        return false;
    }

    *outKey = nullptr;

    gpgme_key_t key = nullptr;
    if (locateKeyByPattern_(ctx, callsign, callsign, true, GPGME_KEYLIST_MODE_LOCAL, &key))
    {
        *outKey = key;
        return true;
    }

    return lookupPublicKeyFromKeyserver_(callsign, ctx, outKey);
}

bool FreeDVAuthStep::fetchPublicKeyForSender_(
    const std::string& callsign,
    gpgme_ctx_t ctx,
    const char* signatureFpr)
{
    if (ctx == nullptr || callsign.empty())
    {
        return false;
    }

    gpgme_key_t key = nullptr;

    if (signatureFpr != nullptr && signatureFpr[0] != '\0')
    {
        log_info("FreeDVAuthStep: locating signing key %s for sender '%s'", signatureFpr, callsign.c_str());
        if (locateKeyByPattern_(ctx, signatureFpr, callsign, false, GPGME_KEYLIST_MODE_LOCATE, &key))
        {
            gpgme_key_unref(key);
            return true;
        }
    }

    if (lookupPublicKey_(callsign, ctx, &key))
    {
        gpgme_key_unref(key);
        return true;
    }

    return false;
}

FreeDVAuthStep::VerifyResult FreeDVAuthStep::verifyFrame_(const uint8_t* frame, size_t frameLen, const uint8_t* hashOverride)
{
    if (gpgCtx_ == nullptr)
    {
        return VerifyResult::UNSIGNED;
    }

    const size_t expectedLen = parseFrameLength_(frame, frameLen);
    if (expectedLen == 0 || frameLen < expectedLen)
    {
        return VerifyResult::UNSIGNED;
    }

    std::string sender = extractCallsign_(frame + 9, AUTH_CALLSIGN_FIELD_SIZE);
    if (sender.empty())
    {
        return VerifyResult::UNSIGNED;
    }

    uint8_t signedPrefix[AUTH_PREFIX_LEN];
    memcpy(signedPrefix, frame, AUTH_PREFIX_LEN);
    if (hashOverride != nullptr)
    {
        memcpy(signedPrefix + 29, hashOverride, AUTH_HASH_FIELD_SIZE);
    }

    const uint16_t sigLen = readBigEndianU16_(frame + AUTH_PREFIX_LEN);
    const uint8_t* sigBytes = frame + AUTH_HEADER_LEN;

    std::lock_guard<std::mutex> lock(gpgMutex_);

    gpgme_data_t sigData = nullptr;
    gpgme_data_t signedData = nullptr;
    gpgme_error_t err = gpgme_data_new_from_mem(
        &sigData, reinterpret_cast<const char*>(sigBytes), sigLen, 0);
    if (err != GPG_ERR_NO_ERROR)
    {
        return VerifyResult::UNSIGNED;
    }

    err = gpgme_data_new_from_mem(
        &signedData, reinterpret_cast<const char*>(signedPrefix), AUTH_PREFIX_LEN, 0);
    if (err != GPG_ERR_NO_ERROR)
    {
        gpgme_data_release(sigData);
        return VerifyResult::UNSIGNED;
    }

    err = gpgme_op_verify(gpgCtx_, sigData, signedData, nullptr);
    gpgme_verify_result_t verifyResult = gpgme_op_verify_result(gpgCtx_);
    gpgme_data_release(sigData);
    gpgme_data_release(signedData);

    if (verifyResult == nullptr || verifyResult->signatures == nullptr)
    {
        log_warn("FreeDVAuthStep: gpgme_op_verify produced no signature result: %s", gpgme_strerror(err));
        return VerifyResult::UNSIGNED;
    }

    gpgme_error_t sigStatus = verifyResult->signatures->status;
    if (sigStatus == GPG_ERR_NO_ERROR)
    {
        return VerifyResult::OK;
    }

    if (gpg_err_code(sigStatus) == GPG_ERR_NO_PUBKEY)
    {
        const char* signatureFpr = verifyResult->signatures->fpr;
        if (fetchPublicKeyForSender_(sender, gpgCtx_, signatureFpr))
        {
            err = gpgme_data_new_from_mem(
                &sigData, reinterpret_cast<const char*>(sigBytes), sigLen, 0);
            if (err != GPG_ERR_NO_ERROR)
            {
                return VerifyResult::NO_KEY;
            }

            err = gpgme_data_new_from_mem(
                &signedData, reinterpret_cast<const char*>(signedPrefix), AUTH_PREFIX_LEN, 0);
            if (err != GPG_ERR_NO_ERROR)
            {
                gpgme_data_release(sigData);
                return VerifyResult::NO_KEY;
            }

            err = gpgme_op_verify(gpgCtx_, sigData, signedData, nullptr);
            verifyResult = gpgme_op_verify_result(gpgCtx_);
            gpgme_data_release(sigData);
            gpgme_data_release(signedData);

            if (verifyResult != nullptr && verifyResult->signatures != nullptr
                && verifyResult->signatures->status == GPG_ERR_NO_ERROR)
            {
                log_info("FreeDVAuthStep: verified sender '%s' after key import", sender.c_str());
                return VerifyResult::OK;
            }
        }

        log_warn("FreeDVAuthStep: no public key for sender '%s'", sender.c_str());
        return VerifyResult::NO_KEY;
    }

    log_warn(
        "FreeDVAuthStep: signature verification failed for sender '%s': %s",
        sender.c_str(),
        gpgme_strerror(sigStatus));
    return VerifyResult::UNSIGNED;
}

void FreeDVAuthStep::queueAuthFrameForTx_(const uint8_t* frame, size_t len)
{
    if (frame == nullptr || len == 0 || len > AUTH_FRAME_MAX)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(txQueueMutex_);
        txPayloadQueue_.emplace_back(frame, frame + len);
    }

    if (dv_ != nullptr)
    {
        freedv_auth_inject(dv_, frame, len);
    }
}

void FreeDVAuthStep::txWorkerEntry_(uint8_t frameType, const AuthConfig& config, const uint8_t* hash)
{
    uint8_t frame[AUTH_FRAME_MAX];
    if (frameType == AUTH_TYPE_START)
    {
        buildStartFrame_(frame, config);
    }
    else
    {
        buildEndFrame_(frame, config, hash);
    }

    State resultState = State::UNSIGNED;
    if (!config.enableSigning || config.keyFingerprint.empty())
    {
        resultState = State::UNSIGNED;
    }
    else
    {
        size_t frameLen = 0;
        if (signFrame_(frame, sizeof(frame), &frameLen, config.keyFingerprint))
        {
            queueAuthFrameForTx_(frame, frameLen);
            resultState = State::SIGNED;
        }
        else
        {
            resultState = (getIdleState() == State::KEY_LOADED) ? State::UNSIGNED : State::NO_KEY;
        }
    }

    setState_(resultState);
}

void FreeDVAuthStep::processTxStart()
{
    if (txWorker_ && txWorker_->joinable())
    {
        txWorker_->join();
    }

    {
        std::lock_guard<std::mutex> lock(txHashMutex_);
        memset(txAccumHash_, 0, sizeof(txAccumHash_));
    }

    {
        std::lock_guard<std::mutex> lock(txQueueMutex_);
        txPayloadQueue_.clear();
    }

    AuthConfig config = loadConfig_();
    if (!config.enableSigning)
    {
        setState_(State::UNSIGNED);
        return;
    }

    State idleState = getIdleState();
    if (idleState != State::KEY_LOADED)
    {
        setState_(idleState);
        return;
    }

    setState_(State::KEY_LOADED);

    txWorker_ = std::make_unique<std::thread>([this, config]() {
        txWorkerEntry_(AUTH_TYPE_START, config, nullptr);
    });
}

void FreeDVAuthStep::processTxEnd()
{
    AuthConfig config = loadConfig_();
    uint8_t hash[AUTH_HASH_FIELD_SIZE];

    {
        std::lock_guard<std::mutex> lock(txHashMutex_);
        memcpy(hash, txAccumHash_, sizeof(hash));
        memset(txAccumHash_, 0, sizeof(txAccumHash_));
    }

    if (txWorker_ && txWorker_->joinable())
    {
        txWorker_->join();
    }

    if (!config.enableSigning)
    {
        return;
    }

    txWorker_ = std::make_unique<std::thread>([this, config, hash]() {
        txWorkerEntry_(AUTH_TYPE_END, config, hash);
    });
    txWorker_->join();
    setState_(getIdleState());
}

void FreeDVAuthStep::accumulateTxPayload(const uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(txHashMutex_);
    uint8_t updated[AUTH_HASH_FIELD_SIZE];
    sha256_(data, len, updated);

    uint8_t concat[AUTH_HASH_FIELD_SIZE * 2];
    memcpy(concat, txAccumHash_, AUTH_HASH_FIELD_SIZE);
    memcpy(concat + AUTH_HASH_FIELD_SIZE, updated, AUTH_HASH_FIELD_SIZE);
    sha256_(concat, sizeof(concat), txAccumHash_);
}

void FreeDVAuthStep::accumulateRxPayload(const uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(rxMutex_);
    if (!rxStartSeen_)
    {
        return;
    }

    uint8_t updated[AUTH_HASH_FIELD_SIZE];
    sha256_(data, len, updated);

    uint8_t concat[AUTH_HASH_FIELD_SIZE * 2];
    memcpy(concat, rxAccumHash_, AUTH_HASH_FIELD_SIZE);
    memcpy(concat + AUTH_HASH_FIELD_SIZE, updated, AUTH_HASH_FIELD_SIZE);
    sha256_(concat, sizeof(concat), rxAccumHash_);
}

bool FreeDVAuthStep::isAuthFrameHeader_(const uint8_t* data, size_t len, uint8_t* typeOut) const
{
    if (data == nullptr || len < 1)
    {
        return false;
    }

    if (data[0] != AUTH_TYPE_START && data[0] != AUTH_TYPE_END)
    {
        return false;
    }

    if (typeOut != nullptr)
    {
        *typeOut = data[0];
    }
    return true;
}

void FreeDVAuthStep::resetRxSession()
{
    std::lock_guard<std::mutex> lock(rxMutex_);
    rxStartSeen_ = false;
    rxStartFrameLen_ = 0;
    rxAuthSeenThisSync_ = false;
    memset(rxAccumHash_, 0, sizeof(rxAccumHash_));
    memset(rxStartFrame_, 0, sizeof(rxStartFrame_));
    rxSenderCallsign_.clear();
    rxFrameBuffer_.clear();
    setState_(getIdleState());
}

void FreeDVAuthStep::onSyncAcquired()
{
    std::lock_guard<std::mutex> lock(rxMutex_);
    rxStartSeen_ = false;
    rxStartFrameLen_ = 0;
    rxAuthSeenThisSync_ = false;
    memset(rxAccumHash_, 0, sizeof(rxAccumHash_));
    memset(rxStartFrame_, 0, sizeof(rxStartFrame_));
    rxSenderCallsign_.clear();
    rxFrameBuffer_.clear();
}

void FreeDVAuthStep::onSyncLost()
{
    bool markUnsigned = false;
    {
        std::lock_guard<std::mutex> lock(rxMutex_);
        markUnsigned = !rxAuthSeenThisSync_;
        rxStartSeen_ = false;
        rxStartFrameLen_ = 0;
        rxAuthSeenThisSync_ = false;
        memset(rxAccumHash_, 0, sizeof(rxAccumHash_));
        memset(rxStartFrame_, 0, sizeof(rxStartFrame_));
        rxSenderCallsign_.clear();
        rxFrameBuffer_.clear();
    }

    if (markUnsigned)
    {
        setState_(State::UNSIGNED);
    }
}

FreeDVAuthStep::State FreeDVAuthStep::getIdleState() const
{
    if (gpgCtx_ == nullptr)
    {
        return State::NO_KEY;
    }

    AuthConfig config = loadConfig_();
    if (!config.enableSigning)
    {
        return State::UNSIGNED;
    }

    if (config.keyFingerprint.empty())
    {
        return State::NO_KEY;
    }

    std::lock_guard<std::mutex> lock(gpgMutex_);
    gpgme_key_t key = nullptr;
    gpgme_error_t err = gpgme_get_key(gpgCtx_, config.keyFingerprint.c_str(), &key, 1);
    if (err != GPG_ERR_NO_ERROR || key == nullptr || !hasSigningSubkey_(key))
    {
        if (key != nullptr)
        {
            gpgme_key_unref(key);
        }
        return State::NO_KEY;
    }

    gpgme_key_unref(key);
    return State::KEY_LOADED;
}

void FreeDVAuthStep::processRxFrame(const uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0)
    {
        return;
    }

    size_t frameLen = len;
    const uint8_t* frameData = data;
    std::vector<uint8_t> completeFrame;

    if (len < AUTH_HEADER_LEN)
    {
        {
            std::lock_guard<std::mutex> lock(rxMutex_);
            rxFrameBuffer_.insert(rxFrameBuffer_.end(), data, data + len);

            if (rxFrameBuffer_.size() >= AUTH_HEADER_LEN)
            {
                const size_t expectedLen = parseFrameLength_(rxFrameBuffer_.data(), rxFrameBuffer_.size());
                if (expectedLen > 0 && rxFrameBuffer_.size() >= expectedLen)
                {
                    completeFrame.assign(rxFrameBuffer_.begin(), rxFrameBuffer_.begin() + expectedLen);
                    rxFrameBuffer_.erase(rxFrameBuffer_.begin(), rxFrameBuffer_.begin() + expectedLen);
                }
            }
        }

        if (completeFrame.empty())
        {
            return;
        }

        frameData = completeFrame.data();
        frameLen = completeFrame.size();
    }
    else
    {
        const size_t expectedLen = parseFrameLength_(data, len);
        if (expectedLen == 0)
        {
            accumulateRxPayload(data, len);
            return;
        }
        if (len < expectedLen)
        {
            {
                std::lock_guard<std::mutex> lock(rxMutex_);
                rxFrameBuffer_.insert(rxFrameBuffer_.end(), data, data + len);
                if (rxFrameBuffer_.size() >= expectedLen)
                {
                    completeFrame.assign(rxFrameBuffer_.begin(), rxFrameBuffer_.begin() + expectedLen);
                    rxFrameBuffer_.erase(rxFrameBuffer_.begin(), rxFrameBuffer_.begin() + expectedLen);
                }
            }

            if (completeFrame.empty())
            {
                return;
            }

            frameData = completeFrame.data();
            frameLen = completeFrame.size();
        }
        else if (len > expectedLen)
        {
            processRxFrame(data, expectedLen);
            processRxFrame(data + expectedLen, len - expectedLen);
            return;
        }
    }

    uint8_t type = 0;
    if (!isAuthFrameHeader_(frameData, frameLen, &type))
    {
        accumulateRxPayload(frameData, frameLen);
        return;
    }

    if (type == AUTH_TYPE_START)
    {
        std::lock_guard<std::mutex> lock(rxMutex_);
        rxAuthSeenThisSync_ = true;
        rxStartSeen_ = true;
        rxStartFrameLen_ = frameLen;
        memset(rxAccumHash_, 0, sizeof(rxAccumHash_));
        memcpy(rxStartFrame_, frameData, frameLen);
        rxSenderCallsign_ = extractCallsign_(frameData + 9, AUTH_CALLSIGN_FIELD_SIZE);
        setState_(getIdleState());
        return;
    }

    if (type == AUTH_TYPE_END)
    {
        std::lock_guard<std::mutex> lock(rxMutex_);
        rxAuthSeenThisSync_ = true;
        if (!rxStartSeen_)
        {
            setState_(State::UNSIGNED);
            return;
        }

        const uint8_t* frameHash = frameData + 29;
        if (memcmp(frameHash, rxAccumHash_, AUTH_HASH_FIELD_SIZE) != 0)
        {
            log_warn("FreeDVAuthStep: END frame hash mismatch");
            setState_(State::UNSIGNED);
            rxStartSeen_ = false;
            return;
        }

        VerifyResult verifyResult = verifyFrame_(frameData, frameLen, frameHash);
        if (verifyResult == VerifyResult::OK)
        {
            setState_(State::SIGNED);
        }
        else if (verifyResult == VerifyResult::NO_KEY)
        {
            setState_(State::NO_KEY);
        }
        else
        {
            setState_(State::UNSIGNED);
        }

        rxStartSeen_ = false;
    }
}

bool FreeDVAuthStep::hasPendingTxPayload() const
{
    std::lock_guard<std::mutex> lock(txQueueMutex_);
    return !txPayloadQueue_.empty();
}

bool FreeDVAuthStep::popTxPayload(uint8_t* packet, size_t* size)
{
    if (packet == nullptr || size == nullptr)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(txQueueMutex_);
    if (txPayloadQueue_.empty())
    {
        *size = 0;
        return false;
    }

    auto& front = txPayloadQueue_.front();
    if (front.size() > *size)
    {
        return false;
    }

    memcpy(packet, front.data(), front.size());
    *size = front.size();
    txPayloadQueue_.erase(txPayloadQueue_.begin());
    if (dv_ != nullptr)
    {
        freedv_auth_inject(dv_, nullptr, 0);
    }
    return true;
}

std::string FreeDVAuthStep::detectHardwareToken()
{
    gpgme_ctx_t ctx = nullptr;
    if (gpgme_new(&ctx) != GPG_ERR_NO_ERROR)
    {
        return "none";
    }

    gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);

    std::string result = "none";
    if (gpgme_op_keylist_start(ctx, nullptr, GPGME_KEYLIST_MODE_SIGS) == GPG_ERR_NO_ERROR)
    {
        gpgme_key_t key = nullptr;
        while (gpgme_op_keylist_next(ctx, &key) == GPG_ERR_NO_ERROR && key != nullptr)
        {
            for (gpgme_subkey_t subkey = key->subkeys; subkey != nullptr; subkey = subkey->next)
            {
                if (subkey->card_number != nullptr && subkey->card_number[0] != '\0')
                {
                    result = subkey->card_number;
                    gpgme_key_unref(key);
                    gpgme_op_keylist_end(ctx);
                    gpgme_release(ctx);
                    return result;
                }
            }
            gpgme_key_unref(key);
            key = nullptr;
        }
        gpgme_op_keylist_end(ctx);
    }

    gpgme_release(ctx);
    return result;
}

#endif // ENABLE_GNUPG_AUTH
