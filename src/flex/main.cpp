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
#include <sstream>
#include <cinttypes>
#include <stdlib.h>

#include "git_version.h"
#include "flex_defines.h"
#include "FlexVitaTask.h"
#include "FlexTcpTask.h"
#include "FlexTxRxThread.h"
#include "FlexRealtimeHelper.h"
#include "../pipeline/rade_text.h"
#include "../util/logging/ulog.h"
#include "../reporting/FreeDVReporter.h"
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

// FreeDV Reporter constants and helpers
#define SOFTWARE_NAME "freedv-flex"
std::string GetVersionString()
{
    std::stringstream ss;
    ss << SOFTWARE_NAME << " " << GetFreeDVVersion();
    return ss.str();
}
#define SOFTWARE_GRID_SQUARE "AA00"
#define MODE_STRING "RADEV1"

class ReportingController : public ThreadedObject
{
public:
    ReportingController()
        : reporterConnection_(nullptr)
        , currentGridSquare_(SOFTWARE_GRID_SQUARE)
        , radioCallsign_("")
        , userHidden_(true)
        , currentFreq_(0)
    {
        // empty
    }
    virtual ~ReportingController() = default;

    void updateRadioCallsign(std::string newCallsign);
    void updateRadioGridSquare(std::string newGridSquare);
    void reportCallsign(std::string callsign, char snr);
    void showSelf();
    void hideSelf();
    void changeFrequency(uint64_t freqHz);
    void transmit(bool transmit);

private:
    FreeDVReporter* reporterConnection_;
    std::string currentGridSquare_;
    std::string radioCallsign_;
    bool userHidden_;
    uint64_t currentFreq_;

    void updateReporterState_();
};

void ReportingController::transmit(bool transmit)
{
    enqueue_([&, transmit]() {
        log_info("Reporting TX state %d", (int)transmit);
        if (reporterConnection_ != nullptr)
        {
            reporterConnection_->transmit(MODE_STRING, transmit);
        }
    });
}

void ReportingController::changeFrequency(uint64_t freqHz)
{
    enqueue_([&, freqHz]() {
        log_info("Reporting frequency %" PRIu64, freqHz);
        if (reporterConnection_ != nullptr)
        {
            reporterConnection_->freqChange(freqHz);
        }
        currentFreq_ = freqHz;
    });
}

void ReportingController::showSelf()
{
    enqueue_([&]() {
        log_info("Showing ourselves on FreeDV Reporter");
        userHidden_ = false;
        updateReporterState_();
    });
}

void ReportingController::hideSelf()
{
    enqueue_([&]() {
        log_info("Hiding ourselves on FreeDV Reporter");
        userHidden_ = true;
        updateReporterState_();
    });
}

void ReportingController::updateReporterState_()
{
    if (reporterConnection_ != nullptr && userHidden_)
    {
        log_info("Disconnecting from FreeDV Reporter");
        delete reporterConnection_;
        reporterConnection_ = nullptr;
    }

    if (reporterConnection_ == nullptr && !userHidden_)
    {
        log_info("Connecting to FreeDV Reporter (callsign = %s, grid square = %s, version = %s)", radioCallsign_.c_str(), currentGridSquare_.c_str(), GetVersionString().c_str());
        reporterConnection_ = new FreeDVReporter("", radioCallsign_, currentGridSquare_, GetVersionString(), false, true);
        reporterConnection_->connect();
    }
}

void ReportingController::updateRadioCallsign(std::string newCallsign)
{
    enqueue_([&, newCallsign]() {
        log_info("Updating callsign to %s", newCallsign.c_str());
        bool changed = newCallsign != radioCallsign_;
        radioCallsign_ = newCallsign;
        if (changed)
        {
            if (reporterConnection_ != nullptr)
            {
                log_info("Disconnecting from FreeDV Reporter");
                delete reporterConnection_;
                reporterConnection_ = nullptr;
            }
            updateReporterState_();
        }
    });
}

void ReportingController::reportCallsign(std::string callsign, char snr)
{
    enqueue_([&, callsign, snr]() {
        log_info("Reporting RX callsign %s (SNR %d) to FreeDV Reporter", callsign.c_str(), (int)snr);
        reporterConnection_->addReceiveRecord(callsign, MODE_STRING, currentFreq_, snr);
    });
}

void ReportingController::updateRadioGridSquare(std::string newGridSquare)
{
    enqueue_([&]() {
        log_info("Grid square updated to %s", newGridSquare.c_str());
        bool changed = newGridSquare != currentGridSquare_;
        currentGridSquare_ = newGridSquare;
        if (changed)
        {
            if (reporterConnection_ != nullptr)
            {
                delete reporterConnection_;
                reporterConnection_ = nullptr;
            }
            updateReporterState_();
        }
    });
}

struct CallsignReporting
{
    ReportingController* reporter;
    FlexTcpTask* tcpTask;
    FlexTxRxThread* rxThread;
};

void ReportReceivedCallsign(rade_text_t rt, const char *txt_ptr, int length, void *state)
{
    CallsignReporting* reportObj = (CallsignReporting*)state;
    std::string callsign(txt_ptr, length);

    if (reportObj->reporter != nullptr)
    {
        reportObj->reporter->reportCallsign(callsign, reportObj->rxThread->getSnr());
    }

    reportObj->tcpTask->addSpot(callsign);
}

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
    
    log_info("Getting available radios from network");
    while (radioIp == "")
    {
        std::unique_lock<std::mutex> lk(radioMapMutex);
        if (radioList.size() == 0)
        {
            log_info("No radios found yet, sleeping 1s");
            lk.unlock();
            std::this_thread::sleep_for(1s);
            lk.lock();
            continue;
        }
        radioIp = radioList.begin()->first;
        enableRadioLookup = false;
    }
    
    log_info("Connecting to radio at IP %s", radioIp.c_str());
    FlexTcpTask tcpTask(vitaTask.getPort());

    CallsignReporting reportData;
    ReportingController reportController;
    reportData.tcpTask = &tcpTask;
    reportData.reporter = &reportController;
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

        reportController.updateRadioCallsign(callsign);
    }, nullptr);
    tcpTask.setWaveformGridSquareUpdateFn([&](FlexTcpTask&, std::string gridSquare, void*) {
        reportController.updateRadioGridSquare(gridSquare);
    }, nullptr);
    tcpTask.setWaveformConnectedFn([&](FlexTcpTask&, void*) {
        vitaTask.enableAudio(true);
        vitaTask.radioConnected(radioIp.c_str());
    }, nullptr);
    tcpTask.setWaveformUserConnectedFn([&](FlexTcpTask&, void*) {
        reportController.showSelf();
    }, nullptr);
    tcpTask.setWaveformUserDisconnectedFn([&](FlexTcpTask&, void*) {
        reportController.hideSelf();
    }, nullptr);
    tcpTask.setWaveformFreqChangeFn([&](FlexTcpTask&, uint64_t freq, void*) {
        reportController.changeFrequency(freq);
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
            reportController.transmit(txFlag);
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
