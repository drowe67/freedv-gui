/* 
 * This file is part of the ezDV project (https://github.com/tmiw/ezDV).
 * Copyright (c) 2023 Mooneer Salem
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <chrono>
#include <cmath>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>

#include "../pipeline/pipeline_defines.h"
#include "flex_defines.h"
#include "FlexVitaTask.h"
#include "FlexKeyValueParser.h"
#include "../util/logging/ulog.h"

constexpr float SHORT_TO_FLOAT_DIVIDER = 32767.0;
constexpr short FLOAT_TO_SHORT_MULTIPLIER = 32767;

using namespace std::placeholders;

FlexVitaTask::FlexVitaTask(std::shared_ptr<IRealtimeHelper> helper, bool randomUdpPort)
    : socket_(-1)
    , rxStreamId_(0)
    , txStreamId_(0)
    , audioSeqNum_(0)
    , timeFracSeq_(0)
    , audioEnabled_(false)
    , isTransmitting_(false)
    , inputCtr_(0)
    , samplesRequired_(0)
    , helper_(std::move(helper))
    , randomUdpPort_(randomUdpPort)
{
    packetArray_ = new vita_packet[MAX_VITA_PACKETS];
    assert(packetArray_ != nullptr);
    packetIndex_ = 0;
    
    // Allocate FIFOs for use by pipelines.
    callbackData_.infifo1 = new GenericFIFO<short>(FIFO_SIZE_SAMPLES);
    assert(callbackData_.infifo1 != nullptr);
    callbackData_.infifo2 = new GenericFIFO<short>(FIFO_SIZE_SAMPLES);
    assert(callbackData_.infifo1 != nullptr);
    callbackData_.outfifo1 = new GenericFIFO<short>(FIFO_SIZE_SAMPLES);
    assert(callbackData_.infifo1 != nullptr);
    callbackData_.outfifo2 = new GenericFIFO<short>(FIFO_SIZE_SAMPLES);
    assert(callbackData_.infifo1 != nullptr);
    
    openSocket_();
}

FlexVitaTask::~FlexVitaTask()
{
    disconnect_();
    delete[] packetArray_;
    delete callbackData_.infifo1;
    delete callbackData_.infifo2;
    delete callbackData_.outfifo1;
    delete callbackData_.outfifo2;
}

GenericFIFO<short>* FlexVitaTask::getAudioInput_(bool tx)
{
    return tx ? callbackData_.infifo1 : callbackData_.infifo2;
}

GenericFIFO<short>* FlexVitaTask::getAudioOutput_(bool tx)
{
    return tx ? callbackData_.outfifo1 : callbackData_.outfifo2;
}

void FlexVitaTask::generateVitaPackets_(bool transmitChannel, uint32_t streamId)
{
    short inputBuffer[MAX_VITA_SAMPLES];
    
    auto fifo = getAudioOutput_(transmitChannel);

    int ctr = 1; 
    while(ctr > 0 && fifo->read(inputBuffer, samplesRequired_) == 0)
    {
        ctr--;

        if (!audioEnabled_)
        {
            // Skip sending audio to SmartSDR if the user isn't using us yet.
            continue;
        }

        // Get free packet
        vita_packet* packet = &packetArray_[packetIndex_++];
        assert(packet != nullptr);
        if (packetIndex_ == MAX_VITA_PACKETS)
        {
            packetIndex_ = 0;
        }
        uint32_t* ptrOut = (uint32_t*)packet->if_samples;

        // Convert short to float samples and save to packet
        for (int index = 0; index < samplesRequired_; index++)
        {
            float fpSample = inputBuffer[index] / SHORT_TO_FLOAT_DIVIDER;
            uint32_t* fpSampleAsInt = (uint32_t*)&fpSample;
            uint32_t tmp = htonl(*fpSampleAsInt);
            *ptrOut++ = tmp;
            *ptrOut++ = tmp;
        }
                
        // Fil in packet with data
        packet->packet_type = VITA_PACKET_TYPE_IF_DATA_WITH_STREAM_ID;
        packet->stream_id = streamId;
        packet->class_id = AUDIO_CLASS_ID;
        packet->timestamp_type = audioSeqNum_++;

        size_t packet_len = VITA_PACKET_HEADER_SIZE + samplesRequired_ * 2 * sizeof(float);

        constexpr uint8_t FRACTIONAL_TIMESTAMP_REAL_TIME = 0x02;
        constexpr uint8_t INTEGER_TIMESTAMP_UTC = 0x01;
        packet->timestamp_type = (((FRACTIONAL_TIMESTAMP_REAL_TIME << 2) | INTEGER_TIMESTAMP_UTC) << 4) | (packet->timestamp_type & 0x0Fu);

        assert((packet_len & 0x3) == 0); // equivalent to packet_len / 4
        
        packet->length = htons(packet_len >> 2); // Length is in 32-bit words, note there are two channels

        struct timespec currentTime = { .tv_sec = 0, .tv_nsec = 0 };
        if (clock_gettime(CLOCK_REALTIME, &currentTime) == -1)
        {
            log_warn("Could not get current time");
            currentTime.tv_sec = 0;
            currentTime.tv_nsec = 0;
        }

        packet->timestamp_int = htonl(currentTime.tv_sec);
        packet->timestamp_frac = __builtin_bswap64(currentTime.tv_nsec * 1000);
        
        int rv = send(socket_, (char*)packet, packet_len, 0);
        if (rv < 0)
        {
            // TBD: close and reopen socket
            constexpr int ERROR_BUFFER_LEN = 1024;
            char tmpBuf[ERROR_BUFFER_LEN];
            log_error("Got socket error %d (%s) while sending", errno, strerror_r(errno, tmpBuf, ERROR_BUFFER_LEN));
        }
    }
}

void FlexVitaTask::openSocket_()
{
    constexpr int ERROR_BUFFER_LEN = 1024;
    char tmpBuf[ERROR_BUFFER_LEN];

    // Bind socket so we can at least get discovery packets.
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == -1)
    {
        log_error("Got socket error %d (%s) while creating socket", errno, strerror_r(errno, tmpBuf, ERROR_BUFFER_LEN));
        assert(socket_ != -1);
        return;
    }

    // Listen on our hardcoded VITA port (or a random one if we already know the radio's IP)
    struct sockaddr_in ourSocketAddress;
    memset((char *) &ourSocketAddress, 0, sizeof(ourSocketAddress));
    ourSocketAddress.sin_family = AF_INET;
    ourSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (!randomUdpPort_)
    {
        ourSocketAddress.sin_port = htons(VITA_PORT);
        
        auto rv = bind(socket_, (struct sockaddr*)&ourSocketAddress, sizeof(ourSocketAddress));
        if (rv == -1)
        {
            auto err = errno;
            log_error("Got socket error %d (%s) while binding", err, strerror_r(err, tmpBuf, ERROR_BUFFER_LEN));
        }
        assert(rv != -1);

        udpPort_ = VITA_PORT;
    }
    else
    {
        socklen_t bindAddrLen = sizeof(ourSocketAddress);
        auto rv = getsockname(socket_, (struct sockaddr*) &ourSocketAddress, &bindAddrLen);
        if (rv == -1)
        {
            auto err = errno;
            log_error("Got socket error %d (%s) while calling getsockname", err, strerror_r(err, tmpBuf, ERROR_BUFFER_LEN));
        }
        assert(rv != -1);

        udpPort_ = ntohl(ourSocketAddress.sin_port);
    }

    fcntl (socket_, F_SETFL , O_NONBLOCK);

    rxTxThreadRunning_ = true;
    rxTxThread_ = std::thread(std::bind(&FlexVitaTask::rxTxThreadEntry_, this));
    
    inputCtr_ = 0;
}

void FlexVitaTask::disconnect_()
{
    if (socket_ > 0)
    {
        rxTxThreadRunning_ = false;
        rxTxThread_.join();
        
        close(socket_);
        socket_ = -1;
        
        rxStreamId_ = 0;
        txStreamId_ = 0;
        audioSeqNum_ = 0;
        timeFracSeq_ = 0;
        inputCtr_ = 0;
        packetIndex_ = 0;
    }
}

void FlexVitaTask::rxTxThreadEntry_()
{
    helper_->setHelperRealTime();

    while (rxTxThreadRunning_)
    {
        fd_set fds;
        struct timeval timeout;
        
        timeout.tv_sec = 0;
        timeout.tv_usec = VITA_IO_TIME_INTERVAL_US;
        
        FD_ZERO(&fds);
        FD_SET(socket_, &fds);
        
        if (select(socket_ + 1, &fds, nullptr, nullptr, &timeout) > 0)
        {
            readPendingPackets_();
        }
    }

    helper_->clearHelperRealTime();
}

void FlexVitaTask::readPendingPackets_()
{ 
    // Process if there are pending datagrams in the buffer
    int ctr = MAX_VITA_PACKETS_TO_SEND;
    while (ctr-- > 0)
    {
        vita_packet* packet = &packetArray_[packetIndex_++];
        assert(packet != nullptr);

        if (packetIndex_ == MAX_VITA_PACKETS)
        {
            packetIndex_ = 0;
        }
    
        auto rv = recv(socket_, (char*)packet, sizeof(vita_packet), 0);
        if (rv > 0)
        {
            // Queue up packet for future processing.
            onReceiveVitaMessage_(packet, rv);
        }
        else
        {
            break;
        }
    }
}

void FlexVitaTask::sendAudioOut_()
{
    // Generate packets for both RX and TX.
    if (rxStreamId_ && !isTransmitting_)
    {
        generateVitaPackets_(false, rxStreamId_);
    }
    else if (rxStreamId_)
    {
        // Clear FIFO if we're not in the right state. This is so that we
        // don't end up with audio packets going to the wrong place
        // (i.e. UI beeps being transmitted along with the FreeDV signal).
        auto fifo = getAudioOutput_(false);
        fifo->reset();
    }

    if (txStreamId_ && isTransmitting_)
    {
        generateVitaPackets_(true, txStreamId_);
    }
    else if (txStreamId_)
    {
        // Clear FIFO if we're not in the right state. This is so that we
        // don't end up with audio packets going to the wrong place
        // (i.e. UI beeps being transmitted along with the FreeDV signal).
        auto fifo = getAudioOutput_(true);
        fifo->reset();
    }
}

void FlexVitaTask::radioConnected(const char* ip)
{
    enqueue_([this, ip]() {
        ip_ = ip;
    
        radioAddress_.sin_addr.s_addr = inet_addr(ip_.c_str());
        radioAddress_.sin_family = AF_INET;
        radioAddress_.sin_port = htons(4993); // hardcoded as per Flex documentation
   
        if (connect(socket_, (struct sockaddr*)&radioAddress_, sizeof(radioAddress_)) < 0)
        {
            log_error("Could not connect socket to radio's IP (errno = %d)", errno);
        }
        else
        {
            log_info("Connected to radio successfully");
        }
    });
}

void FlexVitaTask::onReceiveVitaMessage_(vita_packet* packet, int length)
{
    // Make sure packet is long enough to inspect for VITA header info.
    if ((unsigned)length < VITA_PACKET_HEADER_SIZE)
        return;

    // Make sure packet is from the radio.
    if((packet->class_id & VITA_OUI_MASK) != FLEX_OUI)
        return;

    // Look for discovery packets
    if (packet->stream_id == DISCOVERY_STREAM_ID && packet->class_id == DISCOVERY_CLASS_ID)
    {
        std::stringstream ss((char*)packet->raw_payload);
        auto parameters = FlexKeyValueParser::GetCommandParameters(ss);

        auto radioFriendlyName = parameters["nickname"] + " (" + parameters["callsign"] + ")";
        auto radioIp = parameters["ip"];
        
        log_info("Discovery: found radio %s at IP %s", radioFriendlyName.c_str(), radioIp.c_str());
        
        if (onRadioDiscoveredFn_)
        {
            onRadioDiscoveredFn_(*this, radioFriendlyName, radioIp, onRadioDiscoveredFnState_);
        }
        
        return;
    }
    
    switch(packet->stream_id & STREAM_BITS_MASK) 
    {
        case STREAM_BITS_WAVEFORM | STREAM_BITS_IN:
        {
            unsigned long payload_length = ((htons(packet->length) * sizeof(uint32_t)) - VITA_PACKET_HEADER_SIZE);

            GenericFIFO<short>* inFifo = nullptr;
            if (!(htonl(packet->stream_id) & 0x0001u)) 
            {
                // Packet contains receive audio from radio.
                rxStreamId_ = packet->stream_id;
                inFifo = getAudioInput_(false);
            } 
            else 
            {
                // Packet contains transmit audio from user's microphone.
                txStreamId_ = packet->stream_id;
                inFifo = getAudioInput_(true);
            }
           
            if (inFifo == nullptr)
            {
                // No valid FIFO, return
                return;
            }
 
            // Convert to int16 samples, normalizing to +/- 1.0 beforehand.
            unsigned int num_samples = payload_length >> 2; // / sizeof(uint32_t);
            unsigned int half_num_samples = num_samples >> 1;

            unsigned int i = 0;
            short audioInput[MAX_VITA_SAMPLES];
            float audioInputFloat[MAX_VITA_SAMPLES];
            float maxSampleValue = 1.0;
            while (i < half_num_samples)
            {
                union {
                    uint32_t intVal;
                    float floatVal;
                } temp;
                temp.intVal = ntohl(packet->if_samples[i << 1]);
                audioInputFloat[i++] = temp.floatVal;
                maxSampleValue = std::max((float)fabsf(temp.floatVal), maxSampleValue);
            }
            float multiplier = (1.0 / maxSampleValue);
            for (i = 0; i < half_num_samples; i++)
            {
                audioInput[i] = audioInputFloat[i] * multiplier * FLOAT_TO_SHORT_MULTIPLIER;
            }
            if (!pendingEndTx_)
            {
                inFifo->write(audioInput, half_num_samples); // audio pipeline will resample
            }
	    samplesRequired_ = half_num_samples;
            sendAudioOut_();
            break;
        }
        default:
            log_warn("Undefined stream in %lx", htonl(packet->stream_id));
            break;
    }
}

void FlexVitaTask::enableAudio(bool enabled)
{
    enqueue_([this, enabled]() {
        audioEnabled_ = enabled;
    });
}

void FlexVitaTask::setTransmit(bool tx)
{
    enqueue_([this, tx]() {
        isTransmitting_ = tx;
    });
}
