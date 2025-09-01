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
#include <stdlib.h>

#include "flex_defines.h"
#include "FlexVitaTask.h"
#include "FlexTcpTask.h"
#include "FlexTxRxThread.h"
#include "FlexRealtimeHelper.h"
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
    // Environment setup -- make sure we don't use more threads than needed.
    // Prevents conflicts between numpy/OpenBLAS threading and Python/C++ threading,
    // improving performance.
    setenv("OMP_NUM_THREADS", "1", 1);
    setenv("OPENBLAS_NUM_THREADS", "1", 1);
 
    // Enable maximum optimization for Python.
    setenv("PYTHONOPTIMIZE", "2", 1);
    
    // Initialize and start RADE.
    log_info("Initializing RADE library...");
    rade_initialize();
    
    log_info("Creating RADE object");
    char modelFile[1];
    modelFile[0] = 0;
    struct rade* radeObj = rade_open(modelFile, RADE_USE_C_ENCODER | RADE_USE_C_DECODER | RADE_VERBOSE_0);
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
    bool enableRadioLookup = true;
    vitaTask.setOnRadioDiscoveredFn([&](FlexVitaTask&, std::string friendlyName, std::string ip, void* state)
    {
        std::unique_lock<std::mutex> lk(radioMapMutex);
        if (enableRadioLookup)
        {
            log_info("Got discovery callback (radio %s, IP %s)", friendlyName.c_str(), ip.c_str());
            radioList[ip] = friendlyName;
        }
    }, nullptr);
    
    // Initialize audio pipelines
    auto realtimeHelper = std::make_shared<FlexRealtimeHelper>();
    auto callbackObj = vitaTask.getCallbackData();
    FlexTxRxThread txThread(true, FLEX_SAMPLE_RATE, FLEX_SAMPLE_RATE, realtimeHelper, radeObj, lpcnetEncState, &fargan, radeTextPtr, callbackObj);
    FlexTxRxThread rxThread(false, FLEX_SAMPLE_RATE, FLEX_SAMPLE_RATE, realtimeHelper, radeObj, lpcnetEncState, &fargan, radeTextPtr, callbackObj);
    
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
        enableRadioLookup = false;
    }
    
    log_info("Connecting to radio at IP %s", radioIp.c_str());
    FlexTcpTask tcpTask;
    tcpTask.setWaveformConnectedFn([&](FlexTcpTask&, void*) {
        vitaTask.enableAudio(true);
        vitaTask.radioConnected(radioIp.c_str());
    }, nullptr);
    tcpTask.setWaveformTransmitFn([&](FlexTcpTask&, bool tx, void*) {
        vitaTask.setTransmit(tx);
        g_tx.store(tx, std::memory_order_release);
    }, nullptr);
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