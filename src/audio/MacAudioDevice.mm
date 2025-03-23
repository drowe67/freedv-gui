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
    , audioConverter_(nil)
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
        AVAudioFormat* audioFormat = 
            [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                   sampleRate:sampleRate_
                                   channels:numChannels_
                                   interleaved:YES];
        AVAudioConverter* converter = nil;
    
        if (direction_ == IAudioEngine::AUDIO_ENGINE_IN)
        {
            nativeFormat = [[engine inputNode] outputFormatForBus:0];
            converter = [[AVAudioConverter alloc] initFromFormat:nativeFormat toFormat:audioFormat];
        }
        else
        {
            // Audio nodes only accept non-interleaved float samples for some reason, but can handle sample
            // rate conversions no problem.
            nativeFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                   sampleRate:sampleRate_
                                   channels:numChannels_
                                   interleaved:NO];
            converter = [[AVAudioConverter alloc] initFromFormat:audioFormat toFormat:nativeFormat];
        }
        audioConverter_ = converter;
    
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
            
#if 0
            [inNode installTapOnBus:0 
                    bufferSize:NUM_FRAMES 
                    format:nativeFormat
                    block:^(AVAudioPCMBuffer *buffer, AVAudioTime *when) {            
                // Call callback with audio data
                if (onAudioDataFunction)
                {
                    AVAudioPCMBuffer* outputBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:[converter outputFormat] frameCapacity:buffer.frameLength];
                    NSError* err = nil;
                    auto result = [converter convertToBuffer:outputBuffer error:&err withInputFromBlock:^(unsigned int, enum AVAudioConverterInputStatus *status) {
                        *status = AVAudioConverterInputStatus_HaveData;
                        return buffer;
                    }];
                    if (result == AVAudioConverterOutputStatus_Error)
                    {
                        NSString* errorDesc = [err localizedDescription];
                        log_error("Could not perform sample rate conversion for device %d (err %s)", coreAudioId_, [errorDesc cStringUsingEncoding:NSUTF8StringEncoding]);
                    }
                
                    short *const * channelData = [outputBuffer int16ChannelData];
                    onAudioDataFunction(*this, (void*)channelData[0], outputBuffer.frameLength, onAudioDataState);
                    [outputBuffer release];
                }
            }];
#endif 
        }
        else
        {
            log_info("Create player node for output device %d", coreAudioId_);

            AVAudioSourceNodeRenderBlock block = ^(BOOL *, const AudioTimeStamp *, AVAudioFrameCount frameCount, AudioBufferList *outputData)
            {
                short outputFrames[frameCount * numChannels_];
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
#if 0
            player = [[AVAudioPlayerNode alloc] init]; 
            [engine attachNode:player];
            player_ = player;
        
            [engine connect:player to:mixer format:nativeFormat]; 
        
            // Queue initial audio
            log_info("Queue initial audio for output device %d", coreAudioId_);
            queueNextAudioPlayback_();
#endif
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
        
            log_info("Releasing audio converter for device %d", coreAudioId_);
            AVAudioConverter* converter = (AVAudioConverter*)audioConverter_;
            if (converter != nil)
            {
                [converter release];
            }
        }
        engine_ = nil;
        player_ = nil;
        audioConverter_ = nil;
        
        prom->set_value();
    });
    
    fut.wait();
}

bool MacAudioDevice::isRunning()
{
    return engine_ != nil;
}

void MacAudioDevice::queueNextAudioPlayback_()
{
    AVAudioPlayerNode* playerNode = (AVAudioPlayerNode*)player_;
    
    if (playerNode != nil)
    {    
        // 10ms of samples per run
        AVAudioConverter* converter = (AVAudioConverter*)audioConverter_;
        double destSampleRate = [[converter outputFormat] sampleRate];
        double srcSampleRate = [[converter inputFormat] sampleRate];
        int destNumFrames = FRAME_TIME_SEC_ * destSampleRate;
        int srcNumFrames = FRAME_TIME_SEC_ * srcSampleRate;
        
        //log_info("device %d, src sample rate %d, dest sample rate %d", coreAudioId_, (int)srcSampleRate, (int)destSampleRate);
        
        // Get 2x 10ms frames (20ms of audio). The first schedule has a completion handler that
        // causes this method to be called again once we're halfway done playing.
        AVAudioPCMBuffer* tempBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:[converter inputFormat] frameCapacity:srcNumFrames];
        tempBuffer.frameLength = srcNumFrames;
        AVAudioPCMBuffer* firstBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:[converter outputFormat] frameCapacity:destNumFrames];
        short *const * channelData = [tempBuffer int16ChannelData];
        if (onAudioDataFunction)
        {
            onAudioDataFunction(*this, (void*)channelData[0], srcNumFrames, onAudioDataState);
        }
    
        NSError* err = nil;
        auto result = [converter convertToBuffer:firstBuffer error:&err withInputFromBlock:^(unsigned int, enum AVAudioConverterInputStatus *status) {
            *status = AVAudioConverterInputStatus_HaveData;
            return tempBuffer;
        }];
        if (result == AVAudioConverterOutputStatus_Error)
        {
            NSString* errorDesc = [err localizedDescription];
            log_error("Could not perform sample rate conversion for device %d (err %s)", coreAudioId_, [errorDesc cStringUsingEncoding:NSUTF8StringEncoding]);
        }
    
        [playerNode scheduleBuffer:firstBuffer atTime:nil options:0 completionHandler:^() {
            enqueue_([&]() {
                queueNextAudioPlayback_();
            });
        }];
    
        AVAudioPCMBuffer* secondBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:[converter outputFormat] frameCapacity:destNumFrames];
        if (onAudioDataFunction)
        {
            onAudioDataFunction(*this, (void*)channelData[0], srcNumFrames, onAudioDataState);
        }
        result = [converter convertToBuffer:secondBuffer error:&err withInputFromBlock:^(unsigned int, enum AVAudioConverterInputStatus *status) {
            *status = AVAudioConverterInputStatus_HaveData;
            return tempBuffer;
        }];
        if (result == AVAudioConverterOutputStatus_Error)
        {
            NSString* errorDesc = [err localizedDescription];
            log_error("Could not perform sample rate conversion for device %d (err %s)", coreAudioId_, [errorDesc cStringUsingEncoding:NSUTF8StringEncoding]);
        }
        
        [playerNode scheduleBuffer:secondBuffer atTime:nil options:0 completionHandler:nil];
        
        [firstBuffer release];
        [secondBuffer release];
        [tempBuffer release];
    }
}