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
#include "../reporting/FreeDVReporter.h"

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

// FreeDV Reporter constants
#define SOFTWARE_NAME "freedv-flex 1.0-devel"
#define SOFTWARE_GRID_SQUARE "AA00"
#define MODE_STRING "RADEV1"

struct CallsignReporting
{
    FreeDVReporter** reporter;
    FlexTcpTask* tcpTask;
    FlexTxRxThread* rxThread;
};
uint64_t currentFreq;

void ReportReceivedCallsign(rade_text_t rt, const char *txt_ptr, int length, void *state)
{
    CallsignReporting* reportObj = (CallsignReporting*)state;
    std::string callsign(txt_ptr, length);

    if (*reportObj->reporter != nullptr)
    {
        (*reportObj->reporter)->addReceiveRecord(callsign, MODE_STRING, currentFreq, reportObj->rxThread->getSnr());
    }

    reportObj->tcpTask->addSpot(callsign);
}

int main(int argc, char** argv)
{
    FreeDVReporter* reporterConnection = nullptr;
    std::string radioCallsign;

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

    float zeros[320] = {0};
    float in_features[5*NB_TOTAL_FEATURES] = {0};
    fargan_init(&fargan);
    fargan_cont(&fargan, zeros, in_features);

    lpcnetEncState = lpcnet_encoder_create();
    assert(lpcnetEncState != nullptr);
    
    std::string radioIp = "";
    auto radioAddrEnv = getenv("SSDR_RADIO_ADDRESS");
    if (radioAddrEnv != nullptr)
    {
        radioIp = radioAddrEnv;
        log_info("Got radio address from environment, likely executing on radio itself");
    }

    // Start up VITA task so we can get the list of available radios.
    auto realtimeHelper = std::make_shared<FlexRealtimeHelper>();
    FlexVitaTask vitaTask(realtimeHelper, false /*radioIp != "" ? true : false*/); // TBD - our VITA port must be 4992 despite Flex documentation
    
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
    
    log_info("Sleeping while we get available radios");
    while (radioIp == "")
    {
        std::unique_lock<std::mutex> lk(radioMapMutex);
        if (radioList.size() == 0)
        {
            std::this_thread::sleep_for(1s);
            continue;
        }
        radioIp = radioList.begin()->first;
        enableRadioLookup = false;
    }
    
    log_info("Connecting to radio at IP %s", radioIp.c_str());
    FlexTcpTask tcpTask(vitaTask.getPort());

    CallsignReporting reportData;
    reportData.tcpTask = &tcpTask;
    reportData.reporter = &reporterConnection;
    reportData.rxThread = &rxThread;

    rade_text_set_rx_callback(radeTextPtr, &ReportReceivedCallsign, &reportData);

    tcpTask.setWaveformCallsignRxFn([&](FlexTcpTask&, std::string callsign, void*) {
        // Add callsign to EOO so others can report us
        log_info("Setting EOO bits");
        int nsyms = rade_n_eoo_bits(radeObj);
        float* eooSyms = new float[nsyms];
        assert(eooSyms);
                
        rade_text_generate_tx_string(radeTextPtr, callsign.c_str(), callsign.size(), eooSyms, nsyms);
        rade_tx_set_eoo_bits(radeObj, eooSyms);

        radioCallsign = callsign;
        if (reporterConnection != nullptr)
        {
            delete reporterConnection;
            reporterConnection = new FreeDVReporter("", radioCallsign, SOFTWARE_GRID_SQUARE, SOFTWARE_NAME, false, true);
            reporterConnection->connect();
        }
    }, nullptr);
    tcpTask.setWaveformConnectedFn([&](FlexTcpTask&, void*) {
        vitaTask.enableAudio(true);
        vitaTask.radioConnected(radioIp.c_str());
    }, nullptr);
    tcpTask.setWaveformUserConnectedFn([&](FlexTcpTask&, void*) {
        if (reporterConnection != nullptr)
        {
            delete reporterConnection;
        }
        reporterConnection = new FreeDVReporter("", radioCallsign, SOFTWARE_GRID_SQUARE, SOFTWARE_NAME, false, true);
        reporterConnection->connect();
    }, nullptr);
    tcpTask.setWaveformUserDisconnectedFn([&](FlexTcpTask&, void*) {
        if (reporterConnection != nullptr)
        {
            delete reporterConnection;
            reporterConnection = nullptr;
        }
    }, nullptr);
    tcpTask.setWaveformFreqChangeFn([&](FlexTcpTask&, uint64_t freq, void*) {
        if (reporterConnection != nullptr)
        {
            reporterConnection->freqChange(freq);
        }
        currentFreq = freq;
    }, nullptr);
    tcpTask.setWaveformTransmitFn([&](FlexTcpTask&, FlexTcpTask::TxState tx, void*) {
        if (tx == FlexTcpTask::ENDING_TX)
        {
            endingTx = true; // EOO should then queue out
            vitaTask.setEndingTx(true);
        }
        else
        {
            endingTx = false;
            bool txFlag = tx == FlexTcpTask::TRANSMITTING;
            vitaTask.setEndingTx(false);
            vitaTask.setTransmit(txFlag);
            g_tx.store(txFlag, std::memory_order_release);
            if (reporterConnection != nullptr)
            {
                reporterConnection->transmit(MODE_STRING, txFlag);
            }
        }
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
