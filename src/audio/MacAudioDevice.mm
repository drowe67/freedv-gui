//=========================================================================
// Name:            MacAudioDevice.mm
// Purpose:         Defines the interface to a Core Audio device.
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

#include "MacAudioDevice.h"
#include "../util/logging/ulog.h"

#include <future>
#import <AVFoundation/AVFoundation.h>

MacAudioDevice::MacAudioDevice(int coreAudioId, IAudioEngine::AudioDirection direction, int numChannels, int sampleRate)
    : coreAudioId_(coreAudioId)
    , direction_(direction)
    , numChannels_(numChannels)
    , sampleRate_(sampleRate)
    , engine_(nil)
    , player_(nil)
{
    log_info("Create MacAudioDevice with ID %d, channels %d and sample rate %d", coreAudioId, numChannels, sampleRate);
}

MacAudioDevice::~MacAudioDevice()
{
    if (engine_ != nil)
    {
        stop();
    }
}
    
int MacAudioDevice::getNumChannels()
{
    return numChannels_;
}

int MacAudioDevice::getSampleRate() const
{
    return sampleRate_;
}
    
void MacAudioDevice::start()
{
    std::shared_ptr<std::promise<void>> prom = std::make_shared<std::promise<void> >();
    auto fut = prom->get_future();
    enqueue_([&, prom]() {
        // Set sample rate of device.
        AudioObjectPropertyAddress propertyAddress = {
            .mSelector = kAudioDevicePropertyNominalSampleRate,
            .mScope = kAudioObjectPropertyScopeGlobal,
            .mElement = kAudioObjectPropertyElementMain
        };
        
        propertyAddress.mScope = 
            (direction_ == IAudioEngine::AUDIO_ENGINE_IN) ?
            kAudioDevicePropertyScopeInput :
            kAudioDevicePropertyScopeOutput;
        
        Float64 sampleRateAsFloat = sampleRate_;
        OSStatus error = AudioObjectSetPropertyData(
            coreAudioId_,
            &propertyAddress,
            0,
            nil,
            sizeof(Float64),
            &sampleRateAsFloat);
        if (error != noErr)
        {
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, "Could not set device sample rate", onAudioErrorState);
            }
            return;
        }
        
        // Initialize audio engine.
        AVAudioEngine* engine = [[AVAudioEngine alloc] init];
    
        // Set input or output node to the selected device.    
        AudioUnit audioUnit = 
            (direction_ == IAudioEngine::AUDIO_ENGINE_IN) ?
            [[engine inputNode] audioUnit] :
            [[engine outputNode] audioUnit];
        
        error = AudioUnitSetProperty(audioUnit,
                                     kAudioOutputUnitProperty_CurrentDevice,
                                     kAudioUnitScope_Global,
                                     0,
                                     &coreAudioId_,
                                     sizeof(coreAudioId_));
        if (error != noErr)
        {
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, "Could not assign device to AVAudioEngine", onAudioErrorState);
            }
            [engine release];
            return;
        }
        
        // Need to also get a mixer node so that the objects get
        // created in the right order.
        AVAudioMixerNode* mixer = 
            (direction_ == IAudioEngine::AUDIO_ENGINE_OUT) ?
            [engine mainMixerNode] :
            nil;
            
        // Create sample rate converter
        AVAudioFormat* nativeFormat = nil;    
        if (direction_ == IAudioEngine::AUDIO_ENGINE_IN)
        {
            nativeFormat = [[engine inputNode] outputFormatForBus:0];
        }
        else
        {
            // Audio nodes only accept non-interleaved float samples for some reason, but can handle sample
            // rate conversions no problem.
            nativeFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                   sampleRate:sampleRate_
                                   channels:numChannels_
                                   interleaved:NO];
        }
    
        // Create nodes and links
        AVAudioPlayerNode *player = nil;
        if (direction_ == IAudioEngine::AUDIO_ENGINE_IN)
        {
            log_info("Create mixer node for input device %d", coreAudioId_);
        
            AVAudioInputNode* inNode = [engine inputNode];
            const int NUM_FRAMES = FRAME_TIME_SEC_ * sampleRate_;
            
            AVAudioSinkNodeReceiverBlock block = ^(const AudioTimeStamp* timestamp, AVAudioFrameCount frameCount, const AudioBufferList* inputData) 
            {
                short inputFrames[frameCount * numChannels_];
                memset(inputFrames, 0, sizeof(inputFrames));
                
                if (onAudioDataFunction)
                {
                    for (int index = 0; index < frameCount; index++)
                    {
                        for (int chan = 0; chan < inputData->mNumberBuffers; chan++)
                        {
                            inputFrames[numChannels_ * index + chan] = ((float*)inputData->mBuffers[chan].mData)[index] * 32767;
                        }
                    }
                    
                    onAudioDataFunction(*this, inputFrames, frameCount, onAudioDataState);
                }
                return OSStatus(noErr);
            };
            
            AVAudioSinkNode* sinkNode = [[AVAudioSinkNode alloc] initWithReceiverBlock:block];
            [engine attachNode:sinkNode];    
            [engine connect:[engine inputNode] to:sinkNode format:nativeFormat]; 
        }
        else
        {
            log_info("Create player node for output device %d", coreAudioId_);

            AVAudioSourceNodeRenderBlock block = ^(BOOL *, const AudioTimeStamp *, AVAudioFrameCount frameCount, AudioBufferList *outputData)
            {
                short outputFrames[frameCount * numChannels_];
                memset(outputFrames, 0, sizeof(outputFrames));
                
                if (onAudioDataFunction)
                {
                    onAudioDataFunction(*this, (void*)outputFrames, frameCount, onAudioDataState);
                    
                    for (int index = 0; index < frameCount; index++)
                    {
                        for (int chan = 0; chan < outputData->mNumberBuffers; chan++)
                        {
                            ((float*)outputData->mBuffers[chan].mData)[index] = outputFrames[numChannels_ * index + chan] / 32767.0;
                        }
                    }
                }
                
                return OSStatus(noErr);
            };
            
            AVAudioSourceNode* sourceNode = [[AVAudioSourceNode alloc] initWithFormat:nativeFormat renderBlock:block];
            [engine attachNode:sourceNode];
            [engine connect:sourceNode to:mixer format:nativeFormat];
        }
    
        log_info("Start engine for device %d", coreAudioId_);
    
        NSError *nse = nil;
        [engine prepare];
        if (![engine startAndReturnError:&nse])
        {
            if (onAudioErrorFunction)
            {
                NSString* errorDesc = [nse localizedDescription];
                std::string err = std::string("Could not start AVAudioEngine: ") + [errorDesc cStringUsingEncoding:NSUTF8StringEncoding];
                onAudioErrorFunction(*this, err, onAudioErrorState);
            }
            [engine release];
            return;
        }
        engine_ = engine;
    
        if (direction_ == IAudioEngine::AUDIO_ENGINE_OUT)
        {
            // Begin playback.
            log_info("Start playback for output device %d", coreAudioId_);
            [player play];
        }
        
        prom->set_value();
    });
    fut.wait();
}

void MacAudioDevice::stop()
{
    std::shared_ptr<std::promise<void>> prom = std::make_shared<std::promise<void> >();
    auto fut = prom->get_future();
    enqueue_([&, prom]() {
        if (engine_ != nil)
        {
            AVAudioPlayerNode* player = (AVAudioPlayerNode*)player_;
            if (player != nil)
            {
                log_info("Stopping player for output device %d", coreAudioId_);
                [player stop];
            }
        
            log_info("Stopping engine for device %d", coreAudioId_);
            AVAudioEngine* engine = (AVAudioEngine*)engine_;
            [engine stop];
            [engine release];
        }
        engine_ = nil;
        player_ = nil;
        
        prom->set_value();
    });
    
    fut.wait();
}

bool MacAudioDevice::isRunning()
{
    return engine_ != nil;
}

int MacAudioDevice::getLatencyInMicroseconds()
{
    std::shared_ptr<std::promise<int>> prom = std::make_shared<std::promise<int> >();
    auto fut = prom->get_future();
    enqueue_([&, prom]() {        
        // Total latency is based on the formula:
        //     kAudioDevicePropertyLatency + kAudioDevicePropertySafetyOffset + 
        //     kAudioDevicePropertyBufferFrameSize + kAudioStreamPropertyLatency.
        // This is in terms of the number of samples. Divide by sample rate to get number of seconds.
        // More info:
        //     https://stackoverflow.com/questions/65600996/avaudioengine-reconcile-sync-input-output-timestamps-on-macos-ios
        //     https://forum.juce.com/t/macos-round-trip-latency/45278
        auto scope = 
            (direction_ == IAudioEngine::AUDIO_ENGINE_IN) ? 
            kAudioDevicePropertyScopeInput :
            kAudioDevicePropertyScopeOutput;
            
        // Get audio device latency
        AudioObjectPropertyAddress propertyAddress = {
              kAudioDevicePropertyLatency, 
              scope,
              kAudioObjectPropertyElementMaster};
        UInt32 deviceLatencyFrames = 0;
        UInt32 size = sizeof(deviceLatencyFrames);
        OSStatus result = AudioObjectGetPropertyData(
                  coreAudioId_, 
                  &propertyAddress, 
                  0,
                  nullptr, 
                  &size, 
                  &deviceLatencyFrames); // assume 0 if we can't retrieve for some reason
         
        UInt32 deviceSafetyOffset = 0;
        propertyAddress.mSelector = kAudioDevicePropertySafetyOffset;
        result = AudioObjectGetPropertyData(
                  coreAudioId_, 
                  &propertyAddress, 
                  0,
                  nullptr, 
                  &size, 
                  &deviceSafetyOffset);
                  
        UInt32 bufferFrameSize = 0;
        propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
        result = AudioObjectGetPropertyData(
                  coreAudioId_, 
                  &propertyAddress, 
                  0,
                  nullptr, 
                  &size, 
                  &bufferFrameSize);
    
        propertyAddress.mSelector = kAudioDevicePropertyStreams;
        size = 0;
        result = AudioObjectGetPropertyDataSize(
                  coreAudioId_, 
                  &propertyAddress, 
                  0,
                  nullptr, 
                  &size);
        UInt32 streamLatency = 0;
        if (result == noErr)
        {
            AudioStreamID streams[size / sizeof(AudioStreamID)];
            result = AudioObjectGetPropertyData(
                      coreAudioId_, 
                      &propertyAddress, 
                      0,
                      nullptr, 
                      &size, 
                      &streams);
            if (result == noErr)
            {
                propertyAddress.mSelector = kAudioStreamPropertyLatency;
                size = sizeof(streamLatency);
                result = AudioObjectGetPropertyData(
                          streams[0], 
                          &propertyAddress, 
                          0,
                          nullptr, 
                          &size, 
                          &streamLatency);
            }
        }
        
        // Add additional latency if non bufferFrameSize components > bufferFrameSize.
        // More info: https://www.mail-archive.com/coreaudio-api@lists.apple.com/msg01682.html
        auto ioLatency = streamLatency + deviceLatencyFrames + deviceSafetyOffset;
        auto frameSize = 2 * bufferFrameSize;
        if (ioLatency > bufferFrameSize)
        {
            frameSize += bufferFrameSize;
        }
        prom->set_value(1000000 * (ioLatency + frameSize) / sampleRate_);
    });
    return fut.get();
}