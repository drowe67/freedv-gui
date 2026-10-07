/*
   voicekeyer_cache.h

   Keeps an in-memory copy of the voice keyer file.
*/

#ifndef VOICEKEYER_CACHE_H
#define VOICEKEYER_CACHE_H

#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <sndfile.h>

// Per-SNDFILE read position into a cached file. Must outlive the SNDFILE
// returned by VoiceKeyerFileCache::open().
struct VoiceKeyerMemoryReader
{
    std::shared_ptr<const std::vector<char>> data;
    sf_count_t pos = 0;
};

// Starting the voice keyer must not perform blocking file I/O on the UI
// thread: if the file lives on cloud storage (e.g. an iCloud-synced
// ~/Documents with "Optimize Mac Storage" enabled) or a network share, the
// first open can block for seconds while the contents are fetched, delaying
// TX. This class loads the file into memory in the background ahead of time
// so that opening it later never touches the disk.
class VoiceKeyerFileCache
{
public:
    // Asynchronously loads the given file into memory, replacing any
    // previously cached file.
    void preload(std::string const& path);

    // Opens the cached copy of path for reading. Returns nullptr (and starts
    // a background reload) if the file isn't cached or has changed on disk
    // since it was loaded; the caller should then open the file directly.
    SNDFILE* open(std::string const& path, SF_INFO* sfInfo, std::unique_ptr<VoiceKeyerMemoryReader>& reader);

private:
    struct Entry
    {
        std::string path;
        long long size;
        time_t modTime;
        std::shared_ptr<const std::vector<char>> data;
    };

    // Shared with loader threads so that they can safely finish after
    // this object is destroyed.
    struct State
    {
        std::mutex mtx;
        std::string requestedPath;
        std::shared_ptr<const Entry> entry;
    };

    std::shared_ptr<State> state_ = std::make_shared<State>();

    static bool getFileMetadata_(std::string const& path, long long& size, time_t& modTime);
    static void load_(std::shared_ptr<State> state, std::string path);
};

#endif // VOICEKEYER_CACHE_H
