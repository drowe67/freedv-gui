//=========================================================================
// Name:            FreeDVInterfaceTest.cpp
// Purpose:         Unit tests for FreeDVInterface: the modes it reports,
//                  the transmit and receive pipelines it builds for the
//                  legacy modes and for RADE, and the text that travels
//                  with the voice.
//=========================================================================

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "codec2.h"
#include "codec2_alloc.h"
#include "freedv_api.h"

#include "../../defines.h"
#include "../../freedv_interface.h"
#include "pipeline/IPipelineStep.h"
#include "UtilStubs.h"
#include "../../test/UnitTestCommon.h"

// Defined by main.cpp in the application.
std::string utTxFeatureFile;
std::string utRxFeatureFile;
wxString utFreeDVMode;
std::atomic<bool> g_bwExpandEnabled(false);

namespace {

constexpr int MODEM_SAMPLE_RATE = 8000;
constexpr int LEGACY_SPEECH_SAMPLE_RATE = 8000;
constexpr int RADE_SPEECH_RATE = 16000;

template<typename T>
bool check(const char* description, T actual, T expected)
{
    if (actual == expected) return true;
    std::cout << "[" << description << " was " << actual << ", expected " << expected << "] ";
    return false;
}

bool checkText(const char* description, std::string const& actual, std::string const& expected)
{
    if (actual == expected) return true;
    std::cout << "[" << description << " was \"" << actual << "\", expected \"" << expected << "\"] ";
    return false;
}

bool checkTrue(const char* description, bool condition)
{
    if (condition) return true;
    std::cout << "[" << description << "] ";
    return false;
}

// What the receive pipeline reads its settings from and reports sync to.
std::atomic<int> g_rxState(0);
float g_sigPwrAvg = 0.0f;

// A started FreeDVInterface that's stopped again once the test case is done.
class Interface
{
public:
    Interface(std::initializer_list<int> modes, int txMode, bool usingReliableText = false)
    {
        g_rxState = 0;
        g_sigPwrAvg = 0.0f;

        for (auto mode : modes)
        {
            dv.addRxMode(mode);
        }
        dv.start(txMode, 100, true, usingReliableText);
    }

    ~Interface()
    {
        dv.stop();
    }

    std::unique_ptr<IPipelineStep> transmitPipeline(int speechSampleRate = LEGACY_SPEECH_SAMPLE_RATE)
    {
        return std::unique_ptr<IPipelineStep>(dv.createTransmitPipeline(
            speechSampleRate, MODEM_SAMPLE_RATE,
            +[]() FREEDV_NONBLOCKING { return 0.0f; },
            nullptr));
    }

    std::unique_ptr<IPipelineStep> receivePipeline(int speechSampleRate = LEGACY_SPEECH_SAMPLE_RATE)
    {
        return std::unique_ptr<IPipelineStep>(dv.createReceivePipeline(
            MODEM_SAMPLE_RATE, speechSampleRate,
            +[]() FREEDV_NONBLOCKING { return &g_rxState; },
            +[]() FREEDV_NONBLOCKING { return 0; },
            +[]() FREEDV_NONBLOCKING { return 0; },
            +[]() FREEDV_NONBLOCKING { return 0.0f; },
            +[]() FREEDV_NONBLOCKING { return &g_sigPwrAvg; },
            nullptr));
    }

    FreeDVInterface dv;
};

std::vector<short> tone(int sampleRate, double seconds, double freqHz = 1000.0)
{
    std::vector<short> samples((size_t)(sampleRate * seconds));
    for (size_t i = 0; i < samples.size(); i++)
    {
        samples[i] = (short)(3000.0 * std::sin(2.0 * M_PI * freqHz * i / sampleRate));
    }
    return samples;
}

// Runs the given function the way the application runs its pipelines: on a
// thread of its own that has Codec2 take the memory it needs while encoding
// and decoding from a pool set aside in advance.
template<typename Fn>
void onAudioThread(Fn fn)
{
    std::thread thread([&]() {
        codec2_initialize_realtime(CODEC2_REAL_TIME_MEMORY_SIZE);
        fn();
        codec2_disable_realtime();
    });
    thread.join();
}

// Feeds samples to a pipeline 20 ms at a time, as TxRxThread does, and
// returns everything that came out of it.
template<typename AfterChunkFn>
std::vector<short> feed(IPipelineStep& step, std::vector<short> const& samples, AfterChunkFn afterChunk)
{
    const size_t chunkSize = step.getInputSampleRate() / 50;
    std::vector<short> output;
    std::vector<short> chunk(chunkSize);

    onAudioThread([&]() {
        for (size_t offset = 0; offset + chunkSize <= samples.size(); offset += chunkSize)
        {
            memcpy(chunk.data(), &samples[offset], chunkSize * sizeof(short));

            int numOutput = 0;
            short* produced = step.execute(chunk.data(), chunkSize, &numOutput);
            if (produced != nullptr && numOutput > 0)
            {
                output.insert(output.end(), produced, produced + numOutput);
            }
            afterChunk();
        }
    });

    return output;
}

std::vector<short> feed(IPipelineStep& step, std::vector<short> const& samples)
{
    return feed(step, samples, []() {});
}

// A tone with noise on top, so that what's encoded varies from frame to
// frame as speech does. (A steady tone gives a 1600 signal regular enough
// for Codec2's 700D and 700E demodulators to keep reporting sync to it.)
std::vector<short> noisyTone(int sampleRate, double seconds)
{
    auto samples = tone(sampleRate, seconds);
    unsigned int seed = 1;
    for (auto& sample : samples)
    {
        seed = seed * 1103515245u + 12345u;
        sample += (short)((int)((seed >> 16) % 4000) - 2000);
    }
    return samples;
}

// What a station using the given mode would put on the air, made with
// Codec2 directly rather than with the code under test.
std::vector<short> signalFromOtherStation(int mode, double seconds)
{
    struct freedv* dv = freedv_open(mode);
    int nSpeech = freedv_get_n_speech_samples(dv);
    int nModem = freedv_get_n_nom_modem_samples(dv);

    auto speech = noisyTone(freedv_get_speech_sample_rate(dv), seconds);
    std::vector<short> frame(nModem);
    std::vector<short> modemSamples;

    for (size_t offset = 0; offset + nSpeech <= speech.size(); offset += nSpeech)
    {
        freedv_tx(dv, frame.data(), &speech[offset]);
        modemSamples.insert(modemSamples.end(), frame.begin(), frame.end());
    }

    freedv_close(dv);
    return modemSamples;
}

// Sends a tone through the interface's own transmit pipeline and back into
// its receive pipeline. Returns whether the receiver reported sync at the end.
bool transmitToSelf(Interface& iface, double seconds)
{
    auto tx = iface.transmitPipeline();
    auto rx = iface.receivePipeline();

    auto onAir = feed(*tx, tone(LEGACY_SPEECH_SAMPLE_RATE, seconds));
    feed(*rx, onAir);

    return iface.dv.getSync() != 0;
}

std::string takeReliableText(FreeDVInterface& dv)
{
    const char* text = dv.getReliableText();
    std::string result = text;
    delete[] text;
    return result;
}

// Text typed by the operator, sent a character at a time alongside the voice.
const char* g_textToSend = "CQ TEST\r";
size_t g_textToSendIndex = 0;

// Text arrives while audio is being processed, so room for it is set aside beforehand.
char g_textReceived[1024];
size_t g_textReceivedLength = 0;

char nextCharToSend(void*)
{
    char result = g_textToSend[g_textToSendIndex];
    g_textToSendIndex = (g_textToSendIndex + 1) % strlen(g_textToSend);
    return result;
}

void charReceived(void*, char c)
{
    if (g_textReceivedLength < sizeof(g_textReceived) - 1)
    {
        g_textReceived[g_textReceivedLength++] = c;
    }
}

bool notRunningUntilStarted()
{
    FreeDVInterface dv;

    bool result = true;
    result &= check("running before start", (int)dv.isRunning(), 0);

    dv.addRxMode(FREEDV_MODE_700D);
    dv.start(FREEDV_MODE_700D, 100, true, false);
    result &= check("running after start", (int)dv.isRunning(), 1);
    result &= check("700D active", (int)dv.isModeActive(FREEDV_MODE_700D), 1);
    result &= check("700E active", (int)dv.isModeActive(FREEDV_MODE_700E), 0);
    result &= check("transmit mode", dv.getTxMode(), (int)FREEDV_MODE_700D);

    dv.stop();
    result &= check("running after stop", (int)dv.isRunning(), 0);
    result &= check("700D active after stop", (int)dv.isModeActive(FREEDV_MODE_700D), 0);
    return result;
}

bool reportsNameOfEachMode()
{
    bool result = true;

    {
        Interface iface({ FREEDV_MODE_700D }, FREEDV_MODE_700D);
        result &= checkText("700D receive mode name", iface.dv.getCurrentModeStr(), "700D");
        result &= checkText("700D transmit mode name", iface.dv.getCurrentTxModeStr(), "700D");
    }
    {
        Interface iface({ FREEDV_MODE_700E }, FREEDV_MODE_700E);
        result &= checkText("700E receive mode name", iface.dv.getCurrentModeStr(), "700E");
        result &= checkText("700E transmit mode name", iface.dv.getCurrentTxModeStr(), "700E");
    }
    {
        Interface iface({ FREEDV_MODE_1600 }, FREEDV_MODE_1600);
        result &= checkText("1600 receive mode name", iface.dv.getCurrentModeStr(), "1600");
        result &= checkText("1600 transmit mode name", iface.dv.getCurrentTxModeStr(), "1600");
    }

    return result;
}

bool transmitSizesFollowTransmitMode()
{
    Interface iface({ FREEDV_MODE_700D, FREEDV_MODE_1600 }, FREEDV_MODE_700D);

    struct freedv* ref700D = freedv_open(FREEDV_MODE_700D);
    struct freedv* ref1600 = freedv_open(FREEDV_MODE_1600);

    bool result = true;
    result &= check("700D speech samples per frame", iface.dv.getTxNumSpeechSamples(), freedv_get_n_speech_samples(ref700D));
    result &= check("700D modem samples per frame", iface.dv.getTxNNomModemSamples(), freedv_get_n_nom_modem_samples(ref700D));
    result &= check("700D modem sample rate", iface.dv.getTxModemSampleRate(), freedv_get_modem_sample_rate(ref700D));
    result &= check("700D speech sample rate", iface.dv.getTxSpeechSampleRate(), freedv_get_speech_sample_rate(ref700D));

    iface.dv.changeTxMode(FREEDV_MODE_1600);
    result &= check("transmit mode after change", iface.dv.getTxMode(), (int)FREEDV_MODE_1600);
    result &= checkText("transmit mode name after change", iface.dv.getCurrentTxModeStr(), "1600");
    result &= check("1600 speech samples per frame", iface.dv.getTxNumSpeechSamples(), freedv_get_n_speech_samples(ref1600));
    result &= check("1600 modem samples per frame", iface.dv.getTxNNomModemSamples(), freedv_get_n_nom_modem_samples(ref1600));

    // Receiving has to have room for whichever mode needs the most.
    result &= check("receive modem samples", iface.dv.getRxNumModemSamples(),
        std::max(freedv_get_n_max_modem_samples(ref700D), freedv_get_n_max_modem_samples(ref1600)));
    result &= check("receive speech samples", iface.dv.getRxNumSpeechSamples(),
        std::max(freedv_get_n_speech_samples(ref700D), freedv_get_n_speech_samples(ref1600)));
    result &= check("receive modem sample rate", iface.dv.getRxModemSampleRate(), MODEM_SAMPLE_RATE);
    result &= check("receive speech sample rate", iface.dv.getRxSpeechSampleRate(), LEGACY_SPEECH_SAMPLE_RATE);

    freedv_close(ref700D);
    freedv_close(ref1600);
    return result;
}

bool changingTransmitModeChangesWhatGoesOnAir()
{
    Interface iface({ FREEDV_MODE_700D, FREEDV_MODE_1600 }, FREEDV_MODE_700D);
    auto tx = iface.transmitPipeline();
    auto speech = tone(LEGACY_SPEECH_SAMPLE_RATE, 2.0);

    // Only a 1600 receiver can make sense of what's sent after the change.
    feed(*tx, speech);
    iface.dv.changeTxMode(FREEDV_MODE_1600);
    auto onAir = feed(*tx, speech);

    struct freedv* receiver = freedv_open(FREEDV_MODE_1600);
    std::vector<short> decoded(freedv_get_n_speech_samples(receiver));
    size_t offset = 0;
    bool synced = false;
    while (offset + freedv_nin(receiver) <= onAir.size())
    {
        int nin = freedv_nin(receiver);
        freedv_rx(receiver, decoded.data(), &onAir[offset]);
        offset += nin;
        synced |= freedv_get_sync(receiver) != 0;
    }
    freedv_close(receiver);

    return checkTrue("a 1600 receiver never synced to what was sent", synced);
}

bool receivesItsOwnTransmission()
{
    Interface iface({ FREEDV_MODE_1600 }, FREEDV_MODE_1600);

    bool result = true;
    result &= check("sync before receiving anything", iface.dv.getSync(), 0);
    result &= checkTrue("did not sync to its own signal", transmitToSelf(iface, 2.0));
    result &= check("sync reported to pipeline's owner", g_rxState.load(), 1);
    result &= checkTrue("SNR of a clean signal was not above 10 dB", iface.dv.getSNREstimate() > 10.0f);
    result &= checkTrue("frequency offset of a signal with none was 5 Hz or more", std::fabs(iface.dv.getCurrentRxModemOffset()) < 5.0f);
    result &= checkTrue("variance was negative or not a number", iface.dv.getVariance() >= 0.0f);

    iface.dv.setSync(FREEDV_SYNC_UNSYNC);
    return result;
}

bool countsBitsAndErrorsOfTestFrames()
{
    Interface iface({ FREEDV_MODE_1600 }, FREEDV_MODE_1600);

    bool result = true;
    result &= check("using test frames by default", (int)iface.dv.usingTestFrames(), 0);

    short* pattern = nullptr;
    result &= check("size of error pattern before receiving anything", iface.dv.getErrorPattern(&pattern), 0);
    result &= checkTrue("an error pattern was returned before receiving anything", pattern == nullptr);

    iface.dv.setTestFrames(true, false);
    result &= check("using test frames once enabled", (int)iface.dv.usingTestFrames(), 1);

    result &= checkTrue("did not sync to its own signal", transmitToSelf(iface, 3.0));
    result &= checkTrue("no bits were counted", iface.dv.getTotalBits() > 0);
    result &= check("bit errors over a perfect channel", iface.dv.getTotalBitErrors(), 0);

    int patternSize = iface.dv.getErrorPattern(&pattern);
    result &= checkTrue("no error pattern was available", patternSize > 0 && pattern != nullptr);
    if (pattern != nullptr)
    {
        int errors = 0;
        for (int index = 0; index < patternSize; index++)
        {
            errors += pattern[index] != 0 ? 1 : 0;
        }
        result &= check("errors in error pattern", errors, 0);
        delete[] pattern;
    }

    iface.dv.resetBitStats();
    result &= check("bits after reset", iface.dv.getTotalBits(), 0);
    result &= check("bit errors after reset", iface.dv.getTotalBitErrors(), 0);

    iface.dv.setTestFrames(false, false);
    result &= check("using test frames once disabled", (int)iface.dv.usingTestFrames(), 0);

    // Resetting the statistics turns test frames back on.
    iface.dv.resetTestFrameStats();
    result &= check("using test frames after resetting their statistics", (int)iface.dv.usingTestFrames(), 1);
    return result;
}

bool carriesTypedTextAlongsideVoice()
{
    Interface iface({ FREEDV_MODE_1600 }, FREEDV_MODE_1600);

    g_textToSendIndex = 0;
    memset(g_textReceived, 0, sizeof(g_textReceived));
    g_textReceivedLength = 0;
    iface.dv.setTextVaricodeNum(1);
    iface.dv.setTextCallbackFn(&charReceived, &nextCharToSend);

    bool result = true;
    result &= checkTrue("did not sync to its own signal", transmitToSelf(iface, 10.0));
    if (strstr(g_textReceived, "CQ TEST") == nullptr)
    {
        std::cout << "[received text was \"" << g_textReceived << "\", expected it to contain \"CQ TEST\"] ";
        result = false;
    }
    return result;
}

bool carriesCallsignAsReliableText()
{
    Interface iface({ FREEDV_MODE_1600 }, FREEDV_MODE_1600, true);

    bool result = true;
    result &= checkText("callsign before receiving anything", takeReliableText(iface.dv), "");

    iface.dv.setReliableText("K6AQ");
    result &= checkTrue("did not sync to its own signal", transmitToSelf(iface, 20.0));
    result &= checkText("callsign received", takeReliableText(iface.dv), "K6AQ");

    // It's remembered until asked to be forgotten.
    result &= checkText("callsign when asked again", takeReliableText(iface.dv), "K6AQ");
    iface.dv.resetReliableText();
    result &= checkText("callsign after reset", takeReliableText(iface.dv), "");
    return result;
}

bool switchesToModeOfReceivedSignal()
{
    Interface iface({ FREEDV_MODE_700D, FREEDV_MODE_700E, FREEDV_MODE_1600 }, FREEDV_MODE_700D);
    auto rx = iface.receivePipeline();

    bool result = true;
    result &= checkText("receive mode before any signal", iface.dv.getCurrentModeStr(), "700D");

    feed(*rx, signalFromOtherStation(FREEDV_MODE_700E, 4.0));
    result &= check("sync", iface.dv.getSync(), 1);
    result &= check("sync reported to pipeline's owner", g_rxState.load(), 1);
    result &= checkText("receive mode", iface.dv.getCurrentModeStr(), "700E");
    result &= check("receive mode number", iface.dv.getCurrentMode(), (int)FREEDV_MODE_700E);

    // What's transmitted is unaffected by what's being received.
    result &= checkText("transmit mode", iface.dv.getCurrentTxModeStr(), "700D");

    // A different station then comes up using another mode.
    iface.dv.setSync(FREEDV_SYNC_UNSYNC);
    feed(*rx, std::vector<short>(MODEM_SAMPLE_RATE, 0));
    feed(*rx, signalFromOtherStation(FREEDV_MODE_1600, 4.0));
    result &= check("sync on second signal", iface.dv.getSync(), 1);
    result &= checkText("receive mode for second signal", iface.dv.getCurrentModeStr(), "1600");
    return result;
}

bool staysOnTransmitModeWhenNothingIsReceived()
{
    Interface iface({ FREEDV_MODE_700D, FREEDV_MODE_700E, FREEDV_MODE_1600 }, FREEDV_MODE_700E);
    auto rx = iface.receivePipeline();

    feed(*rx, std::vector<short>(MODEM_SAMPLE_RATE * 2, 0));

    bool result = true;
    result &= check("sync", iface.dv.getSync(), 0);
    result &= check("sync reported to pipeline's owner", g_rxState.load(), 0);
    result &= checkText("receive mode", iface.dv.getCurrentModeStr(), "700E");
    return result;
}

bool doesNotSwitchToSignalBelowSquelch()
{
    Interface iface({ FREEDV_MODE_700D, FREEDV_MODE_700E, FREEDV_MODE_1600 }, FREEDV_MODE_700D);
    auto rx = iface.receivePipeline();

    // No signal is this strong.
    iface.dv.setSquelch(true, 100.0f);
    feed(*rx, signalFromOtherStation(FREEDV_MODE_700E, 4.0));

    bool result = true;
    result &= check("sync", iface.dv.getSync(), 0);
    result &= checkText("receive mode", iface.dv.getCurrentModeStr(), "700D");
    return result;
}

bool clippingAndFilteringChangeTransmittedSignal()
{
    auto speech = tone(LEGACY_SPEECH_SAMPLE_RATE, 1.0);

    std::vector<short> plain;
    std::vector<short> processed;
    {
        Interface iface({ FREEDV_MODE_700D }, FREEDV_MODE_700D);
        iface.dv.setRunTimeOptions(false, false);
        plain = feed(*iface.transmitPipeline(), speech);
    }
    {
        Interface iface({ FREEDV_MODE_700D }, FREEDV_MODE_700D);
        iface.dv.setRunTimeOptions(true, true);
        processed = feed(*iface.transmitPipeline(), speech);
    }

    bool result = true;
    result &= checkTrue("nothing was transmitted", !plain.empty());
    result &= check("samples transmitted with clipping and filtering", processed.size(), plain.size());
    result &= checkTrue("clipping and filtering made no difference", plain != processed);
    return result;
}

bool remainingSettingsDoNotDisturbReception()
{
    // These are passed straight on to Codec2, which offers no way of reading
    // them back; what can be checked is that they're accepted for every mode
    // in use and that a signal is still received afterwards.
    Interface iface({ FREEDV_MODE_700D, FREEDV_MODE_700E, FREEDV_MODE_1600 }, FREEDV_MODE_700D);

    iface.dv.setEq(1);
    iface.dv.setVerbose(true);
    iface.dv.setVerbose(false);
    iface.dv.setCarrierAmplitude(0, 1.0f);
    iface.dv.setLpcPostFilter(1, 0, 0.8f, 0.2f);
    iface.dv.setTextVaricodeNum(1);

    return checkTrue("did not sync to its own signal", transmitToSelf(iface, 4.0));
}

bool radeReportsItsOwnNamesAndSizes()
{
    Interface iface({ FREEDV_MODE_RADE }, FREEDV_MODE_RADE);

    bool result = true;
    result &= check("running", (int)iface.dv.isRunning(), 1);
    result &= checkText("receive mode name", iface.dv.getCurrentModeStr(), "RADEV1");
    result &= checkText("transmit mode name", iface.dv.getCurrentTxModeStr(), "RADEV1");
    result &= check("transmit mode", iface.dv.getTxMode(), (int)FREEDV_MODE_RADE);
    result &= check("receive mode", iface.dv.getCurrentMode(), (int)FREEDV_MODE_RADE);

    result &= check("transmit modem sample rate", iface.dv.getTxModemSampleRate(), MODEM_SAMPLE_RATE);
    result &= check("transmit speech sample rate", iface.dv.getTxSpeechSampleRate(), RADE_SPEECH_RATE);
    result &= check("receive modem sample rate", iface.dv.getRxModemSampleRate(), MODEM_SAMPLE_RATE);
    result &= check("receive speech sample rate", iface.dv.getRxSpeechSampleRate(), RADE_SPEECH_RATE);
    result &= checkTrue("no transmit speech samples per frame", iface.dv.getTxNumSpeechSamples() > 0);
    result &= check("receive speech samples per frame", iface.dv.getRxNumSpeechSamples(), iface.dv.getTxNumSpeechSamples());
    result &= checkTrue("no receive modem samples per frame", iface.dv.getRxNumModemSamples() > 0);

    // Room is needed for whichever is longer of a frame of voice and the end of over.
    int withoutPipeline = iface.dv.getTxNNomModemSamples();
    result &= checkTrue("no transmit modem samples per frame", withoutPipeline > 0);
    {
        auto tx = iface.transmitPipeline(RADE_SPEECH_RATE);
        result &= checkTrue("transmit modem samples per frame shrank once transmitting", iface.dv.getTxNNomModemSamples() >= withoutPipeline);
    }

    // RADE has no bit error statistics; fixed values keep the display sensible.
    short* pattern = nullptr;
    result &= check("total bits", iface.dv.getTotalBits(), 1);
    result &= check("total bit errors", iface.dv.getTotalBitErrors(), 0);
    result &= check("variance", iface.dv.getVariance(), 0.0f);
    result &= check("size of error pattern", iface.dv.getErrorPattern(&pattern), 0);
    result &= check("SNR without sync", iface.dv.getSNREstimate(), 0.0f);

    // Neither of these applies to RADE, but both are used regardless of mode.
    iface.dv.setSync(FREEDV_SYNC_UNSYNC);
    iface.dv.changeTxMode(FREEDV_MODE_RADE);
    result &= check("transmit mode after change", iface.dv.getTxMode(), (int)FREEDV_MODE_RADE);
    return result;
}

bool radeReceivesItsOwnTransmissionAndCallsign()
{
    Interface iface({ FREEDV_MODE_RADE }, FREEDV_MODE_RADE, true);
    iface.dv.setReliableText("K6AQ");

    auto tx = iface.transmitPipeline(RADE_SPEECH_RATE);
    auto rx = iface.receivePipeline(RADE_SPEECH_RATE);

    auto onAir = feed(*tx, tone(RADE_SPEECH_RATE, 4.0));

    // The callsign is sent in the end of over, which is handed back when
    // the pipeline is next asked for output without being given any input.
    iface.dv.restartTxVocoder();
    short nothing[1] = { 0 };
    int numEndOfOver = 0;
    onAudioThread([&]() {
        short* endOfOver = tx->execute(nothing, 0, &numEndOfOver);
        if (endOfOver != nullptr && numEndOfOver > 0)
        {
            onAir.insert(onAir.end(), endOfOver, endOfOver + numEndOfOver);
        }
    });

    bool result = true;
    result &= checkTrue("no end of over was produced", numEndOfOver > 0);
    onAir.insert(onAir.end(), MODEM_SAMPLE_RATE, 0);

    bool everSynced = false;
    float bestSnr = -100.0f;
    float offsetWhileSynced = 1000.0f;
    auto heard = feed(*rx, onAir, [&]() {
        if (iface.dv.getSync() != 0)
        {
            everSynced = true;
            bestSnr = std::max(bestSnr, iface.dv.getSNREstimate());
            offsetWhileSynced = iface.dv.getCurrentRxModemOffset();
        }
    });

    result &= checkTrue("never synced to its own signal", everSynced);
    result &= checkTrue("SNR of a clean signal was not above 10 dB", bestSnr > 10.0f);
    result &= checkTrue("frequency offset of a signal with none was 5 Hz or more", std::fabs(offsetWhileSynced) < 5.0f);
    result &= checkTrue("no speech was produced", !heard.empty());
    result &= check("sync once the over has ended", iface.dv.getSync(), 0);
    result &= check("sync reported to pipeline's owner", g_rxState.load(), 0);

    // Decoding the callsign may finish slightly after the audio has.
    std::string callsign;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while ((callsign = takeReliableText(iface.dv)) == "" && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    result &= checkText("callsign received", callsign, "K6AQ");
    return result;
}

} // namespace

int main()
{
    executeTestCase("notRunningUntilStarted", notRunningUntilStarted);
    executeTestCase("reportsNameOfEachMode", reportsNameOfEachMode);
    executeTestCase("transmitSizesFollowTransmitMode", transmitSizesFollowTransmitMode);
    executeTestCase("changingTransmitModeChangesWhatGoesOnAir", changingTransmitModeChangesWhatGoesOnAir);
    executeTestCase("receivesItsOwnTransmission", receivesItsOwnTransmission);
    executeTestCase("countsBitsAndErrorsOfTestFrames", countsBitsAndErrorsOfTestFrames);
    executeTestCase("carriesTypedTextAlongsideVoice", carriesTypedTextAlongsideVoice);
    executeTestCase("carriesCallsignAsReliableText", carriesCallsignAsReliableText);
    executeTestCase("switchesToModeOfReceivedSignal", switchesToModeOfReceivedSignal);
    executeTestCase("staysOnTransmitModeWhenNothingIsReceived", staysOnTransmitModeWhenNothingIsReceived);
    executeTestCase("doesNotSwitchToSignalBelowSquelch", doesNotSwitchToSignalBelowSquelch);
    executeTestCase("clippingAndFilteringChangeTransmittedSignal", clippingAndFilteringChangeTransmittedSignal);
    executeTestCase("remainingSettingsDoNotDisturbReception", remainingSettingsDoNotDisturbReception);
    executeTestCase("radeReportsItsOwnNamesAndSizes", radeReportsItsOwnNamesAndSizes);
    executeTestCase("radeReceivesItsOwnTransmissionAndCallsign", radeReceivesItsOwnTransmissionAndCallsign);

    return testResult();
}
