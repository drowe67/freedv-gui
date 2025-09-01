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

#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

#include "flex_defines.h"
#include "FlexVitaTask.h"
#include "FlexTcpTask.h"
#include "FlexTxRxThread.h"
#include "../pipeline/rade_text.h"
#include "../util/logging/ulog.h"

#include "rade_api.h"

extern "C" 
{
    #include "fargan_config.h"
    #include "fargan.h"
    #include "lpcnet.h"
}

using namespace std::chrono_literals;

std::atomic<int> g_tx;
bool endingTx;

int main(int argc, char** argv)
{
    // Initialize and start RADE.
    log_info("Initializing RADE library...");
    rade_initialize();
    
    log_info("Creating RADE object");
    char modelFile[1];
    modelFile[0] = 0;
    struct rade* radeObj = rade_open(modelFile, RADE_USE_C_ENCODER | RADE_USE_C_DECODER);
    assert(radeObj != nullptr);
    
    log_info("Creating RADE text object");
    auto radeTextPtr = rade_text_create();
    assert(radeTextPtr != nullptr);
    
    log_info("Creating FARGAN objects");
    FARGANState fargan;
    LPCNetEncState *lpcnetEncState = nullptr;

    // TBD - reporting
    //rade_text_set_rx_callback(radeTextPtr_, &FreeDVInterface::OnRadeTextRx_, this);

    float zeros[320] = {0};
    float in_features[5*NB_TOTAL_FEATURES] = {0};
    fargan_init(&fargan);
    fargan_cont(&fargan, zeros, in_features);

    lpcnetEncState = lpcnet_encoder_create();
    assert(lpcnetEncState != nullptr);
    
    // Start up VITA task so we can get the list of available radios.
    FlexVitaTask vitaTask;
    
    std::map<std::string, std::string> radioList;
    std::mutex radioMapMutex;
    vitaTask.setOnRadioDiscoveredFn([&](FlexVitaTask&, std::string friendlyName, std::string ip, void* state)
    {
        std::unique_lock<std::mutex> lk(radioMapMutex);
        log_info("Got discovery callback (radio %s, IP %s)", friendlyName.c_str(), ip.c_str());
        radioList[ip] = friendlyName;
    }, nullptr);
    
    // Initialize audio pipelines
    auto callbackObj = vitaTask.getCallbackData();
    FlexTxRxThread txThread(true, FLEX_SAMPLE_RATE, FLEX_SAMPLE_RATE, nullptr, radeObj, lpcnetEncState, &fargan, radeTextPtr, callbackObj);
    FlexTxRxThread rxThread(false, FLEX_SAMPLE_RATE, FLEX_SAMPLE_RATE, nullptr, radeObj, lpcnetEncState, &fargan, radeTextPtr, callbackObj);
    
    log_info("Starting TX/RX threads");
    txThread.start();
    rxThread.start();
    
    log_info("Synchronizing startup of TX/RX threads");
    txThread.waitForReady();
    rxThread.waitForReady();
    txThread.signalToStart();
    rxThread.signalToStart();
    
    log_info("Sleeping 2 seconds to get available radios");
    std::this_thread::sleep_for(2s);
    
    std::string radioIp;
    {
        std::unique_lock<std::mutex> lk(radioMapMutex);
        radioIp = radioList.begin()->first;
    }
    
    log_info("Connecting to radio at IP %s", radioIp.c_str());
    FlexTcpTask tcpTask;
    tcpTask.connect(radioIp.c_str(), FLEX_TCP_PORT, true);
        
    for(;;)
    {
        std::this_thread::sleep_for(3600s);
    }
    
    // TBD - the below isn't called since we're in an infinite loop above.
    rade_close(radeObj);
    rade_finalize();
    return 0;
}