//=========================================================================
// Name:            IRigFrequencyController.h
// Purpose:         Interface for control of frequency/mode on radios.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#ifndef I_RIG_FREQUENCY_CONTROLLER_H
#define I_RIG_FREQUENCY_CONTROLLER_H

#include <atomic>
#include <chrono>
#include <cstdint>

#include "IRigController.h"

class IRigFrequencyController : virtual public IRigController
{
public:
    enum Mode
    {
        UNKNOWN,
        
        USB,
        DIGU,
        LSB,
        DIGL,
        FM,
        DIGFM,
        AM,
    };
    
    virtual ~IRigFrequencyController() = default;
    
    // Event handlers that are called by implementers of this interface.
    EventHandler<IRigFrequencyController*, uint64_t, Mode> onFreqModeChange;
    
    virtual void setFrequency(uint64_t frequency) = 0;
    virtual void setMode(Mode mode) = 0;
    virtual void requestCurrentFrequencyMode() = 0;

    // Whether a frequency reported by onFreqModeChange predates the one last
    // asked for with setFrequency(): it was read from the radio before that
    // change was made, and so says nothing about where the radio is now.
    // That's taken to be so until the radio reports the frequency asked for,
    // or for a few seconds at most (in case it can't be tuned exactly there,
    // or is tuned elsewhere meanwhile).
    bool isFrequencyChangePending(uint64_t reportedFrequency)
    {
        auto requested = requestedFrequency_.load(std::memory_order_acquire);
        if (requested == 0)
        {
            return false;
        }

        auto age = nowMs_() - requestedAtMs_.load(std::memory_order_acquire);
        if (reportedFrequency == requested || age > PENDING_FREQUENCY_TIMEOUT_MS)
        {
            requestedFrequency_.compare_exchange_strong(requested, 0, std::memory_order_acq_rel);
            return false;
        }
        return true;
    }

protected:
    IRigFrequencyController() = default;

    // For implementers' setFrequency().
    void noteRequestedFrequency_(uint64_t frequency)
    {
        requestedAtMs_.store(nowMs_(), std::memory_order_release);
        requestedFrequency_.store(frequency, std::memory_order_release);
    }

private:
    static constexpr int64_t PENDING_FREQUENCY_TIMEOUT_MS = 3000;

    static int64_t nowMs_()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::atomic<uint64_t> requestedFrequency_{0};
    std::atomic<int64_t> requestedAtMs_{0};
};

#endif // I_RIG_FREQUENCY_CONTROLLER_H