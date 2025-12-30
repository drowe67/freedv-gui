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

#include <getopt.h>

#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <sstream>
#include <cinttypes>
#include <stdlib.h>
#include <signal.h>

#include "flex_defines.h"
#include "FlexVitaTask.h"
#include "FlexTcpTask.h"
#include "../common/MinimalTxRxThread.h"
#include "../common/MinimalRealTimeHelper.h"
#include "../common/ReportingController.h"
#include "../pipeline/rade_text.h"
#include "../util/logging/ulog.h"
#include "../reporting/FreeDVReporter.h"

#include "rade_api.h"
#include "git_version.h"

extern "C" 
{
    #include "fargan_config_integ.h"
    #include "fargan.h"
    #include "lpcnet.h"
}

#define SOFTWARE_NAME "freedv-flex"

using namespace std::chrono_literals;

std::atomic<int> g_tx;
bool endingTx;
bool exitingApplication;

void OnSignalExit(int)
{
    // Infinite loop at bottom will check for this and trigger cleanup
    exitingApplication = true;
}

struct CallsignReporting
{
    ReportingController* reporter;
    FlexTcpTask* tcpTask;
    MinimalTxRxThread* rxThread;
};

void ReportReceivedCallsign(rade_text_t, const char *txt_ptr, int length, void *state)
{
    CallsignReporting* reportObj = (CallsignReporting*)state;

    if (txt_ptr != nullptr && length > 0)
    {
        std::string callsign(txt_ptr, length);

        if (reportObj->reporter != nullptr)
        {
            reportObj->reporter->reportCallsign(callsign, reportObj->rxThread->getSnr());
        }

        reportObj->tcpTask->addSpot(callsign);
    }
}

void printUsage(char* appName)
{
    log_info("Usage: %s [-d|--disable-reporting] [-l|--reporting-locator LOCATOR] [-m|--reporting-message MESSAGE] [-h|--help] [-v|--version]", appName);
    log_info("    -d|--disable-reporting: Disables FreeDV Reporter reporting.");
    log_info("    -l|--reporting-locator: Overrides grid square/locator from radio for FreeDV Reporter reporting.");
    log_info("    -m|--reporting-message: Sets reporting message for FreeDV Reporter reporting.");
    log_info("    -h|--help: This help message.");
    log_info("    -v|--version: Prints the application version and exits.");
    log_info("");
}

int main(int argc, char** argv)
{
    std::string stationGridSquare;
    std::string stationUserMessage;
    bool disableReporting = false;
    
    // Print version
    log_info("%s version %s", SOFTWARE_NAME, GetFreeDVVersion().c_str());
    
    // Check command line options
    static struct option longOptions[] = {
        {"disable-reporting",     no_argument,       0,  'd' },
        {"reporting-locator",     required_argument, 0,  'l' },
        {"reporting-message",     required_argument, 0,  'm' },
        {"help",                  no_argument,       0,  'h' },
        {"version",               no_argument,       0,  'v' },
        {0,         0,                 0,  0 }
    };
    constexpr char shortOptions[] = "dl:m:hv";
    
    int optionIndex = 0;
    int c = 0;
    while ((c = getopt_long(argc, argv, shortOptions, longOptions, &optionIndex)) != -1) // NOLINT
    {
        switch(c)
        {
            case 'd':
            {
                log_info("Disabling FreeDV Reporter reporting");
                disableReporting = true;
                break;
            }
            case 'm':
            {
                if (optarg == nullptr || strlen(optarg) == 0)
                {
                    log_error("Message required if specifying -m/--reporting-message.");
                    printUsage(argv[0]);
                    exit(-1); // NOLINT
                }
                log_info("Using '%s' for FreeDV Reporter user message", optarg);
                stationUserMessage = optarg;
                break;
            }
            case 'l':
            {
                if (optarg == nullptr || strlen(optarg) == 0)
                {
                    log_error("Grid square required if specifying -l/--reporting-locator.");
                    printUsage(argv[0]);
                    exit(-1); // NOLINT
                }
                log_info("Using grid square %s for FreeDV Reporter reporting", optarg);
                stationGridSquare = optarg;
                break;
            }
            case 'v':
            {
                // Version already printed above.
                exit(0); // NOLINT
            }
            case 'h':
            default:
            {
                printUsage(argv[0]);
                exit(-1); // NOLINT
            }
        }
    }
    
    // Environment setup -- make sure we don't use more threads than needed.
    // Prevents conflicts between numpy/OpenBLAS threading and Python/C++ threading,
    // improving performance.
    // NOLINTBEGIN
    setenv("OMP_NUM_THREADS", "1", 1);
    setenv("OPENBLAS_NUM_THREADS", "1", 1);
 
    // Enable maximum optimization for Python.
    setenv("PYTHONOPTIMIZE", "2", 1);
    // NOLINTEND

    // Initialize and start RADE.
    log_info("Initializing RADE library...");
    rade_initialize();

    // Override signal handlers. Needed to ensure we can properly
    // clean up the waveform if the user wants to exit.
    log_info("Setting up signal handlers...");
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &OnSignalExit;
    action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &action, nullptr);
    
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
    auto radioAddrEnv = getenv("SSDR_RADIO_ADDRESS"); // NOLINT
    if (radioAddrEnv != nullptr)
    {
        radioIp = radioAddrEnv;
        log_info("Got radio address from environment, likely executing on radio itself");
    }

    // Start up VITA task so we can get the list of available radios.
    auto realtimeHelper = std::make_shared<MinimalRealtimeHelper>();
    FlexVitaTask vitaTask(realtimeHelper);
    
    std::map<std::string, std::string> radioList;
    std::mutex radioMapMutex;
    bool enableRadioLookup = true;
    vitaTask.setOnRadioDiscoveredFn([&](FlexVitaTask&, std::string const& friendlyName, std::string const& ip, void*)
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
    MinimalTxRxThread txThread(true, FLEX_SAMPLE_RATE, FLEX_SAMPLE_RATE, realtimeHelper, radeObj, lpcnetEncState, &fargan, radeTextPtr, callbackObj);
    MinimalTxRxThread rxThread(false, FLEX_SAMPLE_RATE, FLEX_SAMPLE_RATE, realtimeHelper, radeObj, lpcnetEncState, &fargan, radeTextPtr, callbackObj);

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
    ReportingController reportController(SOFTWARE_NAME, false, disableReporting);
    reportData.tcpTask = &tcpTask;
    reportData.reporter = &reportController;
    reportData.rxThread = &rxThread;

    if (!disableReporting)
    {
        rade_text_set_rx_callback(radeTextPtr, &ReportReceivedCallsign, &reportData);
        if (stationGridSquare != "")
        {
            reportController.updateRadioGridSquare(stationGridSquare);
        }
    
        reportController.updateUserMessage(stationUserMessage);
    }
 
    // Set up reporting of actual receive state (prior to getting callsign).
    int rxCounter = 0;
    uint16_t meterMeterId = 0;
    ThreadedTimer rxNoCallsignReporting(100, [&](ThreadedTimer&) {
        if (rxThread.getSync() && !reportController.isHidden())
        {
            rxCounter = (rxCounter + 1) % 10;
            auto snr = rxThread.getSnr();
            if (rxCounter == 0)
            {
                reportController.reportCallsign("", snr);
            }
            vitaTask.sendMeter(meterMeterId, snr);
        }
        else
        {
            vitaTask.sendMeter(meterMeterId, -99);
        }        
    }, true);
    if (!disableReporting)
    {
        rxNoCallsignReporting.start();
    }
    
    tcpTask.setWaveformSnrMeterIdentifiersFn([&](FlexTcpTask&, uint16_t meterId, void*) {
        meterMeterId = meterId;
    }, nullptr);

    tcpTask.setWaveformCallsignRxFn([&](FlexTcpTask&, std::string const& callsign, void*) {
        // Add callsign to EOO so others can report us
        log_info("Setting EOO bits");
        int nsyms = rade_n_eoo_bits(radeObj);
        float* eooSyms = new float[nsyms];
        assert(eooSyms);
                
        rade_text_generate_tx_string(radeTextPtr, callsign.c_str(), callsign.size(), eooSyms, nsyms);
        rade_tx_set_eoo_bits(radeObj, eooSyms);

        reportController.updateRadioCallsign(callsign);
    }, nullptr);
    tcpTask.setWaveformGridSquareUpdateFn([&](FlexTcpTask&, std::string const& gridSquare, void*) {
        if (stationGridSquare == "")
        {
            reportController.updateRadioGridSquare(gridSquare);
        }
    }, nullptr);
    tcpTask.setWaveformConnectedFn([&](FlexTcpTask&, void*) {
        vitaTask.clearStreamIds();
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
    tcpTask.setWaveformAddValidStreamIdentifiersFn([&](FlexTcpTask&, uint32_t txInStreamId, uint32_t txOutStreamId, uint32_t rxInStreamId, uint32_t rxOutStreamId, void*) {
        vitaTask.registerStreamIds(txInStreamId, txOutStreamId, rxInStreamId, rxOutStreamId);
    }, nullptr);

    tcpTask.connect(radioIp.c_str(), FLEX_TCP_PORT, true);
        
    while (!exitingApplication)
    {
        std::this_thread::sleep_for(100ms);
    }
    
    // Stop TX/RX threads. Needed to prevent RADE calls after finalize.
    txThread.stop();
    rxThread.stop();

    // Clean up RADE
    rade_close(radeObj);
    rade_finalize();
    return 0;
}
