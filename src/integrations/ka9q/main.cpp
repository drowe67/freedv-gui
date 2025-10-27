//=========================================================================
// Name:            main.cpp
// Purpose:         Main entry point for KA9Q integration.
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

#ifdef _WIN32
// For _setmode().
#include <io.h>
#include <fcntl.h>
#endif // _WIN32

#if defined(USING_MIMALLOC)
#include <mimalloc.h>
#endif // defined(USING_MIMALLOC)

#include "../common/MinimalTxRxThread.h"
#include "../common/MinimalRealTimeHelper.h"
#include "../common/ReportingController.h"
#include "../pipeline/rade_text.h"
#include "../util/logging/ulog.h"
#include "../reporting/FreeDVReporter.h"
#include "../pipeline/paCallbackData.h"
#include "../../util/ThreadedTimer.h"
#include "../../src/pipeline/pipeline_defines.h"

#include "rade_api.h"

extern "C" 
{
    #include "fargan_config.h"
    #include "fargan.h"
    #include "lpcnet.h"
}

constexpr int INPUT_SAMPLE_RATE = 8000;
constexpr int OUTPUT_SAMPLE_RATE = 16000;

#define SOFTWARE_NAME "freedv-ka9q"
#define FIFO_SIZE_SAMPLES (FIFO_SIZE * OUTPUT_SAMPLE_RATE / 1000)

using namespace std::chrono_literals;

std::atomic<int> g_tx;
bool endingTx;

struct CallsignReporting
{
    ReportingController* reporter;
    MinimalTxRxThread* rxThread;
};

void ReportReceivedCallsign(rade_text_t rt, const char *txt_ptr, int length, void *state)
{
    CallsignReporting* reportObj = (CallsignReporting*)state;

    if (txt_ptr != nullptr && length > 0)
    {
        std::string callsign(txt_ptr, length);

        if (reportObj->reporter != nullptr)
        {
            reportObj->reporter->reportCallsign(callsign, reportObj->rxThread->getSnr());
        }
    }
}

int main(int argc, char** argv)
{
    std::string stationCallsign;
    std::string stationGridSquare;
    int64_t stationFrequency = 0;

#if defined(USING_MIMALLOC)
    // Decrease purge interval to 100ms to improve performance (default = 10ms).
    mi_option_set(mi_option_purge_delay, 100);
    mi_option_set(mi_option_purge_extend_delay, 10);
    //mi_option_enable(mi_option_verbose);
#endif // defined(USING_MIMALLOC)

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
    
    // Set up RT handler
    auto realtimeHelper = std::make_shared<MinimalRealtimeHelper>();
    
    // Initialize audio FIFOs
    std::unique_ptr<paCallBackData> callbackObj = std::make_unique<paCallBackData>();
    assert(callbackObj != nullptr);

    callbackObj->infifo1 = nullptr;
    callbackObj->infifo2 = new GenericFIFO<short>(FIFO_SIZE_SAMPLES);
    assert(callbackObj->infifo2 != nullptr);
    callbackObj->outfifo1 = nullptr;
    callbackObj->outfifo2 = new GenericFIFO<short>(FIFO_SIZE_SAMPLES);
    assert(callbackObj->outfifo2 != nullptr);

    // Initialize RX thread
    MinimalTxRxThread rxThread(false, INPUT_SAMPLE_RATE, OUTPUT_SAMPLE_RATE, realtimeHelper, radeObj, lpcnetEncState, &fargan, radeTextPtr, callbackObj.get());

    log_info("Starting RX thread");
    rxThread.start();
    
    log_info("Synchronizing startup of RX thread");
    rxThread.waitForReady();
    rxThread.signalToStart();
    
    CallsignReporting reportData;
    ReportingController reportController(SOFTWARE_NAME);
    reportData.reporter = &reportController;
    reportData.rxThread = &rxThread;

    rade_text_set_rx_callback(radeTextPtr, &ReportReceivedCallsign, &reportData);

    // Set up reporting of actual receive state (prior to getting callsign).
    int rxCounter = 0;    
    ThreadedTimer rxNoCallsignReporting(100, [&](ThreadedTimer&) {
        if (rxThread.getSync())
        {
            rxCounter = (rxCounter + 1) % 10;
            if (rxCounter == 0)
            {
                reportController.reportCallsign("", rxThread.getSnr());
            }
        }
    }, true);
    rxNoCallsignReporting.start();

    bool reporterValid = stationCallsign != "" && stationGridSquare != "" && stationFrequency != 0;
    if (reporterValid)
    {
        reportController.updateRadioCallsign(stationCallsign);
        reportController.updateRadioGridSquare(stationGridSquare);
        reportController.changeFrequency(stationFrequency);
        reportController.showSelf();
    }

#ifdef _WIN32
    // Note: freopen() returns NULL if filename is NULL, so
    // we have to use setmode() to make it a binary stream instead.
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
#endif // _WIN32

    bool exiting = false; 
    while (!exiting)
    {
        constexpr int NUM_TO_READ = (FRAME_DURATION_MS * INPUT_SAMPLE_RATE / 1000);
        constexpr int NUM_TO_WRITE = (FRAME_DURATION_MS * OUTPUT_SAMPLE_RATE / 1000);
        if (callbackObj->infifo2->numFree() >= NUM_TO_READ)
        {
            // Read from standard input and queue on input FIFO
            short readBuffer[NUM_TO_READ];
            int numActuallyRead = fread(readBuffer, sizeof(short), NUM_TO_READ, stdin);
            if (numActuallyRead <= 0) 
            {
                // stdin closed, exit
                exiting = true;
                break;
            }
            callbackObj->infifo2->write(readBuffer, numActuallyRead);
        }

        // If we have anything decoded, output that now
        while (!exiting && callbackObj->outfifo2->numUsed() >= NUM_TO_WRITE)
        {
            short writeBuffer[NUM_TO_WRITE];
            callbackObj->outfifo2->read(writeBuffer, NUM_TO_WRITE);
            int numActuallyWritten = fwrite(writeBuffer, sizeof(short), NUM_TO_WRITE, stdout);
            if (numActuallyWritten <= 0)
            {
                // stdout closed, exit
                exiting = true;
                break;
            }
        }

        if (!exiting)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DURATION_MS));
        }
    }
    
    // TBD - the below isn't called since we're in an infinite loop above.
    rade_close(radeObj);
    rade_finalize();
    return 0;
}
