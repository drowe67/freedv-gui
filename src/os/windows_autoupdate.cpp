//==========================================================================
// Name:            windows_autoupdate.cpp
//
// Purpose:         Provides C wrapper to platform-specific autoupdate interface.
// Created:         Jan. 26, 2024
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

#include "os_interface.h"
#include <winsparkle.h>

// Autoupdate: initialize autoupdater.
void initializeAutoUpdate()
{
    win_sparkle_set_appcast_url(FREEDV_APPCAST_URL);
    win_sparkle_init();
}

// Autoupdate: cleanup autoupdater.
void cleanupAutoUpdate()
{
    win_sparkle_cleanup();
}
