//=========================================================================
// Name:            main.cpp
// Purpose:         Main entry point for Flex waveform.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANYg_eoo_enqueued
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#include <chrono>
#include <thread>
#include <atomic>
#include "FlexVitaTask.h"
#include "../util/logging/ulog.h"

using namespace std::chrono_literals;

std::atomic<int> g_tx;
bool endingTx;

int main(int argc, char** argv)
{
    FlexVitaTask vitaTask;
    
    vitaTask.setOnRadioDiscoveredFn([](FlexVitaTask&, std::string friendlyName, std::string ip, void* state)
    {
        log_info("Got discovery callback (radio %s, IP %s)", friendlyName.c_str(), ip.c_str());
    }, nullptr);
    
    std::this_thread::sleep_for(3600s);
    
    // Add discovery callback so that we can be aware of radios on the network.
    // TBD 
    return 0;
}