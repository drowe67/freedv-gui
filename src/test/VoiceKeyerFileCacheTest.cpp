#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <sndfile.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <wx/init.h>
#include <wx/log.h>

#include "../voicekeyer_cache.h"
#include "util/logging/ulog.h"
#include "UnitTestCommon.h"

using namespace std::chrono_literals;

namespace {

constexpr int SAMPLE_RATE = 8000;
constexpr auto TIMEOUT = 10s;

std::vector<short> ramp(int numSamples, short start)
{
    std::vector<short> result(numSamples);
    for (int index = 0; index < numSamples; index++)
    {
        result[index] = (short)(start + index);
    }
    return result;
}

// A file on disk that's removed once the test case is done.
class TestFile
{
public:
    explicit TestFile(const char* extension = ".wav")
    {
        const char* tmpDir = getenv("TMPDIR");
        path_ = std::string(tmpDir != nullptr ? tmpDir : "/tmp") + "/VoiceKeyerFileCacheTest-" +
            std::to_string(getpid()) + "-" + std::to_string(NextId_++) + extension;
    }

    TestFile(std::vector<short> const& samples, int sampleRate = SAMPLE_RATE)
        : TestFile()
    {
        writeAudio(samples, sampleRate);
    }

    ~TestFile()
    {
        remove(path_.c_str());
    }

    std::string const& path() const { return path_; }

    void writeAudio(std::vector<short> const& samples, int sampleRate = SAMPLE_RATE)
    {
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
    }

    void writeText(std::string const& contents)
    {
        std::ofstream(path_) << contents;
    }

    // Pads (or cuts) the file to the given size without having to write
    // that much to disk.
    bool resize(long long numBytes)
    {
        return truncate(path_.c_str(), numBytes) == 0;
    }

    bool setReadable(bool readable)
    {
        return chmod(path_.c_str(), readable ? 0600 : 0000) == 0;
    }

    // File systems may only record modification times to the second, so
    // changes made by a test are given a time of their own.
    void setModificationTime(int secondsFromNow)
    {
        struct timeval times[2];
        gettimeofday(&times[0], nullptr);
        times[0].tv_sec += secondsFromNow;
        times[1] = times[0];
        utimes(path_.c_str(), times);
    }

private:
    static int NextId_;
    std::string path_;
};

int TestFile::NextId_ = 0;

// A file opened from the cache.
struct CachedFile
{
    SNDFILE* file = nullptr;
    SF_INFO info = {};
    std::unique_ptr<VoiceKeyerMemoryReader> reader;

    CachedFile() = default;
    CachedFile(CachedFile const&) = delete;

    ~CachedFile()
    {
        close();
    }

    void close()
    {
        if (file != nullptr) sf_close(file);
        file = nullptr;
    }

    bool open(VoiceKeyerFileCache& cache, std::string const& path)
    {
        close();
        info = {};
        file = cache.open(path, &info, reader);
        return file != nullptr;
    }

    // Files are loaded in the background, so it takes a moment before
    // they're available.
    bool waitForOpen(VoiceKeyerFileCache& cache, std::string const& path)
    {
        auto deadline = std::chrono::steady_clock::now() + TIMEOUT;
        while (!open(cache, path))
        {
            if (std::chrono::steady_clock::now() > deadline)
            {
                std::cout << "[file was never cached] ";
                return false;
            }
            std::this_thread::sleep_for(10ms);
        }
        return true;
    }

    std::vector<short> read(int maxSamples = 1000000)
    {
        std::vector<short> result(maxSamples);
        result.resize(sf_read_short(file, result.data(), maxSamples));
        return result;
    }
};

bool samplesAre(std::vector<short> const& actual, std::vector<short> const& expected)
{
    if (actual == expected) return true;
    std::cout << "[read " << actual.size() << " samples";
    if (!actual.empty()) std::cout << " starting with " << actual[0];
    std::cout << ", expected " << expected.size();
    if (!expected.empty()) std::cout << " starting with " << expected[0];
    std::cout << "] ";
    return false;
}

bool notAvailableUntilLoaded()
{
    TestFile file(ramp(1000, 1));
    VoiceKeyerFileCache cache;

    // Nothing's been asked for yet, so the caller has to read from disk.
    CachedFile cached;
    if (cached.open(cache, file.path()) || cached.reader != nullptr)
    {
        std::cout << "[file was available without being loaded] ";
        return false;
    }

    // Asking for it is enough to have it loaded for next time.
    return cached.waitForOpen(cache, file.path());
}

bool cachedFileMatchesFileOnDisk()
{
    auto samples = ramp(SAMPLE_RATE * 2, -5000);
    TestFile file(samples, 16000);
    VoiceKeyerFileCache cache;
    cache.preload(file.path());

    CachedFile cached;
    if (!cached.waitForOpen(cache, file.path())) return false;

    if (cached.info.samplerate != 16000 || cached.info.channels != 1 || cached.info.frames != (sf_count_t)samples.size())
    {
        std::cout << "[wrong format: " << cached.info.samplerate << " Hz, " << cached.info.channels
                  << " channel(s), " << cached.info.frames << " frames] ";
        return false;
    }
    return samplesAre(cached.read(), samples) && cached.reader != nullptr;
}

bool readsInBlocksUntilEndOfFile()
{
    auto samples = ramp(1000, 1);
    TestFile file(samples);
    VoiceKeyerFileCache cache;
    cache.preload(file.path());

    CachedFile cached;
    if (!cached.waitForOpen(cache, file.path())) return false;

    std::vector<short> result;
    for (int iteration = 0; iteration < 10; iteration++)
    {
        auto block = cached.read(160);
        result.insert(result.end(), block.begin(), block.end());
    }
    return samplesAre(result, samples) && cached.read(160).empty();
}

bool canSeekWithinCachedFile()
{
    auto samples = ramp(1000, 1);
    TestFile file(samples);
    VoiceKeyerFileCache cache;
    cache.preload(file.path());

    CachedFile cached;
    if (!cached.waitForOpen(cache, file.path())) return false;

    // Looping playback rewinds to the beginning.
    cached.read();
    if (sf_seek(cached.file, 0, SEEK_SET) != 0 || !samplesAre(cached.read(), samples)) return false;

    if (sf_seek(cached.file, 600, SEEK_SET) != 600 ||
        !samplesAre(cached.read(), std::vector<short>(samples.begin() + 600, samples.end())))
    {
        return false;
    }

    if (sf_seek(cached.file, -100, SEEK_END) != 900 ||
        !samplesAre(cached.read(), std::vector<short>(samples.begin() + 900, samples.end())))
    {
        return false;
    }

    // Can't go beyond either end of the file.
    if (sf_seek(cached.file, 1001, SEEK_SET) != -1 || sf_seek(cached.file, -1, SEEK_SET) != -1)
    {
        std::cout << "[was able to seek outside of the file] ";
        return false;
    }

    return sf_seek(cached.file, 1000, SEEK_SET) == 1000 && cached.read().empty();
}

bool eachOpenReadsIndependently()
{
    auto samples = ramp(1000, 1);
    TestFile file(samples);
    VoiceKeyerFileCache cache;
    cache.preload(file.path());

    CachedFile first;
    CachedFile second;
    if (!first.waitForOpen(cache, file.path()) || !second.waitForOpen(cache, file.path())) return false;

    return
        samplesAre(first.read(400), std::vector<short>(samples.begin(), samples.begin() + 400)) &&
        samplesAre(second.read(), samples) &&
        samplesAre(first.read(), std::vector<short>(samples.begin() + 400, samples.end()));
}

bool changedSizeInvalidatesCache()
{
    TestFile file(ramp(1000, 1));
    VoiceKeyerFileCache cache;
    cache.preload(file.path());

    CachedFile cached;
    if (!cached.waitForOpen(cache, file.path())) return false;
    cached.close();

    auto newSamples = ramp(1500, 2000);
    file.writeAudio(newSamples);
    if (cached.open(cache, file.path()))
    {
        std::cout << "[stale copy returned after file changed] ";
        return false;
    }

    return cached.waitForOpen(cache, file.path()) && samplesAre(cached.read(), newSamples);
}

bool changedModificationTimeInvalidatesCache()
{
    TestFile file(ramp(1000, 1));
    file.setModificationTime(-60);
    VoiceKeyerFileCache cache;
    cache.preload(file.path());

    CachedFile cached;
    if (!cached.waitForOpen(cache, file.path())) return false;
    cached.close();

    // Same length, different contents.
    auto newSamples = ramp(1000, 2000);
    file.writeAudio(newSamples);
    file.setModificationTime(-30);
    if (cached.open(cache, file.path()))
    {
        std::cout << "[stale copy returned after file changed] ";
        return false;
    }

    return cached.waitForOpen(cache, file.path()) && samplesAre(cached.read(), newSamples);
}

bool unchangedFileStaysCached()
{
    TestFile file(ramp(1000, 1));
    VoiceKeyerFileCache cache;
    cache.preload(file.path());

    CachedFile cached;
    if (!cached.waitForOpen(cache, file.path())) return false;

    for (int iteration = 0; iteration < 20; iteration++)
    {
        if (!cached.open(cache, file.path()))
        {
            std::cout << "[had to reload on open " << iteration << "] ";
            return false;
        }
    }
    return true;
}

bool onlyTheMostRecentFileIsCached()
{
    auto firstSamples = ramp(1000, 1);
    auto secondSamples = ramp(800, 3000);
    TestFile first(firstSamples);
    TestFile second(secondSamples);
    VoiceKeyerFileCache cache;
    cache.preload(first.path());

    CachedFile cached;
    if (!cached.waitForOpen(cache, first.path())) return false;

    // A different file is never served from the first one's copy.
    if (cached.open(cache, second.path()))
    {
        std::cout << "[opened a file that wasn't cached] ";
        return false;
    }
    if (!cached.waitForOpen(cache, second.path()) || !samplesAre(cached.read(), secondSamples)) return false;

    if (cached.open(cache, first.path()))
    {
        std::cout << "[first file still cached] ";
        return false;
    }
    return true;
}

bool emptyPathClearsCache()
{
    TestFile file(ramp(1000, 1));
    VoiceKeyerFileCache cache;
    cache.preload(file.path());

    CachedFile cached;
    if (!cached.waitForOpen(cache, file.path())) return false;

    cache.preload("");
    if (cached.open(cache, file.path()))
    {
        std::cout << "[file still cached] ";
        return false;
    }

    // Nothing to load, and nothing should go wrong in trying.
    return !cached.open(cache, "");
}

bool clearingWinsOverLoadInProgress()
{
    TestFile file(ramp(SAMPLE_RATE * 10, 1));

    for (int iteration = 0; iteration < 20; iteration++)
    {
        VoiceKeyerFileCache cache;
        cache.preload(file.path());
        cache.preload("");

        // Give the load a chance to finish; it shouldn't be kept.
        std::this_thread::sleep_for(20ms);

        std::unique_ptr<VoiceKeyerMemoryReader> reader;
        SF_INFO info = {};
        SNDFILE* opened = cache.open("", &info, reader);
        if (opened != nullptr)
        {
            sf_close(opened);
            return false;
        }
    }
    return true;
}

bool missingFileIsNeverAvailable()
{
    TestFile file; // never created
    VoiceKeyerFileCache cache;
    cache.preload(file.path());

    CachedFile cached;
    for (int iteration = 0; iteration < 10; iteration++)
    {
        if (cached.open(cache, file.path())) return false;
        std::this_thread::sleep_for(10ms);
    }
    return cached.reader == nullptr;
}

bool nonAudioFileIsNeverAvailable()
{
    TestFile file(".wav");
    file.writeText("This is not a sound file.");
    VoiceKeyerFileCache cache;
    cache.preload(file.path());

    // The caller falls back to opening the file directly, which is where
    // the error gets reported.
    CachedFile cached;
    for (int iteration = 0; iteration < 20; iteration++)
    {
        if (cached.open(cache, file.path())) return false;
        std::this_thread::sleep_for(10ms);
    }
    return cached.reader == nullptr;
}

bool neverAvailable(VoiceKeyerFileCache& cache, std::string const& path, const char* reason)
{
    CachedFile cached;
    for (int iteration = 0; iteration < 30; iteration++)
    {
        if (cached.open(cache, path))
        {
            std::cout << "[file was cached despite being " << reason << "] ";
            return false;
        }
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

bool veryLargeFileIsNotCached()
{
    const long long MAX_CACHED_SIZE = 64 * 1024 * 1024;

    // A playable file with a lot of padding on the end.
    auto samples = ramp(1000, 1);
    TestFile file(samples);
    VoiceKeyerFileCache cache;

    if (!file.resize(MAX_CACHED_SIZE + 1))
    {
        std::cout << "[could not create large file] ";
        return false;
    }
    cache.preload(file.path());
    if (!neverAvailable(cache, file.path(), "too large")) return false;

    // Right at the limit is fine.
    file.resize(MAX_CACHED_SIZE);
    file.setModificationTime(-30);
    CachedFile cached;
    return cached.waitForOpen(cache, file.path()) && samplesAre(cached.read(), samples);
}

bool unreadableFileIsNotCached()
{
    if (geteuid() == 0)
    {
        // Everything's readable when running as root.
        return true;
    }

    auto samples = ramp(1000, 1);
    TestFile file(samples);
    VoiceKeyerFileCache cache;

    // wxWidgets would otherwise report the failure to open the file itself.
    wxLogNull noLogging;

    if (!file.setReadable(false)) return false;
    cache.preload(file.path());
    bool result = neverAvailable(cache, file.path(), "unreadable");

    // Becomes available once the problem's been fixed, even though the
    // file itself hasn't changed.
    file.setReadable(true);
    CachedFile cached;
    return result && cached.waitForOpen(cache, file.path()) && samplesAre(cached.read(), samples);
}

bool openFileOutlivesCache()
{
    auto samples = ramp(1000, 1);
    TestFile file(samples);
    TestFile other(ramp(500, 4000));

    CachedFile cached;
    CachedFile replaced;
    {
        VoiceKeyerFileCache cache;
        cache.preload(file.path());
        if (!cached.waitForOpen(cache, file.path()) || !replaced.waitForOpen(cache, file.path())) return false;

        // Playback of one file continues while another is being loaded.
        CachedFile otherCached;
        if (!otherCached.waitForOpen(cache, other.path())) return false;
        if (!samplesAre(replaced.read(), samples)) return false;
    }

    return samplesAre(cached.read(), samples);
}

bool cacheCanBeDestroyedWhileLoading()
{
    TestFile file(ramp(SAMPLE_RATE * 10, 1));
    for (int iteration = 0; iteration < 50; iteration++)
    {
        VoiceKeyerFileCache cache;
        cache.preload(file.path());
    }

    // Let the loads finish while the file still exists.
    std::this_thread::sleep_for(200ms);
    return true;
}

}

int main()
{
    wxInitializer initializer;
    if (!initializer.IsOk())
    {
        std::cout << "Could not initialize wxWidgets" << std::endl;
        return -1;
    }

    ulog_set_quiet(true);

    executeTestCase("notAvailableUntilLoaded", notAvailableUntilLoaded);
    executeTestCase("cachedFileMatchesFileOnDisk", cachedFileMatchesFileOnDisk);
    executeTestCase("readsInBlocksUntilEndOfFile", readsInBlocksUntilEndOfFile);
    executeTestCase("canSeekWithinCachedFile", canSeekWithinCachedFile);
    executeTestCase("eachOpenReadsIndependently", eachOpenReadsIndependently);
    executeTestCase("changedSizeInvalidatesCache", changedSizeInvalidatesCache);
    executeTestCase("changedModificationTimeInvalidatesCache", changedModificationTimeInvalidatesCache);
    executeTestCase("unchangedFileStaysCached", unchangedFileStaysCached);
    executeTestCase("onlyTheMostRecentFileIsCached", onlyTheMostRecentFileIsCached);
    executeTestCase("emptyPathClearsCache", emptyPathClearsCache);
    executeTestCase("clearingWinsOverLoadInProgress", clearingWinsOverLoadInProgress);
    executeTestCase("missingFileIsNeverAvailable", missingFileIsNeverAvailable);
    executeTestCase("nonAudioFileIsNeverAvailable", nonAudioFileIsNeverAvailable);
    executeTestCase("veryLargeFileIsNotCached", veryLargeFileIsNotCached);
    executeTestCase("unreadableFileIsNotCached", unreadableFileIsNotCached);
    executeTestCase("openFileOutlivesCache", openFileOutlivesCache);
    executeTestCase("cacheCanBeDestroyedWhileLoading", cacheCanBeDestroyedWhileLoading);
    return testResult();
}
