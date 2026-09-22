//=========================================================================
// Name:            HeardStationList.h
// Purpose:         Tracks stations decoded on the text messaging channel.
//
// Authors:         FreeDV text messaging contributors
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================

#ifndef TEXT_MESSAGING__HEARD_STATION_LIST_H
#define TEXT_MESSAGING__HEARD_STATION_LIST_H

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "TextMessagingTypes.h"

namespace TextMessaging
{

// Stations we have decoded a frame from, most recently heard first. Entries
// age out so that the list shows who is on the channel now rather than who was
// on it this morning. A station the operator pinned by hand is the exception:
// it stays until removed, whether or not it has ever been heard.
class HeardStationList
{
public:
    // Default retention, matching how long a quiet station stays interesting
    // to somebody deciding who to call.
    static constexpr std::time_t DEFAULT_MAX_AGE_SECONDS = 60 * 60;

    explicit HeardStationList(std::time_t maxAgeSeconds = DEFAULT_MAX_AGE_SECONDS);

    // Records a decode. Returns true if this added a station the list did not
    // already have, which is what the GUI uses to decide whether the selection
    // it is holding can stay put. A pinned station stays pinned.
    bool heard(const std::string& callsign, float snr, std::time_t when);

    // Adds a station the operator typed in, or pins one already listed. Pass
    // the callsign normalized, since that is the name the row will carry.
    // Returns false if nothing usable is left of the callsign.
    bool pin(const std::string& callsign);

    // Drops a station, heard or pinned. Returns false if it was not listed.
    bool remove(const std::string& callsign);

    // Copies a station's entry out. Returns false if it is not listed.
    bool find(const std::string& callsign, HeardStation& stationOut) const;

    // Replaces the heard entries, used to restore the list from the message
    // store at startup. Entries already older than the retention window are
    // dropped. Pinned entries are kept; a restored decode of one fills in its
    // SNR and last heard time.
    void restore(const std::vector<HeardStation>& stations, std::time_t now);

    // Drops unpinned entries last heard before now - maxAgeSeconds. Returns
    // the number removed so a caller can skip a redraw when nothing changed.
    int prune(std::time_t now);

    // Most recently heard first; pinned stations never heard come last.
    std::vector<HeardStation> stations() const;

    bool contains(const std::string& callsign) const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::map<std::string, HeardStation> stations_;
    std::time_t maxAgeSeconds_;
};

} // namespace TextMessaging

#endif // TEXT_MESSAGING__HEARD_STATION_LIST_H
