//=========================================================================
// Name:            DebugRecordStep.cpp
// Purpose:         Records last X seconds of audio to file for debugging
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#include "DebugRecordStep.h"
#include "sndfile.h"
#include "../util/logging/ulog.h"

DebugRecordStep::DebugRecordStep(int sampleRate, int numSecondsToRecord, std::string fileName)
    : sampleRate_(sampleRate)
    , inputSampleFifo_(sampleRate * numSecondsToRecord)
    , fileName_(fileName)
{
    // empty
}

DebugRecordStep::~DebugRecordStep()
{
    SNDFILE* outputFile;
    SF_INFO     sfInfo;
    
    // Set output file format
    sfInfo.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    sfInfo.channels   = 1;
    sfInfo.samplerate = sampleRate_;
    
    // Open file. If we can't open, just output a log message.
    outputFile = sf_open(fileName_.c_str(), SFM_WRITE, &sfInfo);
    if (outputFile == nullptr)
    {
        std::string errorMessage = sf_strerror(NULL);
        log_error("Could not open %s for writing: %s", fileName_.c_str(), errorMessage.c_str());
        return;
    }
    
    // Write samples and close
    constexpr int MAX_TO_READ = 1024;
    short tmpBuf[MAX_TO_READ];
    int fifoUsed = 0;
    while ((fifoUsed = inputSampleFifo_.numUsed()) > 0)
    {
        auto size = fifoUsed > MAX_TO_READ ? MAX_TO_READ : fifoUsed;
        inputSampleFifo_.read(tmpBuf, size);
        sf_write_short(outputFile, tmpBuf, size);
    }
    sf_close(outputFile);
}

int DebugRecordStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

int DebugRecordStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

short* DebugRecordStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    while (inputSampleFifo_.numFree() < numInputSamples)
    {
        short tmp = 0;
        inputSampleFifo_.read(&tmp, 1);
    }
    
    inputSampleFifo_.write(inputSamples, numInputSamples);
    
    // Just a passthrough.
    *numOutputSamples = numInputSamples;
    return inputSamples;
}

void DebugRecordStep::reset() FREEDV_NONBLOCKING
{
    inputSampleFifo_.reset();
}