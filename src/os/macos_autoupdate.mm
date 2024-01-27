//==========================================================================
// Name:            macos_autoupdate.mm
//
// Purpose:         Provides C wrapper to platform-specific autoupdate interface.
// Created:         Jan. 27, 2024
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

#include <cstdio>
#include "os_interface.h"
#import <Sparkle/Sparkle.h>

@interface AppUpdaterDelegate : NSObject <SPUUpdaterDelegate>

@property (nonatomic) SPUStandardUpdaterController *updaterController;

@end

@implementation AppUpdaterDelegate

- (void)updaterDidNotFindUpdate:(nonnull SPUUpdater *)updater
                          error:(nonnull NSError *)error
{
    NSString* str = [error localizedDescription];
    fprintf(stderr, "update error: %s\n", str.UTF8String);
    fprintf(stderr, "description: %s\n", error.userInfo.description.UTF8String);
}

@end

static AppUpdaterDelegate* _updaterDelegate;

// Autoupdate: initialize autoupdater.
void initializeAutoUpdate()
{
    _updaterDelegate = [[AppUpdaterDelegate alloc] init];
    _updaterDelegate.updaterController = [[SPUStandardUpdaterController alloc] initWithStartingUpdater:YES updaterDelegate:_updaterDelegate userDriverDelegate:nil];
}

// Autoupdate: cleanup autoupdater.
void cleanupAutoUpdate()
{
    [_updaterDelegate release];
}
