//==========================================================================
// Name:            osx_interface.mm
//
// Purpose:         Provides C wrapper to needed Objective-C calls.
// Created:         Nov. 23, 2019
// Authors:         Mooneer Salem
// 
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
//==========================================================================

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>

#include <pthread.h>

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
    [NSApp enumerateWindowsWithOptions:NSWindowListOrderedFrontToBack usingBlock:^(NSWindow *win, BOOL *) {       
        NSColorSpace* colorSpace = win.colorSpace;
        CFStringRef colorSpaceName = CGColorSpaceCopyName(colorSpace.CGColorSpace);
        bool recreate = colorSpaceName == nil || CFStringCompare(colorSpaceName, kCGColorSpaceSRGB, 0) != kCFCompareEqualTo;
        if (colorSpaceName != nil)
        {
            CFRelease(colorSpaceName);
        }

        if (recreate)
        {
            CGColorSpaceRef cs = CGColorSpaceCreateWithName( kCGColorSpaceSRGB );
            assert(cs != nullptr);

            [win setColorSpace:[[[NSColorSpace alloc]initWithCGColorSpace:cs] autorelease]];
            CGColorSpaceRelease(cs);
        }
    }];
}

void StartLowLatencyActivity()
{
    NSActivityOptions options = NSActivityUserInitiated | NSActivityIdleSystemSleepDisabled | NSActivityLatencyCritical;

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

void SetThreadName(std::string const& name)
{
    std::string fullName = "FDV ";
    fullName += name;
    pthread_setname_np(fullName.c_str());
}
