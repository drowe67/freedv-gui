#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <sndfile.h>
#include <unistd.h>

#include "../PlaybackStep.h"
#include "../../util/logging/ulog.h"
#include "../../test/UnitTestCommon.h"

using namespace std::chrono_literals;

// Normally provided by the main application.
std::mutex g_mutexProtectingPlayFiles;

namespace {

constexpr int SAMPLE_RATE = 8000;
constexpr int BLOCK_SIZE = 160;
constexpr auto TIMEOUT = 10s;

// A sound file on disk that's removed once the test case is done.
class TestFile
{
public:
    TestFile(std::vector<short> const& samples, int sampleRate)
        : sampleRate_(sampleRate)
    {
        const char* tmpDir = getenv("TMPDIR");
        path_ = std::string(tmpDir != nullptr ? tmpDir : "/tmp") + "/PlaybackStepTest-" +
            std::to_string(getpid()) + "-" + std::to_string(NextId_++) + ".wav";

        SF_INFO info = {};
        info.samplerate = sampleRate;
        info.channels = 1;
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
        close();
        remove(path_.c_str());
    }

    SNDFILE* get() const { return file_; }
    int sampleRate() const { return sampleRate_; }

    // Same as what FreeDV does when stopping playback.
    void close()
    {
        std::unique_lock<std::mutex> lk(g_mutexProtectingPlayFiles);
        if (file_ != nullptr)
        {
            sf_close(file_);
            file_ = nullptr;
        }
    }

private:
    static inline int NextId_ = 0;

    std::string path_;
    int sampleRate_;
    std::atomic<SNDFILE*> file_ { nullptr };
};

struct Fixture
{
    std::atomic<TestFile*> currentFile { nullptr };
    std::atomic<int> numCompletions { 0 };
    std::atomic<bool> loop { false };
    PlaybackStep step;

    Fixture()
        : step(
            SAMPLE_RATE,
            [this]() {
                auto file = currentFile.load();
                return file != nullptr ? file->sampleRate() : 0;
            },
            [this]() -> SNDFILE* {
                auto file = currentFile.load();
                return file != nullptr ? file->get() : nullptr;
            },
            [this]() {
                numCompletions++;
                if (loop)
                {
                    sf_seek(currentFile.load()->get(), 0, SEEK_SET);
                }
                else
                {
                    // Stops the step from reading the file any further.
                    currentFile = nullptr;
                }
            })
    {
        // empty
    }

    std::vector<short> runOnce(int numSamples = BLOCK_SIZE)
    {
        int numOutput = 0;
        short* output = step.execute(nullptr, numSamples, &numOutput);
        return std::vector<short>(output, output + numOutput);
    }

    // File I/O happens on another thread, so keep asking until we've got
    // what we need or it's clear that we're not going to.
    std::vector<short> collect(size_t numSamples, std::function<bool()> const& stopEarlyFn = nullptr)
    {
        std::vector<short> result;
        auto deadline = std::chrono::steady_clock::now() + TIMEOUT;
        while (result.size() < numSamples && std::chrono::steady_clock::now() < deadline)
        {
            auto block = runOnce(std::min((size_t)BLOCK_SIZE, numSamples - result.size()));
            result.insert(result.end(), block.begin(), block.end());
            if (block.size() == 0)
            {
                if (stopEarlyFn && stopEarlyFn()) break;
                std::this_thread::sleep_for(1ms);
            }
        }
        return result;
    }

    bool waitForCompletions(int count)
    {
        auto deadline = std::chrono::steady_clock::now() + TIMEOUT;
        while (numCompletions < count && std::chrono::steady_clock::now() < deadline)
        {
            runOnce();
            std::this_thread::sleep_for(1ms);
        }
        return numCompletions >= count;
    }
};

std::vector<short> ramp(int numSamples)
{
    std::vector<short> result(numSamples);
    for (int i = 0; i < numSamples; i++)
    {
        result[i] = (i % 30000) + 1; // never zero
    }
    return result;
}

std::vector<short> tone(double freqHz, int sampleRate, int numSamples)
{
    std::vector<short> result(numSamples);
    for (int i = 0; i < numSamples; i++)
    {
        result[i] = (short)(8000 * std::sin(2 * M_PI * freqHz * i / sampleRate));
    }
    return result;
}

bool sampleRatesMatchConstructor()
{
    Fixture f;
    return f.step.getInputSampleRate() == SAMPLE_RATE && f.step.getOutputSampleRate() == SAMPLE_RATE;
}

bool noOutputWithoutFile()
{
    Fixture f;
    for (int i = 0; i < 20; i++)
    {
        if (f.runOnce().size() != 0) return false;
        std::this_thread::sleep_for(1ms);
    }
    return f.numCompletions == 0;
}

bool playsContentsOfFile()
{
    auto contents = ramp(SAMPLE_RATE * 2);
    TestFile file(contents, SAMPLE_RATE);
    if (file.get() == nullptr)
    {
        std::cout << "[could not create test file] ";
        return false;
    }

    Fixture f;
    f.currentFile = &file;

    auto output = f.collect(contents.size());
    if (output != contents)
    {
        std::cout << "[got " << output.size() << " samples, expected " << contents.size() << "] ";
        return false;
    }
    return true;
}

bool neverOutputsMoreThanRequested()
{
    auto contents = ramp(SAMPLE_RATE);
    TestFile file(contents, SAMPLE_RATE);

    Fixture f;
    f.currentFile = &file;

    // Give the step time to buffer up the whole file.
    f.runOnce(0);
    std::this_thread::sleep_for(200ms);

    auto output = f.runOnce(100);
    return output == std::vector<short>(contents.begin(), contents.begin() + 100);
}

bool reportsCompletionOnceAfterEverythingIsPlayed()
{
    auto contents = ramp(SAMPLE_RATE / 2);
    TestFile file(contents, SAMPLE_RATE);

    Fixture f;
    f.currentFile = &file;

    // Shouldn't complete while there's still audio left to hand out.
    auto output = f.collect(contents.size() - BLOCK_SIZE);
    std::this_thread::sleep_for(100ms);
    if (f.numCompletions != 0)
    {
        std::cout << "[completed with audio still pending] ";
        return false;
    }

    auto remainder = f.collect(BLOCK_SIZE);
    output.insert(output.end(), remainder.begin(), remainder.end());
    if (output != contents || !f.waitForCompletions(1))
    {
        std::cout << "[did not complete] ";
        return false;
    }

    // Nothing further should happen once playback is done.
    for (int i = 0; i < 20; i++)
    {
        if (f.runOnce().size() != 0) return false;
        std::this_thread::sleep_for(1ms);
    }
    return f.numCompletions == 1;
}

bool loopsWhenCompletionRewindsFile()
{
    // Same as how the voice keyer repeats its file.
    auto contents = ramp(1000);
    TestFile file(contents, SAMPLE_RATE);

    Fixture f;
    f.loop = true;
    f.currentFile = &file;

    auto output = f.collect(contents.size() * 3 + 500);
    f.currentFile = nullptr;

    if (output.size() != contents.size() * 3 + 500)
    {
        std::cout << "[got " << output.size() << " samples] ";
        return false;
    }

    for (size_t i = 0; i < output.size(); i++)
    {
        if (output[i] != contents[i % contents.size()])
        {
            std::cout << "[mismatch at sample " << i << "] ";
            return false;
        }
    }
    return f.numCompletions >= 3;
}

bool resamplesFileToPipelineRate(int fileSampleRate)
{
    const double TONE_FREQ_HZ = 400;
    const int DURATION_SECS = 2;

    TestFile file(tone(TONE_FREQ_HZ, fileSampleRate, fileSampleRate * DURATION_SECS), fileSampleRate);

    Fixture f;
    f.currentFile = &file;

    auto output = f.collect(SAMPLE_RATE * DURATION_SECS, [&f]() { return f.numCompletions > 0; });

    // Should have the same duration (give or take the resampler's delay)...
    int expectedSamples = SAMPLE_RATE * DURATION_SECS;
    if (std::abs((int)output.size() - expectedSamples) > SAMPLE_RATE / 20)
    {
        std::cout << "[got " << output.size() << " samples, expected about " << expectedSamples << "] ";
        return false;
    }

    // ...and the same pitch.
    int crossings = 0;
    for (size_t i = 1; i < output.size(); i++)
    {
        if ((output[i - 1] < 0) != (output[i] < 0)) crossings++;
    }
    double freqHz = crossings / 2.0 / ((double)output.size() / SAMPLE_RATE);
    if (std::fabs(freqHz - TONE_FREQ_HZ) > 5)
    {
        std::cout << "[tone is at " << freqHz << " Hz, expected " << TONE_FREQ_HZ << " Hz] ";
        return false;
    }
    return true;
}

bool stopsWhenFileIsClosedDuringPlayback()
{
    auto contents = ramp(SAMPLE_RATE * 20);
    TestFile file(contents, SAMPLE_RATE);

    Fixture f;
    f.currentFile = &file;
    if (f.collect(SAMPLE_RATE).size() != (size_t)SAMPLE_RATE) return false;

    // Stop in the same order as FreeDV does: close the file, then let the step notice.
    file.close();
    f.runOnce();

    // Whatever was already buffered may still come out, but then it should dry up
    // and not report completion (as the file didn't actually finish).
    auto deadline = std::chrono::steady_clock::now() + TIMEOUT;
    int emptyBlocks = 0;
    while (emptyBlocks < 20 && std::chrono::steady_clock::now() < deadline)
    {
        emptyBlocks = (f.runOnce().size() == 0) ? emptyBlocks + 1 : 0;
        std::this_thread::sleep_for(1ms);
    }
    return emptyBlocks == 20 && f.numCompletions == 0;
}

bool playsSecondFileAfterFirst()
{
    auto firstContents = ramp(2000);
    auto secondContents = tone(400, SAMPLE_RATE, 2000);
    TestFile first(firstContents, SAMPLE_RATE);
    TestFile second(secondContents, SAMPLE_RATE);

    Fixture f;
    f.currentFile = &first;
    if (f.collect(firstContents.size()) != firstContents || !f.waitForCompletions(1)) return false;

    f.currentFile = &second;
    return f.collect(secondContents.size()) == secondContents && f.waitForCompletions(2);
}

// Returns the frequency of a tone by counting zero crossings.
double toneFrequency(std::vector<short> const& samples, int sampleRate)
{
    if (samples.size() < 2) return 0;

    int crossings = 0;
    for (size_t i = 1; i < samples.size(); i++)
    {
        if ((samples[i - 1] < 0) != (samples[i] < 0)) crossings++;
    }
    return crossings / 2.0 / ((double)samples.size() / sampleRate);
}

bool playsFilesWithDifferentSampleRatesInTurn()
{
    // Each file needs its own conversion to the pipeline's sample rate.
    const int DURATION_SECS = 1;
    const struct { int sampleRate; double toneFreqHz; } FILES[] = {
        { 16000, 400 }, { 8000, 700 }, { 48000, 1000 }, { 16000, 550 }
    };

    Fixture f;
    int numPlayed = 0;
    for (auto& fileInfo : FILES)
    {
        TestFile file(
            tone(fileInfo.toneFreqHz, fileInfo.sampleRate, fileInfo.sampleRate * DURATION_SECS), fileInfo.sampleRate);
        f.currentFile = &file;

        int completionsSoFar = numPlayed;
        auto output = f.collect(SAMPLE_RATE * DURATION_SECS, [&]() { return f.numCompletions > completionsSoFar; });
        numPlayed++;

        int expectedSamples = SAMPLE_RATE * DURATION_SECS;
        double freqHz = toneFrequency(output, SAMPLE_RATE);
        if (std::abs((int)output.size() - expectedSamples) > SAMPLE_RATE / 20 || std::fabs(freqHz - fileInfo.toneFreqHz) > 10)
        {
            std::cout << "[file " << numPlayed << " (" << fileInfo.sampleRate << " Hz): got " << output.size()
                      << " samples of a " << freqHz << " Hz tone, expected about " << expectedSamples
                      << " of " << fileInfo.toneFreqHz << " Hz] ";
            return false;
        }

        // Nothing of this file should spill over into the next one.
        if (!f.waitForCompletions(numPlayed)) return false;
        f.currentFile = nullptr;
        f.collect(SAMPLE_RATE, []() { return true; });
        std::this_thread::sleep_for(20ms);
        f.collect(SAMPLE_RATE, []() { return true; });
    }
    return true;
}

bool resetDiscardsBufferedAudio()
{
    auto contents = ramp(SAMPLE_RATE * 2);
    TestFile file(contents, SAMPLE_RATE);

    Fixture f;
    f.currentFile = &file;

    // Give the step time to buffer up the whole file, then throw it all away.
    f.runOnce(0);
    std::this_thread::sleep_for(200ms);
    f.step.reset();

    // The file's now at its end, so there's nothing else to play.
    return f.waitForCompletions(1) && f.runOnce().size() == 0;
}

bool destroysCleanlyDuringPlayback()
{
    // Long enough that the step can't buffer the whole file, so that its
    // file I/O thread still has work to do when it's told to stop.
    auto contents = ramp(SAMPLE_RATE * 10);

    for (int i = 0; i < 10; i++)
    {
        TestFile file(contents, SAMPLE_RATE);
        Fixture f;
        f.currentFile = &file;
        if (f.collect(BLOCK_SIZE * i).size() != (size_t)(BLOCK_SIZE * i)) return false;
    }
    return true;
}

}

int main()
{
    ulog_set_quiet(true);

    executeTestCase("sampleRatesMatchConstructor", sampleRatesMatchConstructor);
    executeTestCase("noOutputWithoutFile", noOutputWithoutFile);
    executeTestCase("playsContentsOfFile", playsContentsOfFile);
    executeTestCase("neverOutputsMoreThanRequested", neverOutputsMoreThanRequested);
    executeTestCase("reportsCompletionOnceAfterEverythingIsPlayed", reportsCompletionOnceAfterEverythingIsPlayed);
    executeTestCase("loopsWhenCompletionRewindsFile", loopsWhenCompletionRewindsFile);
    executeTestCase("resamplesFileToPipelineRate (16 kHz file)", []() { return resamplesFileToPipelineRate(16000); });
    executeTestCase("resamplesFileToPipelineRate (48 kHz file)", []() { return resamplesFileToPipelineRate(48000); });
    executeTestCase("stopsWhenFileIsClosedDuringPlayback", stopsWhenFileIsClosedDuringPlayback);
    executeTestCase("playsSecondFileAfterFirst", playsSecondFileAfterFirst);
    executeTestCase("playsFilesWithDifferentSampleRatesInTurn", playsFilesWithDifferentSampleRatesInTurn);
    executeTestCase("resetDiscardsBufferedAudio", resetDiscardsBufferedAudio);
    executeTestCase("destroysCleanlyDuringPlayback", destroysCleanlyDuringPlayback);
    return testResult();
}
