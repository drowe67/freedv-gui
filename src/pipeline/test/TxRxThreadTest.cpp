//=========================================================================
// Name:            TxRxThreadTest.cpp
// Purpose:         Unit tests for TxRxThread's processing loop and the UI
//                  hooks injected through TxRxThreadUiHooks.
//
// The real-time thread itself is only exercised for its start/stop
// lifecycle; the pipeline stages are driven synchronously through the
// runOnce_/txProcessing_/rxProcessing_/clearFifos_ seams (TxRxThreadTest
// is a friend of TxRxThread).
//=========================================================================

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// First, as it brings in wxWidgets, which on Windows has to include
// winsock2.h before anything else includes windows.h.
#include "UtilStubs.h"

#include <sndfile.h>

#include "../TxRxThread.h"
#include "pipeline/LinkStep.h"
#include "../../freedv_interface.h"
#include "../../test/UnitTestCommon.h"

// ---------------------------------------------------------------------------
// Globals normally defined by main.cpp. TxRxThread.cpp and the pipeline
// steps it pulls in read these; the test cases set them per case.
// UtilStubs.h already provides g_nSoundCards, g_RxFreqOffsetHz,
// g_TxFreqOffsetHz and the MainApp/MainFrame stubs.
// ---------------------------------------------------------------------------
wxMutex g_mutexProtectingCallbackData;

FreeDVInterface freedvInterface;

std::string utTxFeatureFile;
std::string utRxFeatureFile;
wxString utFreeDVMode;
std::mutex g_mutexProtectingPlayFiles;

paCallBackData* g_rxUserdata = nullptr;

std::atomic<bool>   g_half_duplex(true);
std::atomic<bool>   g_tx(false);
std::atomic<bool>   endingTx(false);
std::atomic<bool>   g_voice_keyer_tx(false);
std::atomic<bool>   g_agcEnabled(false);
std::atomic<bool>   g_noiseReductionEnabled(false);
std::atomic<bool>   g_monitorTxAudio(false);
std::atomic<bool>   g_monitorVoiceKeyerAudio(false);
std::atomic<bool>   g_totBeepActive(false);
std::atomic<bool>   g_eoo_enqueued(false);
std::atomic<bool>   g_recFileFromModulator(false);
std::atomic<bool>   g_recFileFromMic(false);
std::atomic<bool>   g_recVoiceKeyerFile(false);
std::atomic<bool>   g_playFileToMicIn(false);
std::atomic<bool>   g_loopPlayFileToMicIn(false);
std::atomic<bool>   g_playFileFromRadio(false);
std::atomic<bool>   g_loopPlayFileFromRadio(false);
std::atomic<bool>   g_bwExpandEnabled(false);

std::atomic<int>    g_analog(0);
std::atomic<int>    g_State(0);
std::atomic<int>    g_channel_noise(0);
std::atomic<int>    g_noiseSNR(0);
std::atomic<int>    g_sfFs(8000);
std::atomic<int>    g_sfTxFs(8000);
std::atomic<int64_t> g_rxMutedUntilNs(0);
std::atomic<float>  g_txLevelScale(1.0f);
std::atomic<float>  g_monitorTxAudioVol(0.0f);
std::atomic<float>  g_monitorVoiceKeyerAudioVol(0.0f);
std::atomic<float>  g_tone_phase(0.0f);

int g_txLevel = 0;
int g_dump_timing = 0;
int g_dump_fifo_state = 0;
int g_resyncs = 0;

int   g_SquelchActive = 0;
float g_SquelchLevel = 0.0f;
float g_sig_pwr_av = 0.0f;

GenericFIFO<float> g_avmag(2000);
GenericFIFO<short> g_plotSpeechInFifoBeforeEQ(8000);
GenericFIFO<short> g_plotSpeechInFifoAfterAGC(8000);
GenericFIFO<short> g_plotDemodInFifo(8000);
GenericFIFO<short> g_plotSpeechOutFifo(8000);

std::atomic<SNDFILE*> g_sfPlayFile(nullptr);
std::atomic<SNDFILE*> g_sfPlayFileFromRadio(nullptr);
std::atomic<SNDFILE*> g_sfRecFileFromModulator(nullptr);
std::atomic<SNDFILE*> g_sfRecMicFile(nullptr);
std::atomic<SNDFILE*> g_sfRecFile(nullptr);
std::atomic<SNDFILE*> g_sfRecDecoderFile(nullptr);
std::atomic<bool> g_recFileFromRadio(false);
std::atomic<bool> g_recFileFromDecoder(false);
std::atomic<unsigned int> g_recFromRadioSamples(0);

namespace {

constexpr int TEST_SAMPLE_RATE = 8000;
constexpr int FIFO_CAPACITY = 16000;

// What the tone hooks given to the thread under test report.
std::atomic<float> g_testToneFreq(1000.0f);
std::atomic<float> g_testToneAmplitude(0.0f);
std::atomic<bool> g_testToneEnabled(false);

// Prints a bracketed mismatch on failure, like the other pipeline tests' helper.
template<typename T>
bool check(const char* description, T actual, T expected)
{
    if (actual == expected) return true;
    std::cout << "[" << description << " was " << actual << ", expected " << expected << "] ";
    return false;
}

void resetGlobals()
{
    g_nSoundCards = 2;
    g_half_duplex = true;
    g_tx = false;
    endingTx = false;
    g_voice_keyer_tx = false;
    g_agcEnabled = false;
    g_noiseReductionEnabled = false;
    g_monitorTxAudio = false;
    g_monitorVoiceKeyerAudio = false;
    g_monitorTxAudioVol = 0.0f;
    g_monitorVoiceKeyerAudioVol = 0.0f;
    g_totBeepActive = false;
    g_eoo_enqueued = false;
    g_analog = 0;
    g_State = 0;
    g_channel_noise = 0;
    g_noiseSNR = 0;
    g_rxMutedUntilNs = 0;
    g_SquelchActive = 0;
    g_SquelchLevel = 0.0f;
    g_txLevelScale = 1.0f;
    g_playFileToMicIn = false;
    g_loopPlayFileToMicIn = false;
    g_playFileFromRadio = false;
    g_loopPlayFileFromRadio = false;
    g_recFileFromMic = false;
    g_recVoiceKeyerFile = false;
    g_recFileFromModulator = false;
    g_recFileFromRadio = false;
    g_sfPlayFile = nullptr;
    g_recFileFromDecoder = false;
    g_sfPlayFileFromRadio = nullptr;
    g_sfRecFileFromModulator = nullptr;
    g_sfRecMicFile = nullptr;
    g_sfFs = TEST_SAMPLE_RATE;
    g_sfTxFs = TEST_SAMPLE_RATE;
    g_resyncs = 0;
    g_sig_pwr_av = 0.0f;
    g_sfRecFile = nullptr;
    g_recFromRadioSamples = 0;
    g_dump_fifo_state = 0;
    g_sfRecDecoderFile = nullptr;
    g_RxFreqOffsetHz = 0.0f;
    g_TxFreqOffsetHz = 0.0f;
    g_tone_phase = 0.0f;
    g_testToneFreq = 1000.0f;
    g_testToneAmplitude = 0.0f;
    g_testToneEnabled = false;
}

// Owns a fresh set of sound-card FIFOs for the duration of a test case.
class FifoGuard
{
public:
    FifoGuard()
    {
        g_rxUserdata = new paCallBackData();
        g_rxUserdata->infifo1 = new GenericFIFO<short>(FIFO_CAPACITY);
        g_rxUserdata->outfifo1 = new GenericFIFO<short>(FIFO_CAPACITY);
        g_rxUserdata->infifo2 = new GenericFIFO<short>(FIFO_CAPACITY);
        g_rxUserdata->outfifo2 = new GenericFIFO<short>(FIFO_CAPACITY);
    }

    ~FifoGuard()
    {
        delete g_rxUserdata->infifo1;
        delete g_rxUserdata->outfifo1;
        delete g_rxUserdata->infifo2;
        delete g_rxUserdata->outfifo2;
        delete g_rxUserdata;
        g_rxUserdata = nullptr;
    }

    FifoGuard(const FifoGuard&) = delete;
    FifoGuard& operator=(const FifoGuard&) = delete;
};

// IRealtimeHelper that just records what the thread asked it to do.
class FakeRealtimeHelper : public IRealtimeHelper
{
public:
    void setHelperRealTime() FREEDV_NONBLOCKING override { setRealTimeCalls++; }
    void startRealTimeWork() FREEDV_NONBLOCKING override { startCalls++; }
    void stopRealTimeWork(bool fastMode) FREEDV_NONBLOCKING override { stopCalls++; lastFastMode = fastMode; }
    void clearHelperRealTime() FREEDV_NONBLOCKING override { clearRealTimeCalls++; }
    bool mustStopWork() FREEDV_NONBLOCKING override
    {
        if (stopWork.load(std::memory_order_acquire)) return true;

        int allowed = framesAllowed.load(std::memory_order_acquire);
        if (allowed < 0) return false;
        if (allowed == 0) return true;
        framesAllowed.store(allowed - 1, std::memory_order_release);
        return false;
    }

    // What the thread is told when it asks whether it has run out of time.
    std::atomic<bool> stopWork{false};

    // If not negative, the number of frames the thread may still process
    // before being told that it has run out of time.
    std::atomic<int> framesAllowed{-1};

    // Atomic as the thread under test updates these while the test reads them.
    std::atomic<int> setRealTimeCalls{0};
    std::atomic<int> startCalls{0};
    std::atomic<int> stopCalls{0};
    std::atomic<int> clearRealTimeCalls{0};
    std::atomic<bool> lastFastMode{false};
};

// Tone getters: realtime_fp only accepts raw function pointers, so these are
// captureless file-scope functions (the app's equivalents live in main.cpp).
float testToneFreq() FREEDV_NONBLOCKING { return g_testToneFreq.load(std::memory_order_acquire); }
float testToneAmplitude() FREEDV_NONBLOCKING { return g_testToneAmplitude.load(std::memory_order_acquire); }
bool testToneEnabled() FREEDV_NONBLOCKING { return g_testToneEnabled.load(std::memory_order_acquire); }

TxRxThreadUiHooks makeHooks(std::atomic<int>& playFileToMicInFinished, std::atomic<int>& recFileFromRadioFinished, std::atomic<int>& playFileFromRadioFinished)
{
    TxRxThreadUiHooks hooks;
    hooks.playFileToMicInFinished = [&playFileToMicInFinished]() { playFileToMicInFinished++; };
    hooks.recFileFromRadioFinished = [&recFileFromRadioFinished]() { recFileFromRadioFinished++; };
    hooks.playFileFromRadioFinished = [&playFileFromRadioFinished]() { playFileFromRadioFinished++; };
    hooks.toneFreqHz = testToneFreq;
    hooks.toneAmplitude = testToneAmplitude;
    hooks.toneEnabled = testToneEnabled;
    return hooks;
}

void seedSine(GenericFIFO<short>* fifo, int numSamples, double freqHz = 440.0)
{
    std::vector<short> samples(numSamples);
    for (int i = 0; i < numSamples; i++)
    {
        samples[i] = (short)(8000.0 * std::sin(2.0 * M_PI * freqHz * i / TEST_SAMPLE_RATE));
    }
    fifo->write(samples.data(), numSamples);
}

std::vector<short> drain(GenericFIFO<short>* fifo)
{
    std::vector<short> out(fifo->numUsed());
    if (!out.empty())
    {
        fifo->read(out.data(), out.size());
    }
    return out;
}

int countNonZero(std::vector<short> const& samples)
{
    int count = 0;
    for (auto sample : samples)
    {
        if (sample != 0) count++;
    }
    return count;
}

int peak(std::vector<short> const& samples)
{
    int result = 0;
    for (auto sample : samples)
    {
        result = std::max(result, std::abs((int)sample));
    }
    return result;
}

bool checkBetween(const char* description, int actual, int lowest, int highest)
{
    if (actual >= lowest && actual <= highest) return true;
    std::cout << "[" << description << " was " << actual << ", expected " << lowest << " to " << highest << "] ";
    return false;
}

std::vector<short> sine(int numSamples, double freqHz = 440.0, double amplitude = 4000.0)
{
    std::vector<short> samples(numSamples);
    for (int i = 0; i < numSamples; i++)
    {
        samples[i] = (short)(amplitude * std::sin(2.0 * M_PI * freqHz * i / TEST_SAMPLE_RATE));
    }
    return samples;
}

void seedSilence(GenericFIFO<short>* fifo, int numSamples)
{
    std::vector<short> samples(numSamples, 0);
    fifo->write(samples.data(), numSamples);
}

double rms(std::vector<short> const& samples)
{
    if (samples.empty()) return 0.0;

    double sum = 0.0;
    for (auto sample : samples)
    {
        sum += (double)sample * sample;
    }
    return std::sqrt(sum / samples.size());
}

// Returns the frequency of a tone from how often it crosses zero.
double toneFrequency(std::vector<short> const& samples, int sampleRate)
{
    int crossings = 0;
    for (size_t i = 1; i < samples.size(); i++)
    {
        if ((samples[i - 1] < 0) != (samples[i] < 0)) crossings++;
    }
    return samples.empty() ? 0.0 : crossings / 2.0 * sampleRate / samples.size();
}

std::vector<short> noise(int numSamples, int amplitude)
{
    std::vector<short> samples(numSamples);
    unsigned int seed = 1;
    for (auto& sample : samples)
    {
        seed = seed * 1103515245u + 12345u;
        sample = (short)((int)((seed >> 16) % (2 * amplitude)) - amplitude);
    }
    return samples;
}

// A sound file being recorded to, removed once the test case is done.
class RecordingFile
{
public:
    explicit RecordingFile(int sampleRate)
    {
        path_ = unitTestTempPath("TxRxThreadTest", "-recording.wav");

        SF_INFO info = {};
        info.samplerate = sampleRate;
        info.channels = 1;
        info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
        file_ = sf_open(path_.c_str(), SFM_WRITE, &info);
    }

    ~RecordingFile()
    {
        close();
        remove(path_.c_str());
    }

    SNDFILE* get() const { return file_; }

    // Returns how much has been recorded so far. RecordStep holds this lock
    // while writing, so the file isn't touched while it's being asked.
    int samplesSoFar()
    {
        g_mutexProtectingCallbackData.Lock();
        int result = (int)sf_seek(file_, 0, SEEK_CUR);
        g_mutexProtectingCallbackData.Unlock();
        return result;
    }

    // Recording happens on other threads; gives them time to catch up.
    void waitForSamples(int numSamples)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (samplesSoFar() < numSamples && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    // Closes the file and returns what ended up in it.
    std::vector<short> finish()
    {
        close();

        SF_INFO info = {};
        std::vector<short> samples;
        SNDFILE* recorded = sf_open(path_.c_str(), SFM_READ, &info);
        if (recorded != nullptr)
        {
            samples.resize(info.frames);
            sf_read_short(recorded, samples.data(), samples.size());
            sf_close(recorded);
        }
        return samples;
    }

    RecordingFile(const RecordingFile&) = delete;
    RecordingFile& operator=(const RecordingFile&) = delete;

private:
    void close()
    {
        if (file_ != nullptr)
        {
            sf_close(file_);
            file_ = nullptr;
        }
    }

    std::string path_;
    SNDFILE* file_ = nullptr;
};

bool anyNonZero(std::vector<short> const& samples)
{
    for (auto sample : samples)
    {
        if (sample != 0) return true;
    }
    return false;
}

// A sound file on disk that's removed once the test case is done
// (same shape as PlaybackStepTest's helper).
class TestFile
{
public:
    TestFile(std::vector<short> const& samples, int sampleRate, int channels = 1)
    {
        path_ = unitTestTempPath("TxRxThreadTest", "-" + std::to_string(NextId_++) + ".wav");

        SF_INFO info = {};
        info.samplerate = sampleRate;
        info.channels = channels;
        info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

        SNDFILE* file = sf_open(path_.c_str(), SFM_WRITE, &info);
        if (file != nullptr)
        {
            sf_write_short(file, samples.data(), samples.size());
            sf_close(file);
        }

        info = {};
        file_ = sf_open(path_.c_str(), SFM_READ, &info);
    }

    ~TestFile()
    {
        if (file_ != nullptr)
        {
            sf_close(file_);
            file_ = nullptr;
        }
        remove(path_.c_str());
    }

    SNDFILE* get() const { return file_; }

    TestFile(const TestFile&) = delete;
    TestFile& operator=(const TestFile&) = delete;

private:
    std::string path_;
    SNDFILE* file_ = nullptr;
    static int NextId_;
};

int TestFile::NextId_ = 0;

} // namespace

// Friend of TxRxThread: drives the private processing seams directly.
class TxRxThreadTest
{
public:
    static bool txProcessingSkippedWhenHalfDuplexPttOff()
    {
        resetGlobals(); // half duplex, PTT off
        FifoGuard fifos;
        seedSine(g_rxUserdata->infifo2, 1600);

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));

        FakeRealtimeHelper helper;
        thread.txProcessing_(&helper);

        bool result = true;
        result &= check("outfifo1 used", g_rxUserdata->outfifo1->numUsed(), 0);
        result &= check("infifo2 left untouched", g_rxUserdata->infifo2->numUsed(), 1600);
        result &= check("deferReset_ set for next TX entry", (int)thread.deferReset_, 1);
        return result;
    }

    static bool txProcessingGeneratesModemOutputWhenPttOn()
    {
        resetGlobals();
        g_tx = true;
        FifoGuard fifos;
        seedSine(g_rxUserdata->infifo2, TEST_SAMPLE_RATE); // 1s of speech

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();

        FakeRealtimeHelper helper;
        thread.txProcessing_(&helper);

        auto modemOut = drain(g_rxUserdata->outfifo1);

        bool result = true;
        result &= check("infifo2 drained", g_rxUserdata->infifo2->numUsed(), 0);
        if (modemOut.size() < 320)
        {
            std::cout << "[outfifo1 got " << modemOut.size() << " samples, expected at least 320] ";
            result = false;
        }
        result &= !anyNonZero(modemOut) ? (std::cout << "[outfifo1 output was all zeros]" << std::endl, false) : true;
        return result;
    }

    static bool enteringTxAfterRxResetsPipelineAndSkipsOneCycle()
    {
        resetGlobals();
        g_tx = true;
        FifoGuard fifos;
        seedSine(g_rxUserdata->infifo2, TEST_SAMPLE_RATE);
        seedSine(g_rxUserdata->outfifo1, 1000); // stale RX-side data in the TX output FIFO

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();
        thread.deferReset_ = true;

        FakeRealtimeHelper helper;

        // First entry after RX: reset everything and produce nothing.
        thread.txProcessing_(&helper);
        bool result = true;
        result &= check("outfifo1 cleared on entry", g_rxUserdata->outfifo1->numUsed(), 0);
        result &= check("infifo2 cleared on entry", g_rxUserdata->infifo2->numUsed(), 0);
        result &= check("deferReset_ consumed", (int)thread.deferReset_, 0);

        seedSine(g_rxUserdata->infifo2, TEST_SAMPLE_RATE);
        // Second entry: normal TX processing resumes.
        thread.txProcessing_(&helper);
        result &= g_rxUserdata->outfifo1->numUsed() >= 320 ? true
            : (std::cout << "[outfifo1 got " << g_rxUserdata->outfifo1->numUsed() << " samples after re-entry, expected >= 320]" << std::endl, false);
        return result;
    }

    // Transmits the given number of seconds of speech-like audio and returns
    // what was sent to the radio.
    static std::vector<short> transmittedSignal(int seconds)
    {
        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();

        // A tone with noise on top, so that it varies from frame to frame as speech does.
        auto mic = sine(TEST_SAMPLE_RATE, 1000.0, 3000.0);
        auto hiss = noise(TEST_SAMPLE_RATE, 2000);
        for (size_t i = 0; i < mic.size(); i++)
        {
            mic[i] += hiss[i];
        }

        // Transmitting needs a sound card of its own.
        int numSoundCards = g_nSoundCards;
        g_nSoundCards = 2;
        bool wasTransmitting = g_tx.exchange(true);
        FakeRealtimeHelper helper;
        std::vector<short> signal;
        for (int second = 0; second < seconds; second++)
        {
            g_rxUserdata->infifo2->write(mic.data(), mic.size());
            thread.txProcessing_(&helper);

            auto sent = drain(g_rxUserdata->outfifo1);
            signal.insert(signal.end(), sent.begin(), sent.end());
        }
        g_tx = wasTransmitting;
        g_nSoundCards = numSoundCards;
        return signal;
    }

    // Has a receive thread demodulate four seconds of signal, enough for it
    // to find the signal and start decoding speech, and returns what reached
    // each of the two places the speech can be sent to.
    static void receiveSignal(std::vector<short>& toOutfifo1, std::vector<short>& toOutfifo2)
    {
        auto signal = transmittedSignal(4);

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();

        // initializePipeline_ clears the RX FIFOs, so seed after it. The
        // FIFOs only have room for two seconds, so hand it over a second
        // at a time.
        FakeRealtimeHelper helper;
        for (size_t offset = 0; offset < signal.size(); offset += TEST_SAMPLE_RATE)
        {
            g_rxUserdata->infifo1->write(&signal[offset], std::min((size_t)TEST_SAMPLE_RATE, signal.size() - offset));
            thread.rxProcessing_(&helper);

            auto out1 = drain(g_rxUserdata->outfifo1);
            auto out2 = drain(g_rxUserdata->outfifo2);
            toOutfifo1.insert(toOutfifo1.end(), out1.begin(), out1.end());
            toOutfifo2.insert(toOutfifo2.end(), out2.begin(), out2.end());
        }
    }

    static bool rxProcessingDemodulatesToOutfifo1ForSingleSoundCard()
    {
        resetGlobals();
        g_nSoundCards = 1;
        FifoGuard fifos;

        std::vector<short> toOutfifo1;
        std::vector<short> toOutfifo2;
        receiveSignal(toOutfifo1, toOutfifo2);

        bool result = true;
        result &= check("decoder in sync", g_State.load(), 1);
        if (toOutfifo1.empty())
        {
            std::cout << "[outfifo1 empty after demodulating 4s of signal] ";
            result = false;
        }
        result &= !anyNonZero(toOutfifo1) ? (std::cout << "[outfifo1 output was all zeros]" << std::endl, false) : true;
        result &= check("outfifo2 unused (single sound card)", (int)toOutfifo2.size(), 0);
        return result;
    }

    static bool rxProcessingWritesToOutfifo2ForDualSoundCards()
    {
        resetGlobals(); // g_nSoundCards = 2
        FifoGuard fifos;

        std::vector<short> toOutfifo1;
        std::vector<short> toOutfifo2;
        receiveSignal(toOutfifo1, toOutfifo2);

        bool result = true;
        result &= check("decoder in sync", g_State.load(), 1);
        result &= check("outfifo1 unused (dual sound cards)", (int)toOutfifo1.size(), 0);
        result &= !anyNonZero(toOutfifo2) ? (std::cout << "[nothing but silence in outfifo2 after demodulating 4s of signal]" << std::endl, false) : true;
        return result;
    }

    static bool runOnceRequestsFastModeWhenOutputFifoLow()
    {
        resetGlobals(); // PTT off: txProcessing_ produces nothing, so the FIFO state below is what runOnce_ sees
        FifoGuard fifos;

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));

        FakeRealtimeHelper helper;

        // Empty output FIFO -> the thread should ask to be woken sooner.
        thread.runOnce_(&helper);
        bool result = true;
        result &= check("fastMode with empty outfifo1", (int)helper.lastFastMode.load(), 1);

        // More than half full -> normal wake interval.
        std::vector<short> junk(FIFO_CAPACITY * 3 / 4, 1);
        g_rxUserdata->outfifo1->write(junk.data(), junk.size());
        thread.runOnce_(&helper);
        result &= check("no fastMode with outfifo1 mostly full", (int)helper.lastFastMode.load(), 0);

        result &= check("startRealTimeWork calls", helper.startCalls.load(), 2);
        result &= check("stopRealTimeWork calls", helper.stopCalls.load(), 2);
        return result;
    }

    static bool clearFifosResetsTxFifosAndSelectedRxFifo()
    {
        resetGlobals(); // g_nSoundCards = 2
        FifoGuard fifos;

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread txThread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        TxRxThread rxThread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));

        bool result = true;

        seedSine(g_rxUserdata->outfifo1, 1000);
        seedSine(g_rxUserdata->infifo2, 1000);
        txThread.clearFifos_();
        result &= check("TX outfifo1 cleared", g_rxUserdata->outfifo1->numUsed(), 0);
        result &= check("TX infifo2 cleared", g_rxUserdata->infifo2->numUsed(), 0);

        seedSine(g_rxUserdata->infifo1, 1000);
        seedSine(g_rxUserdata->outfifo1, 1000);
        seedSine(g_rxUserdata->outfifo2, 1000);
        rxThread.clearFifos_();
        result &= check("RX infifo1 cleared", g_rxUserdata->infifo1->numUsed(), 0);
        result &= check("RX outfifo2 cleared", g_rxUserdata->outfifo2->numUsed(), 0);
        // With two sound cards the RX output FIFO is outfifo2; outfifo1 is
        // the TX-side FIFO and must not be touched by RX cleanup.
        result &= check("TX-side outfifo1 left alone", g_rxUserdata->outfifo1->numUsed(), 1000);
        return result;
    }

    static bool playFileToMicInFinishedHookFiresWhenFileEnds()
    {
        resetGlobals();
        g_tx = true; // playing to mic-in only flows while transmitting
        g_playFileToMicIn = true;
        g_loopPlayFileToMicIn = false;
        FifoGuard fifos;

        std::vector<short> fileSamples(TEST_SAMPLE_RATE / 10); // 0.1s of tone
        for (size_t i = 0; i < fileSamples.size(); i++)
        {
            fileSamples[i] = (short)(4000.0 * std::sin(2.0 * M_PI * 440.0 * i / TEST_SAMPLE_RATE));
        }
        TestFile file(fileSamples, TEST_SAMPLE_RATE);
        if (file.get() == nullptr)
        {
            std::cout << "[could not open test sound file] ";
            return false;
        }
        g_sfPlayFile.store(file.get(), std::memory_order_release);

        seedSine(g_rxUserdata->infifo2, TEST_SAMPLE_RATE); // 1s of mic audio, longer than the file

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();

        FakeRealtimeHelper helper;
        thread.txProcessing_(&helper);

        // PlaybackStep detects EOF on its file I/O thread, which only runs
        // when execute() signals it and drains its output FIFO. Keep pumping
        // the pipeline so the buffered file samples get consumed and the
        // completion hook can fire.
        short zeros[160] = {};
        int numOut = 0;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (micInDone.load(std::memory_order_acquire) == 0 &&
               std::chrono::steady_clock::now() < deadline)
        {
            thread.pipeline_->execute(zeros, 160, &numOut);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }


        g_sfPlayFile.store(nullptr, std::memory_order_release);

        bool result = true;
        if (micInDone.load(std::memory_order_acquire) < 1)
        {
            std::cout << "[playFileToMicInFinished hook fired " << micInDone.load() << " times, expected at least 1] ";
            result = false;
        }
        return result;
    }

    // Runs one second of microphone audio through a transmit thread and
    // returns the loudest sample the receive thread then sends to the speaker
    // while the radio itself is silent.
    static int peakOfMicAudioHeardThroughSpeaker()
    {
        FifoGuard fifos;
        auto link = std::make_shared<LinkStep>(TEST_SAMPLE_RATE);

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread txThread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, link, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        TxRxThread rxThread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, link, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        txThread.initializePipeline_();
        rxThread.initializePipeline_();

        // Both initializations clear the FIFOs, so seed after them.
        seedSine(g_rxUserdata->infifo2, TEST_SAMPLE_RATE);
        seedSilence(g_rxUserdata->infifo1, TEST_SAMPLE_RATE);

        FakeRealtimeHelper helper;
        txThread.txProcessing_(&helper);
        rxThread.rxProcessing_(&helper);

        return peak(drain(g_rxUserdata->outfifo2));
    }

    static bool monitoringTransmitAudioPlaysMicThroughSpeaker()
    {
        resetGlobals();
        g_tx = true;
        g_monitorTxAudio = true;

        // seedSine() produces a tone with a peak of 8000, which mixing with
        // the (silent) time-out beep brings down to about 5600.
        return checkBetween("loudest speaker sample", peakOfMicAudioHeardThroughSpeaker(), 5400, 5800);
    }

    static bool monitoredTransmitAudioFollowsItsVolumeSetting()
    {
        resetGlobals();
        g_tx = true;
        g_monitorTxAudio = true;
        g_monitorTxAudioVol = -20.0f; // a tenth of the amplitude
        g_monitorVoiceKeyerAudioVol = 0.0f;

        return checkBetween("loudest speaker sample", peakOfMicAudioHeardThroughSpeaker(), 540, 590);
    }

    static bool monitoredVoiceKeyerAudioFollowsItsOwnVolumeSetting()
    {
        resetGlobals();
        g_voice_keyer_tx = true;
        g_monitorVoiceKeyerAudio = true;
        g_monitorVoiceKeyerAudioVol = -20.0f;
        g_monitorTxAudioVol = 0.0f;

        return checkBetween("loudest speaker sample", peakOfMicAudioHeardThroughSpeaker(), 540, 590);
    }

    static bool micAudioStaysOutOfSpeakerWhenNotMonitoring()
    {
        resetGlobals();
        g_tx = true;
        g_half_duplex = false; // so that the receiver isn't simply muted
        g_analog = 1; // so that the radio's silence reaches the speaker as is

        return check("loudest speaker sample", peakOfMicAudioHeardThroughSpeaker(), 0);
    }

    static bool speakerIsSilentWhileRecordingVoiceKeyerFile()
    {
        resetGlobals();
        g_analog = 1; // would otherwise pass the radio's audio straight through
        g_recVoiceKeyerFile = true;
        FifoGuard fifos;
        auto link = std::make_shared<LinkStep>(TEST_SAMPLE_RATE);

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, link, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();
        seedSine(g_rxUserdata->infifo1, TEST_SAMPLE_RATE);

        FakeRealtimeHelper helper;
        thread.rxProcessing_(&helper);

        auto speakerOut = drain(g_rxUserdata->outfifo2);

        bool result = true;
        result &= check("speaker samples", (int)speakerOut.size(), TEST_SAMPLE_RATE);
        result &= check("loudest speaker sample", peak(speakerOut), 0);
        return result;
    }

    static bool clearFifosAlsoClearsMonitoredMicAudio()
    {
        resetGlobals();
        FifoGuard fifos;
        auto link = std::make_shared<LinkStep>(TEST_SAMPLE_RATE);

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, link, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));

        seedSine(&link->getFifo(), 1000);
        thread.clearFifos_();

        return check("monitored mic samples waiting", link->getFifo().numUsed(), 0);
    }

    // Keeps a thread's pipeline moving, since PlaybackStep only reads more of
    // its file as its output is consumed, until enough has come out of it or
    // the condition is met.
    template<typename DoneFn>
    static std::vector<short> pumpUntil(TxRxThread& thread, DoneFn done)
    {
        std::vector<short> collected;
        short zeros[160] = {};
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!done(collected) && std::chrono::steady_clock::now() < deadline)
        {
            int numOut = 0;
            auto out = thread.pipeline_->execute(zeros, 160, &numOut);
            if (out != nullptr && numOut > 0)
            {
                collected.insert(collected.end(), out, out + numOut);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return collected;
    }

    static bool playFileToMicInStartsOverWhenLooping()
    {
        resetGlobals();
        g_tx = true;
        g_analog = 1; // so that what's played comes out of the pipeline as is
        g_playFileToMicIn = true;
        g_loopPlayFileToMicIn = true;
        g_dump_fifo_state = 1; // only adds logging
        FifoGuard fifos;

        auto fileSamples = sine(TEST_SAMPLE_RATE / 10); // 0.1s
        TestFile file(fileSamples, TEST_SAMPLE_RATE);
        if (file.get() == nullptr)
        {
            std::cout << "[could not open test sound file] ";
            return false;
        }
        g_sfPlayFile.store(file.get(), std::memory_order_release);

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();
        seedSilence(g_rxUserdata->infifo2, 1600);

        FakeRealtimeHelper helper;
        thread.txProcessing_(&helper);

        // More than twice the file's worth of sound can only come from playing it again.
        int wanted = (int)fileSamples.size() * 5 / 2;
        int heard = countNonZero(drain(g_rxUserdata->outfifo1));
        auto rest = pumpUntil(thread, [&](std::vector<short> const& out) { return heard + countNonZero(out) >= wanted; });
        heard += countNonZero(rest);

        g_playFileToMicIn = false;
        g_sfPlayFile.store(nullptr, std::memory_order_release);

        bool result = true;
        if (heard < wanted)
        {
            std::cout << "[only " << heard << " samples of sound came out, expected at least " << wanted << "] ";
            result = false;
        }
        result &= check("times playback was reported finished", micInDone.load(), 0);
        return result;
    }

    static bool playFileFromRadioFinishedHookFiresWhenFileEnds()
    {
        resetGlobals();
        g_analog = 1;
        g_playFileFromRadio = true;
        FifoGuard fifos;

        auto fileSamples = sine(TEST_SAMPLE_RATE / 10);
        TestFile file(fileSamples, TEST_SAMPLE_RATE);
        if (file.get() == nullptr)
        {
            std::cout << "[could not open test sound file] ";
            return false;
        }
        g_sfPlayFileFromRadio.store(file.get(), std::memory_order_release);

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();
        seedSilence(g_rxUserdata->infifo1, 1600);

        FakeRealtimeHelper helper;
        thread.rxProcessing_(&helper);

        int heard = countNonZero(drain(g_rxUserdata->outfifo2));
        auto rest = pumpUntil(thread, [&](std::vector<short> const&) { return playRadioDone.load() > 0; });
        heard += countNonZero(rest);

        g_playFileFromRadio = false;
        g_sfPlayFileFromRadio.store(nullptr, std::memory_order_release);

        bool result = true;
        result &= check("times playback was reported finished", playRadioDone.load() > 0 ? 1 : 0, 1);
        result &= check("times mic playback was reported finished", micInDone.load(), 0);

        // The file replaces what the radio sent (silence here), so sound
        // can only have come from the file.
        result &= checkBetween("samples of sound heard", heard, (int)fileSamples.size() * 9 / 10, (int)fileSamples.size());
        return result;
    }

    static bool playFileFromRadioStartsOverWhenLooping()
    {
        resetGlobals();
        g_analog = 1;
        g_playFileFromRadio = true;
        g_loopPlayFileFromRadio = true;
        FifoGuard fifos;

        auto fileSamples = sine(TEST_SAMPLE_RATE / 10);
        TestFile file(fileSamples, TEST_SAMPLE_RATE);
        if (file.get() == nullptr)
        {
            std::cout << "[could not open test sound file] ";
            return false;
        }
        g_sfPlayFileFromRadio.store(file.get(), std::memory_order_release);

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();
        seedSilence(g_rxUserdata->infifo1, 1600);

        FakeRealtimeHelper helper;
        thread.rxProcessing_(&helper);

        int wanted = (int)fileSamples.size() * 5 / 2;
        int heard = countNonZero(drain(g_rxUserdata->outfifo2));
        auto rest = pumpUntil(thread, [&](std::vector<short> const& out) { return heard + countNonZero(out) >= wanted; });
        heard += countNonZero(rest);

        g_playFileFromRadio = false;
        g_sfPlayFileFromRadio.store(nullptr, std::memory_order_release);

        bool result = true;
        if (heard < wanted)
        {
            std::cout << "[only " << heard << " samples of sound came out, expected at least " << wanted << "] ";
            result = false;
        }
        result &= check("times playback was reported finished", playRadioDone.load(), 0);
        return result;
    }

    static bool recordingFromRadioContinuesUntilStopped()
    {
        // Recording is done at RECORD_FILE_SAMPLE_RATE, so receive at that
        // rate to have what arrives from the radio end up in the file as is.
        constexpr int RADIO_SAMPLE_RATE = RECORD_FILE_SAMPLE_RATE;

        // What's received reaches the file by way of a buffer with room for a
        // quarter of a second, anything beyond which is discarded, and up to
        // a tenth of a second is held back in it until more arrives. Audio
        // arrives here far faster than it would from a radio, so it has to
        // be handed over a little at a time, each round only once there's
        // little more than that tenth of a second still waiting.
        constexpr int SAMPLES_PER_ROUND = RADIO_SAMPLE_RATE / 10; // 0.1s
        constexpr int ROUNDS = 6;
        constexpr int MAX_SAMPLES_WAITING = RADIO_SAMPLE_RATE * 13 / 100;
        constexpr unsigned int UNTIL_STOPPED = UINT32_MAX; // what the application asks for

        resetGlobals();
        g_analog = 1;
        FifoGuard fifos;

        std::string path = unitTestTempPath("TxRxThreadTest", "-rec.wav");
        SF_INFO info = {};
        info.samplerate = RADIO_SAMPLE_RATE;
        info.channels = 1;
        info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
        g_sfRecFile = sf_open(path.c_str(), SFM_WRITE, &info);
        if (g_sfRecFile == nullptr)
        {
            std::cout << "[could not create file to record to] ";
            return false;
        }
        g_recFromRadioSamples = UNTIL_STOPPED;
        g_recFileFromRadio = true;

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        {
            TxRxThread thread(false, RADIO_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
            thread.initializePipeline_();

            FakeRealtimeHelper helper;
            for (int round = 0; round < ROUNDS; round++)
            {
                seedSine(g_rxUserdata->infifo1, SAMPLES_PER_ROUND);
                thread.rxProcessing_(&helper);
                drain(g_rxUserdata->outfifo2);

                // Recording happens on other threads; give them time to catch up.
                int expectedSoFar = SAMPLES_PER_ROUND * (round + 1) - MAX_SAMPLES_WAITING;
                auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while ((int)(UNTIL_STOPPED - g_recFromRadioSamples.load()) < expectedSoFar &&
                       std::chrono::steady_clock::now() < deadline)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }

        g_recFileFromRadio = false;
        sf_close(g_sfRecFile);
        g_sfRecFile = nullptr;

        info = {};
        SNDFILE* recorded = sf_open(path.c_str(), SFM_READ, &info);
        int recordedSamples = recorded != nullptr ? (int)info.frames : -1;
        if (recorded != nullptr) sf_close(recorded);
        remove(path.c_str());

        bool result = true;

        // The last tenth of a second or so may still have been waiting to be recorded.
        result &= checkBetween("samples in recorded file", recordedSamples, SAMPLES_PER_ROUND * ROUNDS - MAX_SAMPLES_WAITING, SAMPLES_PER_ROUND * ROUNDS);
        result &= check("samples counted as recorded", (int)(UNTIL_STOPPED - g_recFromRadioSamples.load()), recordedSamples);
        result &= check("times recording was reported finished", recRadioDone.load(), 0);
        return result;
    }

    static bool timeOutWarningBeepsThenTurnsItselfOff()
    {
        resetGlobals();
        g_tx = true; // half duplex and transmitting: nothing else reaches the speaker
        g_totBeepActive = true;
        FifoGuard fifos;

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();
        seedSine(g_rxUserdata->infifo1, TEST_SAMPLE_RATE);

        FakeRealtimeHelper helper;
        thread.rxProcessing_(&helper);

        auto speakerOut = drain(g_rxUserdata->outfifo2);

        bool result = true;

        // Five 80 ms beeps at 8 kHz are 3200 samples; a few of those are zero crossings.
        result &= checkBetween("samples of beep heard", countNonZero(speakerOut), 2800, 3200);
        result &= check("beep still wanted once finished", (int)g_totBeepActive.load(), 0);

        // Nothing more is heard until the beep is asked for again.
        seedSine(g_rxUserdata->infifo1, TEST_SAMPLE_RATE);
        thread.rxProcessing_(&helper);
        result &= check("loudest speaker sample after beep", peak(drain(g_rxUserdata->outfifo2)), 0);

        g_totBeepActive = true;
        seedSine(g_rxUserdata->infifo1, TEST_SAMPLE_RATE);
        thread.rxProcessing_(&helper);
        result &= checkBetween("samples of second beep heard", countNonZero(drain(g_rxUserdata->outfifo2)), 2800, 3200);
        return result;
    }

    // Runs a second of silence from the radio through a receive thread and
    // returns what reaches the speaker.
    static std::vector<short> speakerOutputForSilentRadio()
    {
        FifoGuard fifos;

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();
        seedSilence(g_rxUserdata->infifo1, TEST_SAMPLE_RATE);

        FakeRealtimeHelper helper;
        thread.rxProcessing_(&helper);

        return drain(g_rxUserdata->outfifo2);
    }

    static bool interferingToneIsAddedToReceivedAudio()
    {
        resetGlobals();
        g_analog = 1; // so that what's received reaches the speaker as is
        g_testToneEnabled = true;
        g_testToneFreq = 1000.0f;
        g_testToneAmplitude = 8000.0f;

        auto speakerOut = speakerOutputForSilentRadio();

        bool result = true;

        // Mixing with the (silent) time-out beep brings a peak of 8000 down to about 5600.
        result &= checkBetween("loudest speaker sample", peak(speakerOut), 5400, 5800);
        result &= checkBetween("tone frequency", (int)std::lround(toneFrequency(speakerOut, TEST_SAMPLE_RATE)), 995, 1005);
        return result;
    }

    static bool interferingToneFollowsItsFrequencyAndAmplitudeSettings()
    {
        resetGlobals();
        g_analog = 1;
        g_testToneEnabled = true;
        g_testToneFreq = 400.0f;
        g_testToneAmplitude = 800.0f;

        auto speakerOut = speakerOutputForSilentRadio();

        bool result = true;
        result &= checkBetween("loudest speaker sample", peak(speakerOut), 540, 590);
        result &= checkBetween("tone frequency", (int)std::lround(toneFrequency(speakerOut, TEST_SAMPLE_RATE)), 395, 405);
        return result;
    }

    static bool noInterferingToneUnlessEnabled()
    {
        resetGlobals();
        g_analog = 1;
        g_testToneFreq = 1000.0f;
        g_testToneAmplitude = 8000.0f;

        return check("loudest speaker sample", peak(speakerOutputForSilentRadio()), 0);
    }

    // Recordings are fed through buffers sized for audio arriving as it's
    // spoken or received, so these hand a second of tone over a tenth of a
    // second at a time and let the recording keep up.
    static constexpr int RECORD_ROUNDS = 10;
    static constexpr int SAMPLES_PER_ROUND = TEST_SAMPLE_RATE / RECORD_ROUNDS;
    static constexpr int FRAMES_PER_ROUND = SAMPLES_PER_ROUND / (TEST_SAMPLE_RATE * FRAME_DURATION_MS / MS_TO_SEC);

    static void waitForRound(RecordingFile& recording, int round, int fileSampleRate)
    {
        // Up to a tenth of a second is held back until more arrives. The
        // buffer has room for a quarter of a second, so the next round is
        // only safe to hand over once there's little more than that waiting.
        int expectedSoFar = fileSampleRate * (round + 1) / RECORD_ROUNDS;
        recording.waitForSamples(expectedSoFar - fileSampleRate * 13 / 100);
    }

    static void transmitSecondOfTone(RecordingFile& recording, int fileSampleRate)
    {
        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();

        auto mic = sine(TEST_SAMPLE_RATE, 440.0, 8000.0);
        FakeRealtimeHelper helper;
        for (int round = 0; round < RECORD_ROUNDS; round++)
        {
            g_rxUserdata->infifo2->write(&mic[round * SAMPLES_PER_ROUND], SAMPLES_PER_ROUND);

            // Stop once the microphone audio is used up; carrying on would
            // send (and record) silence in its place.
            helper.framesAllowed = FRAMES_PER_ROUND;
            thread.txProcessing_(&helper);
            drain(g_rxUserdata->outfifo1);
            waitForRound(recording, round, fileSampleRate);
        }
    }

    static void receiveSecondOfTone(RecordingFile& recording, int fileSampleRate)
    {
        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();

        auto radio = sine(TEST_SAMPLE_RATE, 440.0, 8000.0);
        FakeRealtimeHelper helper;
        for (int round = 0; round < RECORD_ROUNDS; round++)
        {
            g_rxUserdata->infifo1->write(&radio[round * SAMPLES_PER_ROUND], SAMPLES_PER_ROUND);
            thread.rxProcessing_(&helper);
            drain(g_rxUserdata->outfifo2);
            waitForRound(recording, round, fileSampleRate);
        }
    }

    static bool isRecordingOfTone(std::vector<short> const& recorded, int fileSampleRate, int lowestPeak, int highestPeak)
    {
        bool result = true;

        // The last couple of tenths of a second may still have been waiting to be recorded.
        result &= checkBetween("samples recorded", (int)recorded.size(), fileSampleRate * 3 / 4, fileSampleRate);
        result &= checkBetween("loudest recorded sample", peak(recorded), lowestPeak, highestPeak);
        result &= checkBetween("recorded tone frequency", (int)std::lround(toneFrequency(recorded, fileSampleRate)), 435, 445);
        return result;
    }

    static bool recordsMicrophoneAudio()
    {
        resetGlobals();
        g_tx = true;
        g_analog = 1;
        FifoGuard fifos;

        RecordingFile recording(TEST_SAMPLE_RATE);
        if (recording.get() == nullptr)
        {
            std::cout << "[could not create file to record to] ";
            return false;
        }
        g_sfRecMicFile = recording.get();
        g_recFileFromMic = true;

        transmitSecondOfTone(recording, TEST_SAMPLE_RATE);

        g_recFileFromMic = false;
        g_sfRecMicFile = nullptr;

        // What's recorded is the microphone as is, not what was then done to it.
        return isRecordingOfTone(recording.finish(), TEST_SAMPLE_RATE, 7900, 8000);
    }

    static bool recordsWhatIsSentToRadio()
    {
        resetGlobals();
        g_tx = true;
        g_analog = 1; // sends the microphone at twice its level
        FifoGuard fifos;

        RecordingFile recording(RECORD_FILE_SAMPLE_RATE);
        if (recording.get() == nullptr)
        {
            std::cout << "[could not create file to record to] ";
            return false;
        }
        g_sfRecFileFromModulator = recording.get();
        g_recFileFromModulator = true;

        transmitSecondOfTone(recording, RECORD_FILE_SAMPLE_RATE);

        g_recFileFromModulator = false;
        g_sfRecFileFromModulator = nullptr;

        return isRecordingOfTone(recording.finish(), RECORD_FILE_SAMPLE_RATE, 15800, 16200);
    }

    static bool recordsWhatIsSentToSpeaker()
    {
        resetGlobals();
        g_analog = 1;
        FifoGuard fifos;

        RecordingFile recording(RECORD_FILE_SAMPLE_RATE);
        if (recording.get() == nullptr)
        {
            std::cout << "[could not create file to record to] ";
            return false;
        }
        g_sfRecDecoderFile = recording.get();
        g_recFileFromDecoder = true;

        receiveSecondOfTone(recording, RECORD_FILE_SAMPLE_RATE);

        g_recFileFromDecoder = false;
        g_sfRecDecoderFile = nullptr;

        // Converting to the file's sample rate can overshoot slightly.
        return isRecordingOfTone(recording.finish(), RECORD_FILE_SAMPLE_RATE, 7900, 8100);
    }

    static bool nothingIsRecordedUnlessAskedFor()
    {
        resetGlobals();
        g_tx = true;
        g_half_duplex = false; // so that both directions run
        g_analog = 1;
        FifoGuard fifos;

        // A file is open in all four places, but recording isn't turned on for any.
        RecordingFile mic(TEST_SAMPLE_RATE);
        if (mic.get() == nullptr)
        {
            std::cout << "[could not create file to record to] ";
            return false;
        }
        g_sfRecMicFile = mic.get();
        g_sfRecFileFromModulator = mic.get();
        g_sfRecFile = mic.get();
        g_sfRecDecoderFile = mic.get();

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        {
            TxRxThread txThread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
            TxRxThread rxThread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
            txThread.initializePipeline_();
            rxThread.initializePipeline_();
            seedSine(g_rxUserdata->infifo2, TEST_SAMPLE_RATE);
            seedSine(g_rxUserdata->infifo1, TEST_SAMPLE_RATE);

            FakeRealtimeHelper helper;
            txThread.txProcessing_(&helper);
            rxThread.rxProcessing_(&helper);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        g_sfRecMicFile = nullptr;
        g_sfRecFileFromModulator = nullptr;
        g_sfRecFile = nullptr;
        g_sfRecDecoderFile = nullptr;

        return check("samples recorded", (int)mic.finish().size(), 0);
    }

    // Runs four seconds of the given microphone audio through a transmit
    // thread in analog mode and returns what was sent to the radio during
    // the last of them.
    static std::vector<short> lastSecondSentToRadio(std::vector<short> const& oneSecondOfMic)
    {
        FifoGuard fifos;

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();

        FakeRealtimeHelper helper;
        std::vector<short> sent;
        for (int second = 0; second < 4; second++)
        {
            g_rxUserdata->infifo2->write(const_cast<short*>(oneSecondOfMic.data()), oneSecondOfMic.size());
            thread.txProcessing_(&helper);
            sent = drain(g_rxUserdata->outfifo1);
        }
        return sent;
    }

    static bool automaticGainControlRaisesQuietMicrophone()
    {
        auto quietSpeech = sine(TEST_SAMPLE_RATE, 440.0, 300.0);

        resetGlobals();
        g_tx = true;
        g_analog = 1;
        double without = rms(lastSecondSentToRadio(quietSpeech));

        resetGlobals();
        g_tx = true;
        g_analog = 1;
        g_agcEnabled = true;
        double with = rms(lastSecondSentToRadio(quietSpeech));

        bool result = true;
        result &= checkBetween("level sent without gain control", (int)without, 400, 450); // 300 peak, doubled
        if (with < without * 1.15)
        {
            std::cout << "[level sent with gain control was " << with << ", expected well above " << without << "] ";
            result = false;
        }
        return result;
    }

    static bool noiseReductionQuietensNoise()
    {
        auto hiss = noise(TEST_SAMPLE_RATE, 3000);

        resetGlobals();
        g_tx = true;
        g_analog = 1;
        double without = rms(lastSecondSentToRadio(hiss));

        resetGlobals();
        g_tx = true;
        g_analog = 1;
        g_noiseReductionEnabled = true;
        double with = rms(lastSecondSentToRadio(hiss));

        bool result = true;
        if (without < 1000.0)
        {
            std::cout << "[level sent without noise reduction was only " << without << "] ";
            result = false;
        }
        if (with > without / 2.0)
        {
            std::cout << "[level sent with noise reduction was " << with << ", expected at most half of " << without << "] ";
            result = false;
        }
        return result;
    }

    static bool stopsProcessingWhenToldTimeIsUp()
    {
        resetGlobals();
        g_tx = true;
        g_half_duplex = false;
        g_analog = 1;
        FifoGuard fifos;

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread txThread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        TxRxThread rxThread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        txThread.initializePipeline_();
        rxThread.initializePipeline_();
        seedSine(g_rxUserdata->infifo2, TEST_SAMPLE_RATE);
        seedSine(g_rxUserdata->infifo1, TEST_SAMPLE_RATE);

        FakeRealtimeHelper helper;
        helper.stopWork = true;
        txThread.txProcessing_(&helper);
        rxThread.rxProcessing_(&helper);

        bool result = true;

        // Nothing is lost; it's all still there for next time.
        result &= check("microphone samples still waiting", g_rxUserdata->infifo2->numUsed(), TEST_SAMPLE_RATE);
        result &= check("radio samples still waiting", g_rxUserdata->infifo1->numUsed(), TEST_SAMPLE_RATE);
        result &= check("samples sent to radio", g_rxUserdata->outfifo1->numUsed(), 0);
        result &= check("samples sent to speaker", g_rxUserdata->outfifo2->numUsed(), 0);

        helper.stopWork = false;
        txThread.txProcessing_(&helper);
        rxThread.rxProcessing_(&helper);
        result &= check("microphone samples waiting once there's time", g_rxUserdata->infifo2->numUsed(), 0);
        result &= check("radio samples waiting once there's time", g_rxUserdata->infifo1->numUsed(), 0);
        result &= checkBetween("samples sent to radio once there's time", g_rxUserdata->outfifo1->numUsed(), TEST_SAMPLE_RATE * 9 / 10, TEST_SAMPLE_RATE * 11 / 10);
        result &= checkBetween("samples sent to speaker once there's time", g_rxUserdata->outfifo2->numUsed(), TEST_SAMPLE_RATE * 9 / 10, TEST_SAMPLE_RATE * 11 / 10);
        return result;
    }

    static bool endOfOverIsSentOnceWhenTransmitEnds()
    {
        resetGlobals();
        g_tx = true;
        FifoGuard fifos;

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        TxRxThread thread(true, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, nullptr, makeHooks(micInDone, recRadioDone, playRadioDone));
        thread.initializePipeline_();

        FakeRealtimeHelper helper;
        bool result = true;

        seedSine(g_rxUserdata->infifo2, TEST_SAMPLE_RATE);
        thread.txProcessing_(&helper);
        result &= checkBetween("loudest sample of voice sent", peak(drain(g_rxUserdata->outfifo1)), 1, 32767);
        result &= check("end of over reported as queued while still talking", (int)g_eoo_enqueued.load(), 0);

        // The microphone has nothing left and transmit is being ended.
        endingTx = true;
        thread.txProcessing_(&helper);

        auto endOfOver = drain(g_rxUserdata->outfifo1);
        result &= check("samples of end of over", (int)endOfOver.size(), freedvInterface.getTxNNomModemSamples());
        result &= checkBetween("loudest sample of end of over", peak(endOfOver), 1, 32767);
        result &= check("end of over reported as queued", (int)g_eoo_enqueued.load(), 1);

        // However long it then takes for transmit to actually end, it isn't sent again.
        thread.txProcessing_(&helper);
        thread.txProcessing_(&helper);
        result &= check("samples sent after end of over", g_rxUserdata->outfifo1->numUsed(), 0);
        result &= check("end of over still reported as queued", (int)g_eoo_enqueued.load(), 1);

        // The next over gets an end of its own.
        endingTx = false;
        seedSine(g_rxUserdata->infifo2, TEST_SAMPLE_RATE);
        thread.txProcessing_(&helper);
        drain(g_rxUserdata->outfifo1);
        result &= check("end of over reported as queued during next over", (int)g_eoo_enqueued.load(), 0);

        endingTx = true;
        thread.txProcessing_(&helper);
        result &= check("samples of second end of over", g_rxUserdata->outfifo1->numUsed(), freedvInterface.getTxNNomModemSamples());
        return result;
    }

    static bool startStopRunsRealTimeHelperLifecycle()
    {
        resetGlobals();
        g_nSoundCards = 1;
        FifoGuard fifos;

        auto helper = std::make_shared<FakeRealtimeHelper>();

        std::atomic<int> micInDone{0}, recRadioDone{0}, playRadioDone{0};
        NeverReused<TxRxThread> thread(false, TEST_SAMPLE_RATE, TEST_SAMPLE_RATE, nullptr, helper, makeHooks(micInDone, recRadioDone, playRadioDone));

        (*thread).start();
        (*thread).waitForReady();
        (*thread).signalToStart();

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (helper->startCalls == 0 && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::yield();
        }

        (*thread).stop();

        bool result = true;
        result &= check("setHelperRealTime calls", helper->setRealTimeCalls.load(), 1);
        result &= check("clearHelperRealTime calls", helper->clearRealTimeCalls.load(), 1);
        result &= helper->startCalls >= 1 ? true : (std::cout << "[startRealTimeWork never called] " << std::endl, false);
        result &= helper->stopCalls >= 1 ? true : (std::cout << "[stopRealTimeWork never called] " << std::endl, false);
        return result;
    }
};

int main()
{
    freedvInterface.start(100, false);

    executeTestCase("txProcessingSkippedWhenHalfDuplexPttOff", &TxRxThreadTest::txProcessingSkippedWhenHalfDuplexPttOff);
    executeTestCase("txProcessingGeneratesModemOutputWhenPttOn", &TxRxThreadTest::txProcessingGeneratesModemOutputWhenPttOn);
    executeTestCase("enteringTxAfterRxResetsPipelineAndSkipsOneCycle", &TxRxThreadTest::enteringTxAfterRxResetsPipelineAndSkipsOneCycle);
    executeTestCase("rxProcessingDemodulatesToOutfifo1ForSingleSoundCard", &TxRxThreadTest::rxProcessingDemodulatesToOutfifo1ForSingleSoundCard);
    executeTestCase("rxProcessingWritesToOutfifo2ForDualSoundCards", &TxRxThreadTest::rxProcessingWritesToOutfifo2ForDualSoundCards);
    executeTestCase("runOnceRequestsFastModeWhenOutputFifoLow", &TxRxThreadTest::runOnceRequestsFastModeWhenOutputFifoLow);
    executeTestCase("clearFifosResetsTxFifosAndSelectedRxFifo", &TxRxThreadTest::clearFifosResetsTxFifosAndSelectedRxFifo);
    executeTestCase("playFileToMicInFinishedHookFiresWhenFileEnds", &TxRxThreadTest::playFileToMicInFinishedHookFiresWhenFileEnds);
    executeTestCase("monitoringTransmitAudioPlaysMicThroughSpeaker", &TxRxThreadTest::monitoringTransmitAudioPlaysMicThroughSpeaker);
    executeTestCase("monitoredTransmitAudioFollowsItsVolumeSetting", &TxRxThreadTest::monitoredTransmitAudioFollowsItsVolumeSetting);
    executeTestCase("monitoredVoiceKeyerAudioFollowsItsOwnVolumeSetting", &TxRxThreadTest::monitoredVoiceKeyerAudioFollowsItsOwnVolumeSetting);
    executeTestCase("micAudioStaysOutOfSpeakerWhenNotMonitoring", &TxRxThreadTest::micAudioStaysOutOfSpeakerWhenNotMonitoring);
    executeTestCase("speakerIsSilentWhileRecordingVoiceKeyerFile", &TxRxThreadTest::speakerIsSilentWhileRecordingVoiceKeyerFile);
    executeTestCase("clearFifosAlsoClearsMonitoredMicAudio", &TxRxThreadTest::clearFifosAlsoClearsMonitoredMicAudio);
    executeTestCase("playFileToMicInStartsOverWhenLooping", &TxRxThreadTest::playFileToMicInStartsOverWhenLooping);
    executeTestCase("playFileFromRadioFinishedHookFiresWhenFileEnds", &TxRxThreadTest::playFileFromRadioFinishedHookFiresWhenFileEnds);
    executeTestCase("playFileFromRadioStartsOverWhenLooping", &TxRxThreadTest::playFileFromRadioStartsOverWhenLooping);
    executeTestCase("recordingFromRadioContinuesUntilStopped", &TxRxThreadTest::recordingFromRadioContinuesUntilStopped);
    executeTestCase("timeOutWarningBeepsThenTurnsItselfOff", &TxRxThreadTest::timeOutWarningBeepsThenTurnsItselfOff);
    executeTestCase("interferingToneIsAddedToReceivedAudio", &TxRxThreadTest::interferingToneIsAddedToReceivedAudio);
    executeTestCase("interferingToneFollowsItsFrequencyAndAmplitudeSettings", &TxRxThreadTest::interferingToneFollowsItsFrequencyAndAmplitudeSettings);
    executeTestCase("noInterferingToneUnlessEnabled", &TxRxThreadTest::noInterferingToneUnlessEnabled);
    executeTestCase("recordsMicrophoneAudio", &TxRxThreadTest::recordsMicrophoneAudio);
    executeTestCase("recordsWhatIsSentToRadio", &TxRxThreadTest::recordsWhatIsSentToRadio);
    executeTestCase("recordsWhatIsSentToSpeaker", &TxRxThreadTest::recordsWhatIsSentToSpeaker);
    executeTestCase("nothingIsRecordedUnlessAskedFor", &TxRxThreadTest::nothingIsRecordedUnlessAskedFor);
    executeTestCase("automaticGainControlRaisesQuietMicrophone", &TxRxThreadTest::automaticGainControlRaisesQuietMicrophone);
    executeTestCase("noiseReductionQuietensNoise", &TxRxThreadTest::noiseReductionQuietensNoise);
    executeTestCase("stopsProcessingWhenToldTimeIsUp", &TxRxThreadTest::stopsProcessingWhenToldTimeIsUp);
    executeTestCase("endOfOverIsSentOnceWhenTransmitEnds", &TxRxThreadTest::endOfOverIsSentOnceWhenTransmitEnds);
    executeTestCase("startStopRunsRealTimeHelperLifecycle", &TxRxThreadTest::startStopRunsRealTimeHelperLifecycle);

    freedvInterface.stop();

    return testResult();
}
