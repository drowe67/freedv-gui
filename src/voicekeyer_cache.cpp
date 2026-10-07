/*
   voicekeyer_cache.cpp

   Keeps an in-memory copy of the voice keyer file.
*/

#include "voicekeyer_cache.h"

#include <algorithm>
#include <cstring>
#include <thread>
#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#include "util/logging/ulog.h"

// Voice keyer files are normally well under this; anything larger is
// opened directly instead of being held in memory.
#define MAX_CACHED_FILE_SIZE_BYTES (64 * 1024 * 1024)

static sf_count_t MemGetFileLen_(void* userData)
{
    auto reader = static_cast<VoiceKeyerMemoryReader*>(userData);
    return reader->data->size();
}

static sf_count_t MemSeek_(sf_count_t offset, int whence, void* userData)
{
    auto reader = static_cast<VoiceKeyerMemoryReader*>(userData);
    sf_count_t size = reader->data->size();
    sf_count_t newPos;
    switch (whence)
    {
        case SEEK_SET:
            newPos = offset;
            break;
        case SEEK_CUR:
            newPos = reader->pos + offset;
            break;
        case SEEK_END:
            newPos = size + offset;
            break;
        default:
            return -1;
    }

    if (newPos < 0 || newPos > size)
    {
        return -1;
    }

    reader->pos = newPos;
    return reader->pos;
}

static sf_count_t MemRead_(void* ptr, sf_count_t count, void* userData)
{
    auto reader = static_cast<VoiceKeyerMemoryReader*>(userData);
    sf_count_t size = reader->data->size();
    sf_count_t toRead = std::max((sf_count_t)0, std::min(count, size - reader->pos));
    memcpy(ptr, reader->data->data() + reader->pos, toRead);
    reader->pos += toRead;
    return toRead;
}

static sf_count_t MemWrite_(const void*, sf_count_t, void*)
{
    // Read-only.
    return 0;
}

static sf_count_t MemTell_(void* userData)
{
    auto reader = static_cast<VoiceKeyerMemoryReader*>(userData);
    return reader->pos;
}

void VoiceKeyerFileCache::preload(std::string const& path)
{
    {
        std::unique_lock<std::mutex> lk(state_->mtx);
        state_->requestedPath = path;
        if (path == "")
        {
            state_->entry = nullptr;
            return;
        }
    }

    std::thread(&VoiceKeyerFileCache::load_, state_, path).detach();
}

SNDFILE* VoiceKeyerFileCache::open(std::string const& path, SF_INFO* sfInfo, std::unique_ptr<VoiceKeyerMemoryReader>& reader)
{
    std::shared_ptr<const Entry> entry;
    {
        std::unique_lock<std::mutex> lk(state_->mtx);
        entry = state_->entry;
    }

    // Note: retrieving metadata doesn't cause cloud-backed files to be
    // downloaded, so this is safe to do on the UI thread.
    long long size = 0;
    time_t modTime = 0;
    if (entry == nullptr ||
        entry->path != path ||
        !getFileMetadata_(path, size, modTime) ||
        size != entry->size ||
        modTime != entry->modTime)
    {
        log_info("Voice keyer file %s not cached or changed on disk, reloading", path.c_str());
        preload(path);
        return nullptr;
    }

    static SF_VIRTUAL_IO virtualIo = {
        .get_filelen = MemGetFileLen_,
        .seek = MemSeek_,
        .read = MemRead_,
        .write = MemWrite_,
        .tell = MemTell_,
    };

    auto newReader = std::make_unique<VoiceKeyerMemoryReader>();
    newReader->data = entry->data;

    SNDFILE* file = sf_open_virtual(&virtualIo, SFM_READ, sfInfo, newReader.get());
    if (file != nullptr)
    {
        reader = std::move(newReader);
    }
    return file;
}

bool VoiceKeyerFileCache::getFileMetadata_(std::string const& path, long long& size, time_t& modTime)
{
    wxString wxPath(path);
    wxULongLong fileSize = wxFileName::GetSize(wxPath);
    if (fileSize == wxInvalidSize)
    {
        return false;
    }

    modTime = wxFileModificationTime(wxPath);
    if (modTime == (time_t)-1)
    {
        return false;
    }

    size = fileSize.GetValue();
    return true;
}

void VoiceKeyerFileCache::load_(std::shared_ptr<State> state, std::string path)
{
    auto entry = std::make_shared<Entry>();
    entry->path = path;

    if (!getFileMetadata_(path, entry->size, entry->modTime))
    {
        log_warn("Could not retrieve metadata for voice keyer file %s", path.c_str());
        return;
    }

    if (entry->size > MAX_CACHED_FILE_SIZE_BYTES)
    {
        log_info("Voice keyer file %s is too large to cache (%lld bytes)", path.c_str(), entry->size);
        return;
    }

    wxFile file;
    if (!file.Open(wxString(path)))
    {
        log_warn("Could not open voice keyer file %s for caching", path.c_str());
        return;
    }

    auto data = std::make_shared<std::vector<char>>(entry->size);
    if (file.Read(data->data(), data->size()) != (ssize_t)data->size())
    {
        log_warn("Could not read voice keyer file %s for caching", path.c_str());
        return;
    }
    entry->data = std::move(data);

    std::unique_lock<std::mutex> lk(state->mtx);
    if (state->requestedPath == path)
    {
        log_info("Cached voice keyer file %s (%lld bytes)", path.c_str(), entry->size);
        state->entry = std::move(entry);
    }
}
