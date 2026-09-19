//=========================================================================
// Name:            HeardStationList.cpp
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

#include "HeardStationList.h"

#include <algorithm>

#include "FrameCodec.h"

namespace TextMessaging
{

constexpr std::time_t HeardStationList::DEFAULT_MAX_AGE_SECONDS;

HeardStationList::HeardStationList(std::time_t maxAgeSeconds)
    : maxAgeSeconds_(maxAgeSeconds)
{
    // empty
}

bool HeardStationList::heard(const std::string& callsign, float snr, std::time_t when)
{
    std::string normalized = FrameCodec::normalizeCallsign(callsign);
    if (normalized.empty()) return false;

    std::lock_guard<std::mutex> lock(mutex_);

    auto existing = stations_.find(normalized);
    if (existing != stations_.end())
    {
        // An older decode can arrive after a newer one only if the caller
        // passes a stale timestamp; keep the newest either way.
        if (when >= existing->second.lastHeard)
        {
            existing->second.snr = snr;
            existing->second.lastHeard = when;
        }
        return false;
    }

    HeardStation station;
    station.callsign = normalized;
    station.snr = snr;
    station.lastHeard = when;
    stations_[normalized] = station;

    return true;
}

void HeardStationList::restore(const std::vector<HeardStation>& stations, std::time_t now)
{
    std::lock_guard<std::mutex> lock(mutex_);

    stations_.clear();
    for (const HeardStation& station : stations)
    {
        std::string normalized = FrameCodec::normalizeCallsign(station.callsign);
        if (normalized.empty()) continue;
        if (station.lastHeard < now - maxAgeSeconds_) continue;

        HeardStation copy = station;
        copy.callsign = normalized;
        stations_[normalized] = copy;
    }
}

int HeardStationList::prune(std::time_t now)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::time_t cutoff = now - maxAgeSeconds_;
    int removed = 0;

    for (auto it = stations_.begin(); it != stations_.end();)
    {
        if (it->second.lastHeard < cutoff)
        {
            it = stations_.erase(it);
            removed++;
        }
        else
        {
            ++it;
        }
    }

    return removed;
}

std::vector<HeardStation> HeardStationList::stations() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<HeardStation> result;
    result.reserve(stations_.size());
    for (const auto& entry : stations_) result.push_back(entry.second);

    std::sort(result.begin(), result.end(),
              [](const HeardStation& left, const HeardStation& right)
              {
                  if (left.lastHeard != right.lastHeard) return left.lastHeard > right.lastHeard;
                  return left.callsign < right.callsign;
              });

    return result;
}

bool HeardStationList::contains(const std::string& callsign) const
{
    std::string normalized = FrameCodec::normalizeCallsign(callsign);

    std::lock_guard<std::mutex> lock(mutex_);
    return stations_.find(normalized) != stations_.end();
}

void HeardStationList::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    stations_.clear();
}

} // namespace TextMessaging
