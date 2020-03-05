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
#include <mutex>
#include <condition_variable>
#include "osx_interface.h"

static std::mutex osx_permissions_mutex;
static std::condition_variable osx_permissions_condvar;
static bool globalHasAccess = false;

bool VerifyMicrophonePermissions()
{
    bool hasAccess = true;
#ifndef APPLE_OLD_XCODE
    if (@available(macOS 10.14, *)) {
        // OSX >= 10.14: Request permission to access the camera and microphone.
        switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio])
        {
            case AVAuthorizationStatusAuthorized:
            {
                // The user has previously granted access to the camera.
                break;
            }
            case AVAuthorizationStatusNotDetermined:
            {
                // The app hasn't yet asked the user for camera access.
                // Note that this call is asynchronous but we need to wait for a response before
                // proceeding. TBD for improvement.
                [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
                    std::unique_lock<std::mutex> lk(osx_permissions_mutex);
                    globalHasAccess = granted;
                    osx_permissions_condvar.notify_one();
                }];

                {
                    std::unique_lock<std::mutex> lk(osx_permissions_mutex);
                    osx_permissions_condvar.wait(lk);
                    hasAccess = globalHasAccess;
                }
                break;
            }
            case AVAuthorizationStatusDenied:
            case AVAuthorizationStatusRestricted:
            {
                // The user has previously denied access or otherwise can't grant permissions.
                hasAccess = false;
                break;
            }
            default:
            {
                // Other result not previously handled.
                assert(0);
            }
        }
    }
#endif // !APPLE_OLD_XCODE

    return hasAccess;
}

