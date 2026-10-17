//==========================================================================
// Name:            test_agent_osx.mm
// Purpose:         macOS-specific helpers for TestAgent
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
//  for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#import <Cocoa/Cocoa.h>

// Brings FreeDV to the front. None of its windows can become the active one
// while another application is in front, which is what a test script running
// it from a terminal leaves it as.
void TestAgentActivateApp()
{
    [NSApp activateIgnoringOtherApps:YES];
}
