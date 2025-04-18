//==========================================================================
// Name:            osx_interface.mm
//
// Purpose:         Provides C wrapper to needed Objective-C calls.
// Created:         Nov. 23, 2019
// Authors:         Mooneer Salem
// 
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#include <pthread.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>

#include "os_interface.h"
#include "../util/logging/ulog.h"

void VerifyMicrophonePermissions(std::promise<bool>& microphonePromise)
{
#ifndef APPLE_OLD_XCODE
    if (@available(macOS 10.14, *)) 
    {        
        // OSX >= 10.14: Request permission to access the camera and microphone.
        switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio])
        {
            case AVAuthorizationStatusAuthorized:
            {
                // The user has previously granted access to the microphone.
                microphonePromise.set_value(true);
                break;
            }
            case AVAuthorizationStatusNotDetermined:
            {
                // The app hasn't yet asked the user for microphone access.
                [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
                    microphonePromise.set_value(granted);
                }];

                break;
            }
            case AVAuthorizationStatusDenied:
            case AVAuthorizationStatusRestricted:
            {
                // The user has previously denied access or otherwise can't grant permissions.
                microphonePromise.set_value(false);
                break;
            }
            default:
            {
                // Other result not previously handled.
                assert(0);
            }
        }
    }
    else
    {
        microphonePromise.set_value(true);
    }
#else
    microphonePromise.set_value(true);
#endif // !APPLE_OLD_XCODE
}

void ResetMainWindowColorSpace()
{
    NSWindow* win = [NSApp mainWindow];
    CGColorSpaceRef cs = CGColorSpaceCreateWithName( kCGColorSpaceSRGB );
    [win setColorSpace:[[[NSColorSpace alloc]initWithCGColorSpace:cs] autorelease]];
    
    assert(cs != nullptr);
    CGColorSpaceRelease(cs);
}

std::string GetOperatingSystemString()
{
    return "macos";
}

void RequestRealTimeScheduling()
{
    // Adapted from Chromium project. May need to adjust parameters
    // depending on behavior.
    
    // Get current thread ID
    auto currentThreadId = pthread_mach_thread_np(pthread_self());
    
    // Increase thread priority to real-time.
    // Please note that the thread_policy_set() calls may fail in
    // rare cases if the kernel decides the system is under heavy load
    // and is unable to handle boosting the thread priority.
    // In these cases we just return early and go on with life.
    // Make thread fixed priority.
    thread_extended_policy_data_t policy;
    policy.timeshare = 0;  // Set to 1 for a non-fixed thread.
    kern_return_t result =
        thread_policy_set(currentThreadId,
                          THREAD_EXTENDED_POLICY,
                          reinterpret_cast<thread_policy_t>(&policy),
                          THREAD_EXTENDED_POLICY_COUNT);
    if (result != KERN_SUCCESS) 
    {
        log_warn("Could not set current thread to real-time: %d");
        return;
    }
    
    // Set to relatively high priority.
    thread_precedence_policy_data_t precedence;
    precedence.importance = 63;
    result = thread_policy_set(currentThreadId,
                               THREAD_PRECEDENCE_POLICY,
                               reinterpret_cast<thread_policy_t>(&precedence),
                               THREAD_PRECEDENCE_POLICY_COUNT);
    if (result != KERN_SUCCESS)
    {
        log_warn("Could not increase thread priority");
        return;
    }
    
    // Most important, set real-time constraints.
    // Define the guaranteed and max fraction of time for the audio thread.
    // These "duty cycle" values can range from 0 to 1.  A value of 0.5
    // means the scheduler would give half the time to the thread.
    // These values have empirically been found to yield good behavior.
    // Good means that audio performance is high and other threads won't starve.
    const double kGuaranteedAudioDutyCycle = 0.75;
    const double kMaxAudioDutyCycle = 0.85;
    
    // Define constants determining how much time the audio thread can
    // use in a given time quantum.  All times are in milliseconds.
    // About 512 frames @48KHz
    const double kTimeQuantum = 10.67;
    
    // Time guaranteed each quantum.
    const double kAudioTimeNeeded = kGuaranteedAudioDutyCycle * kTimeQuantum;
    
    // Maximum time each quantum.
    const double kMaxTimeAllowed = kMaxAudioDutyCycle * kTimeQuantum;
    
    // Get the conversion factor from milliseconds to absolute time
    // which is what the time-constraints call needs.
    mach_timebase_info_data_t tbInfo;
    mach_timebase_info(&tbInfo);
    double ms_to_abs_time =
        (static_cast<double>(tbInfo.denom) / tbInfo.numer) * 1000000;
    thread_time_constraint_policy_data_t timeConstraints;
    timeConstraints.period = kTimeQuantum * ms_to_abs_time;
    timeConstraints.computation = kAudioTimeNeeded * ms_to_abs_time;
    timeConstraints.constraint = kMaxTimeAllowed * ms_to_abs_time;
    timeConstraints.preemptible = 0;
    
    result =
        thread_policy_set(currentThreadId,
                          THREAD_TIME_CONSTRAINT_POLICY,
                          reinterpret_cast<thread_policy_t>(&timeConstraints),
                          THREAD_TIME_CONSTRAINT_POLICY_COUNT);
    if (result != KERN_SUCCESS)
    {
        log_warn("Could not set time constraint policy");
    }
    
    return;
}

void RequestNormalScheduling()
{
    // TBD
}