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

#include <getopt.h>
#include <inttypes.h>
#include <unistd.h>

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

#include "../common/MinimalTxRxThread.h"
#include "../common/MinimalRealTimeHelper.h"
#include "../common/ReportingController.h"
#include "../pipeline/rade_text.h"
#include "../util/logging/ulog.h"
#include "../reporting/FreeDVReporter.h"
#include "../pipeline/paCallbackData.h"
#include "../../util/ThreadedTimer.h"
#include "../../src/pipeline/pipeline_defines.h"

#include "git_version.h"

#include "rade_api.h"

extern "C" 
{
    #include "fargan_config_integ.h"
    #include "fargan.h"
    #include "lpcnet.h"
}

constexpr int DEFAULT_INPUT_SAMPLE_RATE = 8000;
constexpr int DEFAULT_OUTPUT_SAMPLE_RATE = 16000;

#define SOFTWARE_NAME "freedv-ka9q"
#define INPUT_FIFO_SIZE_SAMPLES (FIFO_SIZE * inputSampleRate / 1000)
#define OUTPUT_FIFO_SIZE_SAMPLES (FIFO_SIZE * outputSampleRate / 1000)

using namespace std::chrono_literals;

std::atomic<int> g_tx;
bool endingTx;

struct CallsignReporting
{
    ReportingController* reporter;
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
    }
}

void printUsage(char* appName)
{
    log_info("Usage: %s [-i|--input-sample-rate RATE] [-o|--output-sample-rate RATE] [-c|--reporting-callsign CALLSIGN] [-l|--reporting-locator LOCATOR] [-f|--reporting-freq-hz FREQUENCY_IN_HERTZ] [-h|--help] [-v|--version]", appName);
    log_info("    -i|--input-sample-rate: The sample rate for int16 audio samples received over standard input (default %d Hz).", DEFAULT_INPUT_SAMPLE_RATE);
    log_info("    -o|--output-sample-rate: The sample rate for int16 audio samples output over standard output (default %d Hz).", DEFAULT_OUTPUT_SAMPLE_RATE);
    log_info("    -c|--reporting-callsign: The callsign to use for FreeDV Reporter reporting.");
    log_info("    -l|--reporting-locator: The grid square/locator to use for FreeDV Reporter reporting.");
    log_info("    -f|--reporting-frequency-hz: The frequency to report for FreeDV Reporter reporting, in hertz. (Example: 14236000 for 14.236MHz)");
    log_info("    -h|--help: This help message.");
    log_info("    -v|--version: Prints the application version and exits.");
    log_info("");
    log_info("Note: Callsign, locator and frequency must all be provided for the application to connect to FreeDV Reporter.");
}

int main(int argc, char** argv)
{
    int inputSampleRate = DEFAULT_INPUT_SAMPLE_RATE;
    int outputSampleRate = DEFAULT_OUTPUT_SAMPLE_RATE;
    
    std::string stationCallsign;
    std::string stationGridSquare;
    int64_t stationFrequency = 0;

    // Environment setup -- make sure we don't use more threads than needed.
    // Prevents conflicts between numpy/OpenBLAS threading and Python/C++ threading,
    // improving performance.
    // NOLINTBEGIN
    setenv("OMP_NUM_THREADS", "1", 1);
    setenv("OPENBLAS_NUM_THREADS", "1", 1);
 
    // Enable maximum optimization for Python.
    setenv("PYTHONOPTIMIZE", "2", 1);

    // Enable Python JIT (if version of Python supports it).
    setenv("PYTHON_JIT", "1", 1);
    // NOLINTEND

    // Print version
    log_info("%s version %s", SOFTWARE_NAME, GetFreeDVVersion().c_str());
    
    // Check command line options
    static struct option longOptions[] = {
        {"input-sample-rate",     required_argument, 0,  'i' },
        {"output-sample-rate",    required_argument, 0,  'o' },
        {"reporting-callsign",    required_argument, 0,  'c' },
        {"reporting-locator",     required_argument, 0,  'l' },
        {"reporting-freq-hz",     required_argument, 0,  'f' },
        {"help",                  no_argument,       0,  'h' },
        {"version",               no_argument,       0,  'v' },
        {0,         0,                 0,  0 }
    };
    constexpr char shortOptions[] = "i:o:c:l:f:hv";
    
    int optionIndex = 0;
    int c = 0;
    while ((c = getopt_long(argc, argv, shortOptions, longOptions, &optionIndex)) != -1) // NOLINT
    {
        switch(c)
        {
            case 'i':
            case 'o':
            {
                int tmpSampleRate = atoi(optarg);
                if (tmpSampleRate <= 0)
                {
                    log_error("Invalid sample rate provided (%s)", optarg);
                    printUsage(argv[0]);
                    exit(-1); // NOLINT
                }
                if (c == 'i')
                {
                    inputSampleRate = tmpSampleRate;
                }
                else
                {
                    outputSampleRate = tmpSampleRate;
                }
                break;
            }
            case 'c':
            {
                if (optarg == nullptr || strlen(optarg) == 0)
                {
                    log_error("Callsign required if specifying -c/--reporting-callsign.");
                    printUsage(argv[0]);
                    exit(-1); // NOLINT
                }
                log_info("Using callsign %s for FreeDV Reporter reporting", optarg);
                stationCallsign = optarg;
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
            case 'f':
            {
                int64_t tmpFrequency = atoll(optarg);
                if (tmpFrequency <= 0)
                {
                    log_error("Invalid frequency provided (%s)", optarg);
                    printUsage(argv[0]);
                    exit(-1); // NOLINT
                }
                stationFrequency = tmpFrequency;
                log_info("Using frequency %" PRId64 " for FreeDV Reporter reporting", stationFrequency);
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
    
    log_info("Expecting int16 samples at %d Hz sample rate on stdin and outputting int16 samples at %d Hz on stdout", inputSampleRate, outputSampleRate);
    
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
    callbackObj->infifo2 = new GenericFIFO<short>(INPUT_FIFO_SIZE_SAMPLES);
    assert(callbackObj->infifo2 != nullptr);
    callbackObj->outfifo1 = nullptr;
    callbackObj->outfifo2 = new GenericFIFO<short>(OUTPUT_FIFO_SIZE_SAMPLES);
    assert(callbackObj->outfifo2 != nullptr);

    // Initialize RX thread
    MinimalTxRxThread rxThread(false, inputSampleRate, outputSampleRate, realtimeHelper, radeObj, lpcnetEncState, &fargan, radeTextPtr, callbackObj.get());

    log_info("Starting RX thread");
    rxThread.start();
    
    log_info("Synchronizing startup of RX thread");
    rxThread.waitForReady();
    rxThread.signalToStart();
    
    CallsignReporting reportData;
    ReportingController reportController(SOFTWARE_NAME, true);
    reportData.reporter = &reportController;
    reportData.rxThread = &rxThread;

    rade_text_set_rx_callback(radeTextPtr, &ReportReceivedCallsign, &reportData);

    // Set up reporting of actual receive state (prior to getting callsign).
    int rxCounter = 0;    
    ThreadedTimer rxNoCallsignReporting(100, [&](ThreadedTimer&) {
        if (rxThread.getSync() && !reportController.isHidden())
        {
            rxCounter = (rxCounter + 1) % 10;
            if (rxCounter == 0)
            {
                reportController.reportCallsign("", rxThread.getSnr());
            }
        }
    }, true);
    rxNoCallsignReporting.start();

    bool reporterValid = stationCallsign != "" && stationGridSquare != "" && stationFrequency > 0;
    if (reporterValid)
    {
        reportController.updateRadioCallsign(stationCallsign);
        reportController.updateRadioGridSquare(stationGridSquare);
        reportController.changeFrequency(stationFrequency);
        reportController.showSelf();
    }
    else
    {
        log_warn("Callsign, frequency and grid square/locator must all be provided to enable FreeDV Reporter reporting. Not connecting to FreeDV Reporter.");
    }

#ifdef _WIN32
    // Note: freopen() returns NULL if filename is NULL, so
    // we have to use setmode() to make it a binary stream instead.
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
#endif // _WIN32

    bool exiting = false; 
    const int NUM_TO_READ = (FRAME_DURATION_MS * inputSampleRate / 1000);
    const int NUM_TO_WRITE = (FRAME_DURATION_MS * outputSampleRate / 1000);
    short* readBuffer = new short[NUM_TO_READ];
    assert(readBuffer != nullptr);
    short* writeBuffer = new short[NUM_TO_WRITE];
    assert(writeBuffer != nullptr);
    while (!exiting)
    {
        fd_set readSet;
        struct timeval tv;
        
        auto stdinFileNo = fileno(stdin);

        FD_ZERO(&readSet);
        FD_SET(stdinFileNo, &readSet);

        tv.tv_sec = 0;
        tv.tv_usec = 0;
        
        int rv = select(stdinFileNo + 1, &readSet, nullptr, nullptr, &tv);
        if (rv > 0 && !exiting)
        {
            if (callbackObj->infifo2->numFree() >= NUM_TO_READ)
            {
                // Read from standard input and queue on input FIFO
                int numActuallyRead = read(stdinFileNo, readBuffer, sizeof(short) * NUM_TO_READ);
                if (numActuallyRead <= 0) 
                {
                    // stdin closed, exit
                    log_warn("stdin pipe closed, exiting");
                    exiting = true;
                }
                callbackObj->infifo2->write(readBuffer, numActuallyRead / sizeof(short));
            }
        }
        else if (rv < 0)
        {
            constexpr int ERROR_BUFFER_LEN = 1024;
            char tmpBuf[ERROR_BUFFER_LEN];
            log_error("Error waiting for input: %s", strerror_r(errno, tmpBuf, ERROR_BUFFER_LEN));
            break;
        }

        // If we have anything decoded, output that now
        while (callbackObj->outfifo2->numUsed() >= NUM_TO_WRITE)
        {
            callbackObj->outfifo2->read(writeBuffer, NUM_TO_WRITE);
            int numActuallyWritten = write(fileno(stdout), writeBuffer, sizeof(short) * NUM_TO_WRITE);
            if (numActuallyWritten <= 0)
            {
                // stdout closed, exit
                log_warn("stdout pipe closed, exiting");
                exiting = true;
                break;
            }
        }

        if (!exiting)
        {
            // Wait a bit for the output FIFO to be processed.
            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DURATION_MS));
        }
    }
    delete[] readBuffer;
    delete[] writeBuffer;

    rxThread.stop();
    
    // TBD - the below isn't called since we're in an infinite loop above.
    rade_close(radeObj);
    rade_finalize();
    return 0;
}
