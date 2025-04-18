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

static OSStatus GetIOBufferFrameSizeRange(AudioObjectID inDeviceID,
                                          UInt32* outMinimum,
                                          UInt32* outMaximum)
{
    AudioObjectPropertyAddress theAddress = { kAudioDevicePropertyBufferFrameSizeRange,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMaster };

    AudioValueRange theRange = { 0, 0 };
    UInt32 theDataSize = sizeof(AudioValueRange);
    OSStatus theError = AudioObjectGetPropertyData(inDeviceID,
                                                   &theAddress,
                                                   0,
                                                   NULL,
                                                   &theDataSize,
                                                   &theRange);
    if(theError == 0)
    {
        *outMinimum = theRange.mMinimum;
        *outMaximum = theRange.mMaximum;
    }
    return theError;
}

static OSStatus SetCurrentIOBufferFrameSize(AudioObjectID inDeviceID,
                                            UInt32 inIOBufferFrameSize)
{
    AudioObjectPropertyAddress theAddress = { kAudioDevicePropertyBufferFrameSize,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMaster };

    return AudioObjectSetPropertyData(inDeviceID,
                                      &theAddress,
                                      0,
                                      NULL,
                                      sizeof(UInt32), &inIOBufferFrameSize);
}

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
        
        // Attempt to set the IO frame size to an optimal value. This hopefully 
        // reduces dropouts on marginal hardware.
        UInt32 minFrameSize = 0;
        UInt32 maxFrameSize = 0;
        UInt32 desiredFrameSize = 2048;
        GetIOBufferFrameSizeRange(coreAudioId_, &minFrameSize, &maxFrameSize);
        if (minFrameSize != 0 && maxFrameSize != 0)
        {
            log_info("Frame sizes of %d to %d are supported for audio device ID %d", minFrameSize, maxFrameSize, coreAudioId_);
            desiredFrameSize = std::min(maxFrameSize, desiredFrameSize);
            desiredFrameSize = std::max(minFrameSize, desiredFrameSize);
            if (SetCurrentIOBufferFrameSize(coreAudioId_, desiredFrameSize) != noErr)
            {
                log_warn("Could not set IO frame size to %d for audio device ID %d", desiredFrameSize, coreAudioId_);
            }
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
        
        // If we were able to set the IO frame size above, also set kAudioUnitProperty_MaximumFramesPerSlice
        // More info: https://developer.apple.com/library/archive/technotes/tn2321/_index.html
        if (desiredFrameSize > 512)
        {
            error = AudioUnitSetProperty(
                audioUnit, 
                kAudioUnitProperty_MaximumFramesPerSlice,
                kAudioUnitScope_Global, 
                0, 
                &desiredFrameSize, 
                sizeof(desiredFrameSize));
            if (error != noErr)
            {
                log_warn("Could not set max frames/slice to %d for audio device ID %d", desiredFrameSize, coreAudioId_);
                SetCurrentIOBufferFrameSize(coreAudioId_, 512);
            }
        }
        
        // Need to also get a mixer node so that the objects get
        // created in the right order.
        AVAudioMixerNode* mixer = 
            (direction_ == IAudioEngine::AUDIO_ENGINE_OUT) ?
            [engine mainMixerNode] :
            nil;
    
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
            [engine connect:[engine inputNode] to:sinkNode format:nil]; 
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

            // Audio nodes only accept non-interleaved float samples for some reason, but can handle sample
            // rate conversions no problem.
            AVAudioFormat* nativeFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                   sampleRate:sampleRate_
                                   channels:numChannels_
                                   interleaved:NO];
            
            AVAudioSourceNode* sourceNode = [[AVAudioSourceNode alloc] initWithFormat:nativeFormat renderBlock:block];
            [engine attachNode:sourceNode];
            [engine connect:sourceNode to:mixer format:nil];
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
        // This is in terms of the number of samples. Divide by sample rate and number of channels to get number of seconds.
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
        
        auto ioLatency = streamLatency + deviceLatencyFrames + deviceSafetyOffset;
        auto frameSize = bufferFrameSize;
        prom->set_value(1000000 * (ioLatency + frameSize) / sampleRate_);
    });
    return fut.get();
}
