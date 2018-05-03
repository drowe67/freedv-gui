//==========================================================================
// Name:            fdmdv2_pa_wrapper.cpp
// Purpose:         Implements a wrapper class around the PortAudio library.
// Created:         August 12, 2012
// Authors:         David Rowe, David Witten
// 
// License:
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
//==========================================================================
#include "fdmdv2_pa_wrapper.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// PortAudioWrap()
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
PortAudioWrap::PortAudioWrap()
{
    m_pStream                   = NULL;
    m_pUserData                 = NULL;
    m_samplerate                = 0;
    m_framesPerBuffer           = 0;
    m_statusFlags               = 0;
    m_pStreamCallback           = NULL;
    m_pStreamFinishedCallback   = NULL;
    m_pTimeInfo                 = 0;
    m_newdata                   = false;

//    loadData();
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// ~PortAudioWrap()
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
PortAudioWrap::~PortAudioWrap()
{
}

//----------------------------------------------------------------
// streamOpen()
//----------------------------------------------------------------
PaError PortAudioWrap::streamOpen()
{
    return Pa_OpenStream(
                            &m_pStream,
                            m_inputBuffer.device == paNoDevice ? NULL : &m_inputBuffer,
                            m_outputBuffer.device == paNoDevice ? NULL : &m_outputBuffer,
                            m_samplerate,
                            m_framesPerBuffer,
                            m_statusFlags,
                            *m_pStreamCallback,
                            m_pUserData
                        );
}

//----------------------------------------------------------------
// streamStart()
//----------------------------------------------------------------
PaError PortAudioWrap::streamStart()
{
    return Pa_StartStream(m_pStream);
}

//----------------------------------------------------------------
// streamClose()
//----------------------------------------------------------------
PaError PortAudioWrap::streamClose()
{
    if(isOpen())
    {
        PaError rv = Pa_CloseStream(m_pStream);
        return rv;
    }
    else
    {
        return paNoError;
    }
}

//----------------------------------------------------------------
// terminate()
//----------------------------------------------------------------
void PortAudioWrap::terminate()
{
    if(Pa_IsStreamStopped(m_pStream) != paNoError)
    {
        Pa_StopStream(m_pStream);
    }
    Pa_Terminate();
}

//----------------------------------------------------------------
// stop()
//----------------------------------------------------------------
void PortAudioWrap::stop()
{
    Pa_StopStream(m_pStream);
}

//----------------------------------------------------------------
// abort()
//----------------------------------------------------------------
void PortAudioWrap::abort()
{
    Pa_AbortStream(m_pStream);
}

//----------------------------------------------------------------
// isStopped()
//----------------------------------------------------------------
bool PortAudioWrap::isStopped() const
{
    PaError ret = Pa_IsStreamStopped(m_pStream);
    return ret;
}

//----------------------------------------------------------------
// isActive()
//----------------------------------------------------------------
bool PortAudioWrap::isActive() const
{
    PaError ret = Pa_IsStreamActive(m_pStream);
    return ret;
}

//----------------------------------------------------------------
// isOpen()
//----------------------------------------------------------------
bool PortAudioWrap::isOpen() const
{
    return (m_pStream != NULL);
}

//----------------------------------------------------------------
// getDefaultInputDevice()
//----------------------------------------------------------------
PaDeviceIndex PortAudioWrap::getDefaultInputDevice()
{
    return Pa_GetDefaultInputDevice();
}

//----------------------------------------------------------------
// getDefaultOutputDevice()
//----------------------------------------------------------------
PaDeviceIndex PortAudioWrap::getDefaultOutputDevice()
{
    return Pa_GetDefaultOutputDevice();
}

//----------------------------------------------------------------
// setInputChannelCount()
//----------------------------------------------------------------
PaError PortAudioWrap::setInputChannelCount(int count)
{
    m_inputBuffer.channelCount = count;
    return paNoError;
}

//----------------------------------------------------------------
// getInputChannelCount()
//----------------------------------------------------------------
PaError PortAudioWrap::getInputChannelCount()
{
    return m_inputBuffer.channelCount;
}

//----------------------------------------------------------------
// setInputSampleFormat()
//----------------------------------------------------------------
PaError PortAudioWrap::setInputSampleFormat(PaSampleFormat format)
{
    m_inputBuffer.sampleFormat = format;
    return paNoError;
}

//----------------------------------------------------------------
// setInputLatency()
//----------------------------------------------------------------
PaError PortAudioWrap::setInputLatency(PaTime latency)
{
    m_inputBuffer.suggestedLatency = latency;
    return paNoError;
}

//----------------------------------------------------------------
// setInputHostApiStreamInfo()
//----------------------------------------------------------------
void PortAudioWrap::setInputHostApiStreamInfo(void *info)
{
    m_inputBuffer.hostApiSpecificStreamInfo = info;
}

//----------------------------------------------------------------
// getInputDefaultLowLatency()
//----------------------------------------------------------------
PaTime  PortAudioWrap::getInputDefaultLowLatency()
{
    return Pa_GetDeviceInfo(m_inputBuffer.device)->defaultLowInputLatency;
}

//----------------------------------------------------------------
// getInputDefaultHighLatency()
//----------------------------------------------------------------
PaTime  PortAudioWrap::getInputDefaultHighLatency()
{
    return Pa_GetDeviceInfo(m_inputBuffer.device)->defaultHighInputLatency;
}

//----------------------------------------------------------------
// setOutputChannelCount()
//----------------------------------------------------------------
PaError PortAudioWrap::setOutputChannelCount(int count)
{
    m_outputBuffer.channelCount = count;
    return paNoError;
}

//----------------------------------------------------------------
// getOutputChannelCount()
//----------------------------------------------------------------
const int PortAudioWrap::getOutputChannelCount()
{
    return m_outputBuffer.channelCount;
}

//----------------------------------------------------------------
// getDeviceName()
//----------------------------------------------------------------
const char *PortAudioWrap::getDeviceName(PaDeviceIndex dev)
{
    const PaDeviceInfo *info;
    info = Pa_GetDeviceInfo(dev);
    return info->name;
}

//----------------------------------------------------------------
// setOutputSampleFormat()
//----------------------------------------------------------------
PaError PortAudioWrap::setOutputSampleFormat(PaSampleFormat format)
{
    m_outputBuffer.sampleFormat = format;
    return paNoError;
}

//----------------------------------------------------------------
// setOutputLatency()
//----------------------------------------------------------------
PaError PortAudioWrap::setOutputLatency(PaTime latency)
{
    m_outputBuffer.suggestedLatency = latency;
    return paNoError;
}

//----------------------------------------------------------------
// setOutputHostApiStreamInfo()
//----------------------------------------------------------------
void PortAudioWrap::setOutputHostApiStreamInfo(void *info)
{
    m_outputBuffer.hostApiSpecificStreamInfo = info;
}

//----------------------------------------------------------------
// getOutputDefaultLowLatency()
//----------------------------------------------------------------
PaTime  PortAudioWrap::getOutputDefaultLowLatency()
{
    return Pa_GetDeviceInfo(m_outputBuffer.device)->defaultLowOutputLatency;
}

//----------------------------------------------------------------
// getOutputDefaultHighLatency()
//----------------------------------------------------------------
PaTime  PortAudioWrap::getOutputDefaultHighLatency()
{
    return Pa_GetDeviceInfo(m_outputBuffer.device)->defaultHighOutputLatency;
}

//----------------------------------------------------------------
// setFramesPerBuffer()
//----------------------------------------------------------------
PaError PortAudioWrap::setFramesPerBuffer(unsigned long size)
{
    m_framesPerBuffer = size;
    return paNoError;
}

//----------------------------------------------------------------
// setSampleRate()
//----------------------------------------------------------------
PaError PortAudioWrap::setSampleRate(unsigned long rate)
{
    m_samplerate = rate;
    return paNoError;
}

//----------------------------------------------------------------
// setStreamFlags()
//----------------------------------------------------------------
PaError PortAudioWrap::setStreamFlags(PaStreamFlags flags)
{
    m_statusFlags = flags;
    return paNoError;
}

//----------------------------------------------------------------
// setInputDevice()
//----------------------------------------------------------------
PaError PortAudioWrap::setInputDevice(PaDeviceIndex index)
{
    m_inputBuffer.device = index;
    return paNoError;
}

//----------------------------------------------------------------
// setOutputDevice()
//----------------------------------------------------------------
PaError PortAudioWrap::setOutputDevice(PaDeviceIndex index)
{
    m_outputBuffer.device = index;
    return paNoError;
}

//----------------------------------------------------------------
// setCallback()
//----------------------------------------------------------------
PaError PortAudioWrap::setCallback(PaStreamCallback *callback)
{
    m_pStreamCallback = callback;
    return paNoError;
}

