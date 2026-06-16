//=========================================================================
// Name:            FreeDVAuthStep.h
// Purpose:         OpenPGP bookend-frame signing for FreeDV transmissions.
//
// Auth frame layout (variable length):
//   [1 byte  type: 0x41=START, 0x45=END]
//   [8 bytes UTC timestamp, big-endian Unix seconds]
//   [20 bytes callsign, null-padded]
//   [32 bytes SHA-256 hash field (zeros in START, payload hash in END)]
//   [2 bytes signature length, big-endian uint16]
//   [N bytes OpenPGP detached signature]
//
// Total: 63 + N bytes.  Bytes 0..60 (61 bytes) are signed via gpgme_op_sign().
//
// Authors:         FreeDV Contributors
//=========================================================================

#ifndef AUDIO_PIPELINE__FREEDV_AUTH_STEP_H
#define AUDIO_PIPELINE__FREEDV_AUTH_STEP_H

#ifdef ENABLE_GNUPG_AUTH

#include <gpgme.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "freedv_sanitizers.h"

struct freedv;

class FreeDVAuthStep
{
public:
    static constexpr size_t AUTH_PREFIX_LEN = 61;
    static constexpr size_t AUTH_HEADER_LEN = 63;
    static constexpr size_t AUTH_HASH_FIELD_SIZE = 32;
    static constexpr size_t AUTH_CALLSIGN_FIELD_SIZE = 20;
    static constexpr size_t AUTH_SIG_LEN_MAX = 512;
    static constexpr size_t AUTH_FRAME_MAX = AUTH_HEADER_LEN + AUTH_SIG_LEN_MAX;
    static constexpr size_t AUTH_DATA_CHUNK_MAX = 63;
    static constexpr uint8_t AUTH_TYPE_START = 0x41;
    static constexpr uint8_t AUTH_TYPE_END = 0x45;

    enum class State
    {
        SIGNED,
        UNSIGNED,
        NO_KEY,
        KEY_LOADED
    };

    using StateChangeFn = std::function<void(State)>;

    FreeDVAuthStep(struct freedv* dv, StateChangeFn stateChangeFn);
    ~FreeDVAuthStep();

    State getState() const { return state_.load(std::memory_order_acquire); }

    void processTxStart();
    void processTxEnd();
    void processRxFrame(const uint8_t* data, size_t len);

    bool hasPendingTxPayload() const;
    bool popTxPayload(uint8_t* packet, size_t* size);

    void accumulateTxPayload(const uint8_t* data, size_t len);
    void accumulateRxPayload(const uint8_t* data, size_t len);
    void resetRxSession();
    void onSyncAcquired();
    void onSyncLost();

    State getIdleState() const;

    static std::string detectHardwareToken();
    static bool validateSigningKeyFingerprint(const std::string& fingerprint);

private:
    enum class VerifyResult
    {
        OK,
        UNSIGNED,
        NO_KEY
    };

    struct AuthConfig
    {
        bool enableSigning;
        std::string keyFingerprint;
        std::string callsign;
    };

    void setState_(State newState);
    AuthConfig loadConfig_() const;
    void validateConfiguredKey_();

    bool signFrame_(uint8_t* frame, size_t frameCapacity, size_t* frameLenOut, const std::string& fingerprint);
    VerifyResult verifyFrame_(const uint8_t* frame, size_t frameLen, const uint8_t* hashOverride);
    bool lookupPublicKey_(const std::string& callsign, gpgme_ctx_t ctx, gpgme_key_t* outKey);
    bool lookupPublicKeyFromKeyserver_(const std::string& callsign, gpgme_ctx_t ctx, gpgme_key_t* outKey);
    bool fetchPublicKeyForSender_(const std::string& callsign, gpgme_ctx_t ctx, const char* signatureFpr);

    void buildStartFrame_(uint8_t* frame, const AuthConfig& config);
    void buildEndFrame_(uint8_t* frame, const AuthConfig& config, const uint8_t* hash);
    void queueAuthFrameForTx_(const uint8_t* frame, size_t len);
    void txWorkerEntry_(uint8_t frameType, const AuthConfig& config, const uint8_t* hash);

    void sha256_(const uint8_t* data, size_t len, uint8_t* outHash) const;
    bool isAuthFrameHeader_(const uint8_t* data, size_t len, uint8_t* typeOut) const;
    static size_t parseFrameLength_(const uint8_t* data, size_t len);
    static bool hasSigningSubkey_(gpgme_key_t key);

    struct freedv* dv_;
    StateChangeFn stateChangeFn_;

    std::atomic<State> state_;

    mutable std::mutex gpgMutex_;
    gpgme_ctx_t gpgCtx_;

    mutable std::mutex txQueueMutex_;
    std::vector<std::vector<uint8_t>> txPayloadQueue_;

    mutable std::mutex rxMutex_;
    bool rxStartSeen_;
    uint8_t rxAccumHash_[AUTH_HASH_FIELD_SIZE];
    uint8_t rxStartFrame_[AUTH_FRAME_MAX];
    size_t rxStartFrameLen_;
    std::string rxSenderCallsign_;
    std::vector<uint8_t> rxFrameBuffer_;
    bool rxAuthSeenThisSync_;

    mutable std::mutex txHashMutex_;
    uint8_t txAccumHash_[AUTH_HASH_FIELD_SIZE];

    std::unique_ptr<std::thread> txWorker_;
};

#endif // ENABLE_GNUPG_AUTH

#endif // AUDIO_PIPELINE__FREEDV_AUTH_STEP_H
