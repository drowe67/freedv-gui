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

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#include "os_interface.h"

static id<NSObject> Activity = nil;

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
    [NSApp enumerateWindowsWithOptions:NSWindowListOrderedFrontToBack usingBlock:^(NSWindow *win, BOOL *stop) {
        CGColorSpaceRef cs = CGColorSpaceCreateWithName( kCGColorSpaceSRGB );
        [win setColorSpace:[[[NSColorSpace alloc]initWithCGColorSpace:cs] autorelease]];
    
        assert(cs != nullptr);
        CGColorSpaceRelease(cs);
    }];
}

void StartLowLatencyActivity()
{
    NSActivityOptions options = NSActivityBackground | NSActivityIdleSystemSleepDisabled | NSActivityLatencyCritical;

    Activity = [[NSProcessInfo processInfo] beginActivityWithOptions: options reason:@"FreeDV provides low latency audio processing and should not be inturrupted by system throttling."];
    [Activity retain];
}

void StopLowLatencyActivity()
{
    if (Activity)
    {
        [[NSProcessInfo processInfo] endActivity: Activity];
        [Activity release];
        Activity = nil;
    }
}

std::string GetOperatingSystemString()
{
    return "macos";
}
