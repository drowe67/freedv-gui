//=========================================================================
// Name:            PortAudioInterface.cpp
// Purpose:         Wrapper to enforce thread safety around PortAudio.
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

#include <memory>
#include "PortAudioInterface.h"

std::future<PaError> PortAudioInterface::Initialize()
{
    std::shared_ptr<std::promise<PaError> > prom = std::make_shared<std::promise<PaError> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_Initialize());
    });
    return fut;
}

std::future<PaError> PortAudioInterface::Terminate()
{
    std::shared_ptr<std::promise<PaError> > prom = std::make_shared<std::promise<PaError> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_Terminate());
    });
    return fut;
}

std::future<const char*> PortAudioInterface::GetErrorText(PaError error)
{
    std::shared_ptr<std::promise<const char*> > prom = std::make_shared<std::promise<const char*> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_GetErrorText(error));
    });
    return fut;
}

std::future<const PaHostErrorInfo*> PortAudioInterface::GetLastHostErrorInfo()
{
    std::shared_ptr<std::promise<const PaHostErrorInfo*> > prom = std::make_shared<std::promise<const PaHostErrorInfo*> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_GetLastHostErrorInfo());
    });
    return fut;
}

std::future<PaDeviceIndex> PortAudioInterface::GetDeviceCount()
{
    std::shared_ptr<std::promise<PaDeviceIndex> > prom = std::make_shared<std::promise<PaDeviceIndex> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_GetDeviceCount());
    });
    return fut;
}

std::future<const PaDeviceInfo*> PortAudioInterface::GetDeviceInfo(PaDeviceIndex device)
{
    std::shared_ptr<std::promise<const PaDeviceInfo*> > prom = std::make_shared<std::promise<const PaDeviceInfo*> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_GetDeviceInfo(device));
    });
    return fut;
}

std::future<const PaHostApiInfo*> PortAudioInterface::GetHostApiInfo(PaHostApiIndex hostApi)
{
    std::shared_ptr<std::promise<const PaHostApiInfo*> > prom = std::make_shared<std::promise<const PaHostApiInfo*> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_GetHostApiInfo(hostApi));
    });
    return fut;
}

std::future<PaError> PortAudioInterface::IsFormatSupported(
    const PaStreamParameters* inputParameters,
    const PaStreamParameters* outputParameters,
    double sampleRate 
)
{
    std::shared_ptr<std::promise<PaError> > prom = std::make_shared<std::promise<PaError> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_IsFormatSupported(inputParameters, outputParameters, sampleRate));
    });
    return fut;
}

std::future<PaDeviceIndex> PortAudioInterface::GetDefaultInputDevice(void)
{
    std::shared_ptr<std::promise<PaDeviceIndex> > prom = std::make_shared<std::promise<PaDeviceIndex> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_GetDefaultInputDevice());
    });
    return fut;
}

std::future<PaDeviceIndex> PortAudioInterface::GetDefaultOutputDevice(void)
{
    std::shared_ptr<std::promise<PaDeviceIndex> > prom = std::make_shared<std::promise<PaDeviceIndex> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_GetDefaultOutputDevice());
    });
    return fut;
}

std::future<PaError> PortAudioInterface::OpenStream(
    PaStream **stream, const PaStreamParameters *inputParameters, 
    const PaStreamParameters *outputParameters, double sampleRate, 
    unsigned long framesPerBuffer, PaStreamFlags streamFlags, 
    PaStreamCallback *streamCallback, void *userData)
{
    std::shared_ptr<std::promise<PaError> > prom = std::make_shared<std::promise<PaError> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(
            Pa_OpenStream(
                stream, inputParameters, outputParameters, sampleRate, 
                framesPerBuffer, streamFlags, streamCallback, userData));
    });
    return fut;
}
    
std::future<PaError> PortAudioInterface::StartStream(PaStream *stream)
{
    std::shared_ptr<std::promise<PaError> > prom = std::make_shared<std::promise<PaError> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_StartStream(stream));
    });
    return fut;
}

std::future<PaError> PortAudioInterface::StopStream(PaStream *stream)
{
    std::shared_ptr<std::promise<PaError> > prom = std::make_shared<std::promise<PaError> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_StopStream(stream));
    });
    return fut;
}

std::future<PaError> PortAudioInterface::CloseStream(PaStream *stream)
{
    std::shared_ptr<std::promise<PaError> > prom = std::make_shared<std::promise<PaError> >();
    auto fut = prom->get_future();
    enqueue_([=]() {
        prom->set_value(Pa_CloseStream(stream));
    });
    return fut;
}