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

static float tx_scale_factor = std::exp(9.0f/20.0f * std::log(10.0f));
constexpr float SHORT_TO_FLOAT_DIVIDER = 32767.0;
constexpr short FLOAT_TO_SHORT_MULTIPLIER = 32767;

using namespace std::placeholders;

FlexVitaTask::FlexVitaTask()
    : packetReadTimer_(VITA_IO_TIME_INTERVAL_US / US_TO_MS, std::bind(&FlexVitaTask::readPendingPackets_, this, _1), true)
    , packetWriteTimer_(VITA_IO_TIME_INTERVAL_US / US_TO_MS, std::bind(&FlexVitaTask::sendAudioOut_, this, _1), true)
    , socket_(-1)
    , rxStreamId_(0)
    , txStreamId_(0)
    , audioSeqNum_(0)
    , currentTime_(0)
    , timeFracSeq_(0)
    , audioEnabled_(false)
    , isTransmitting_(false)
    , inputCtr_(0)
    , lastVitaGenerationTime_(0)
    , minPacketsRequired_(0)
    , timeBeyondExpectedUs_(0)
{
    packetArray_ = new vita_packet[MAX_VITA_PACKETS];
    assert(packetArray_ != nullptr);
    packetIndex_ = 0;
    
    // Allocate FIFOs for use by pipelines.
    callbackData_.infifo1 = new GenericFIFO<short>(FIFO_SIZE);
    assert(callbackData_.infifo1 != nullptr);
    callbackData_.infifo2 = new GenericFIFO<short>(FIFO_SIZE);
    assert(callbackData_.infifo1 != nullptr);
    callbackData_.outfifo1 = new GenericFIFO<short>(FIFO_SIZE);
    assert(callbackData_.infifo1 != nullptr);
    callbackData_.outfifo2 = new GenericFIFO<short>(FIFO_SIZE);
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

    auto curTime = std::chrono::high_resolution_clock::now().time_since_epoch();
    auto currentTimeInMicroseconds =  std::chrono::duration_cast<std::chrono::microseconds>(curTime).count();
    auto timeSinceLastPacketSend = currentTimeInMicroseconds - lastVitaGenerationTime_;
    lastVitaGenerationTime_ = currentTimeInMicroseconds;

    // If we're starved of audio, don't even bother going through the rest of the logic right now
    // and reset state back to the point right when we started.
    /*if (codec2_fifo_used(fifo) < MAX_VITA_SAMPLES_TO_RESAMPLE * minPacketsRequired_)
    {
        if (isTransmitting_) ESP_LOGW(CURRENT_LOG_TAG, "Not enough audio samples to process/send packets (minimum packets needed: %d)!", minPacketsRequired_);
        minPacketsRequired_ = 0;
        timeBeyondExpectedUs_ = 0;
        return;
    }*/

    if (isTransmitting_ && (
        timeSinceLastPacketSend >= (VITA_IO_TIME_INTERVAL_US + MAX_JITTER_US) ||
        timeSinceLastPacketSend <= (VITA_IO_TIME_INTERVAL_US - MAX_JITTER_US)))
    {
        log_warn( 
            "Packet TX jitter is a bit high (time = %" PRIu64 ", expected: %d-%d)", 
            timeSinceLastPacketSend,
            VITA_IO_TIME_INTERVAL_US - MAX_JITTER_US,
            VITA_IO_TIME_INTERVAL_US + MAX_JITTER_US);
    }

    // Determine the number of extra packets we need to send this go-around.
    // This is since it can take a bit longer than the timer interval to 
    // actually enter this method (depending on what else is going on in the
    // system at the time).
    int addedExtra = 0;
    auto packetsToSend = MIN_VITA_PACKETS_TO_SEND * timeSinceLastPacketSend / VITA_IO_TIME_INTERVAL_US;
    if (packetsToSend == 0 && minPacketsRequired_ == 0)
    {
        // We're executing too quickly (or there are a bunch of events piled up that
        // need to be worked through). We'll wait until the next time we're actually
        // supposed to execute.
        log_warn("Executing send handler too quickly!");
        return;
    }
    else
    {
        //ESP_LOGI(CURRENT_LOG_TAG, "In the previous interval, %" PRIu64 " packets should have gone out (time since last send = %" PRIu64 ")", packetsToSend, timeSinceLastPacketSend);
        minPacketsRequired_ += packetsToSend;
        timeBeyondExpectedUs_ += timeSinceLastPacketSend % VITA_IO_TIME_INTERVAL_US;
    }

    while (timeBeyondExpectedUs_ >= (US_OF_AUDIO_PER_VITA_PACKET >> 1))
    {
        addedExtra = 1;
        minPacketsRequired_++;
        timeBeyondExpectedUs_ -= US_OF_AUDIO_PER_VITA_PACKET;
    }

    if (minPacketsRequired_ <= 0)
    {
        minPacketsRequired_ = 0;
        timeBeyondExpectedUs_ = 0;
    }

    //ESP_LOGI(CURRENT_LOG_TAG, "Packets to be sent this time: %d", minPacketsRequired_);
    int ctr = MAX_VITA_PACKETS_TO_SEND;
    while(minPacketsRequired_ > 0 && ctr > 0 && fifo->read(inputBuffer, MAX_VITA_SAMPLES) == 0)
    {
        minPacketsRequired_--;
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
        for (int index = 0; index < MAX_VITA_SAMPLES; index++)
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

        size_t packet_len = VITA_PACKET_HEADER_SIZE + VITA_SAMPLES_TO_SEND * 2 * sizeof(float);

        //  XXX Lots of magic numbers here!
        packet->timestamp_type = 0x50u | (packet->timestamp_type & 0x0Fu);
        assert((packet_len & 0x3) == 0); // equivalent to packet_len / 4
        packet->length = htons(packet_len >> 2); // Length is in 32-bit words, note there are two channels

        packet->timestamp_int = htonl(time(NULL));
        packet->timestamp_frac = __builtin_bswap64(audioSeqNum_ - 1);
        currentTime_ = packet->timestamp_int;
        
        int rv = sendto(socket_, (char*)packet, packet_len, 0, (struct sockaddr*)&radioAddress_, sizeof(radioAddress_));
        if (rv < 0)
        {
            // TBD: close and reopen socket
            log_error("Got socket error %d (%s) while sending", errno, strerror(errno));
        }
    }

    minPacketsRequired_ -= addedExtra;
}

void FlexVitaTask::openSocket_()
{
    // Bind socket so we can at least get discovery packets.
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == -1)
    {
        log_error("Got socket error %d (%s) while creating socket", errno, strerror(errno));
    }
    assert(socket_ != -1);

    // Listen on our hardcoded VITA port
    struct sockaddr_in ourSocketAddress;
    memset((char *) &ourSocketAddress, 0, sizeof(ourSocketAddress));

    ourSocketAddress.sin_family = AF_INET;
    ourSocketAddress.sin_port = htons(VITA_PORT);
    ourSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    
    auto rv = bind(socket_, (struct sockaddr*)&ourSocketAddress, sizeof(ourSocketAddress));
    if (rv == -1)
    {
        auto err = errno;
        log_error("Got socket error %d (%s) while binding", err, strerror(err));
    }
    assert(rv != -1);

    fcntl (socket_, F_SETFL , O_NONBLOCK);

    // The below tells ESP-IDF to transmit on this socket using Wi-Fi voice priority.
    // This also implicitly disables TX AMPDU for this socket. In testing, not having
    // this worked a lot better than having it, so it's disabled for now. Perhaps in
    // the future we can play with this again (perhaps with a lower priority level that
    // will in fact use AMPDU?)
#if 0
    const int precedenceVI = 6;
    const int precedenceOffset = 7;
    int priority = (precedenceVI << precedenceOffset);
    setsockopt(socket_, IPPROTO_IP, IP_TOS, &priority, sizeof(priority));
#endif // 0

    minPacketsRequired_ = MIN_VITA_PACKETS_TO_SEND;
    timeBeyondExpectedUs_ = 0;
    
    auto curTime = std::chrono::high_resolution_clock::now().time_since_epoch();
    auto currentTimeInMicroseconds =  std::chrono::duration_cast<std::chrono::microseconds>(curTime).count();
    lastVitaGenerationTime_ = currentTimeInMicroseconds;

    packetReadTimer_.start();
    packetWriteTimer_.start();

    inputCtr_ = 0;
}

void FlexVitaTask::disconnect_()
{
    if (socket_ > 0)
    {
        packetReadTimer_.stop();
        packetWriteTimer_.stop();
        close(socket_);
        socket_ = -1;
        
        rxStreamId_ = 0;
        txStreamId_ = 0;
        audioSeqNum_ = 0;
        currentTime_ = 0;
        timeFracSeq_ = 0;
        inputCtr_ = 0;
        packetIndex_ = 0;
    }
}

void FlexVitaTask::readPendingPackets_(ThreadedTimer&)
{ 
    // Process if there are pending datagrams in the buffer
    enqueue_([this]() {
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
    });
}

void FlexVitaTask::sendAudioOut_(ThreadedTimer&)
{
    enqueue_([this]() {
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
    });
}

void FlexVitaTask::radioConnected(const char* ip)
{
    enqueue_([this, ip]() {
        ip_ = ip;
    
        radioAddress_.sin_addr.s_addr = inet_addr(ip_.c_str());
        radioAddress_.sin_family = AF_INET;
        radioAddress_.sin_port = htons(4993); // hardcoded as per Flex documentation
    
        log_info("Connected to radio successfully");
    });
}

void FlexVitaTask::onReceiveVitaMessage_(vita_packet* packet, int length)
{
    // Make sure packet is long enough to inspect for VITA header info.
    if (length < VITA_PACKET_HEADER_SIZE)
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
            /*if(payload_length != packet->length - VITA_PACKET_HEADER_SIZE) 
            {
                ESP_LOGW(CURRENT_LOG_TAG, "VITA header size doesn't match bytes read from network (%lu != %u - %u) -- %u\n", payload_length, packet->length, VITA_PACKET_HEADER_SIZE, sizeof(struct vita_packet));
                goto cleanup;
            }*/

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
            
            // Downconvert to 8K sample rate.
            unsigned int num_samples = payload_length >> 2; // / sizeof(uint32_t);
            unsigned int half_num_samples = num_samples >> 1;

            int i = 0;
            short audioInput[MAX_VITA_SAMPLES];
            while (inFifo != nullptr && i < half_num_samples)
            {
                uint32_t temp = ntohl(packet->if_samples[i << 1]);
                audioInput[i] = *(float*)&temp * FLOAT_TO_SHORT_MULTIPLIER;
                i++;
            }
            inFifo->write(audioInput, half_num_samples); // audio pipeline will resample
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
        
        // Reset packet timing parameters so we can redetermine how quickly we need to be
        // sending packets.
        auto curTime = std::chrono::high_resolution_clock::now().time_since_epoch();
        auto currentTimeInMicroseconds =  std::chrono::duration_cast<std::chrono::microseconds>(curTime).count();
        
        minPacketsRequired_ = MIN_VITA_PACKETS_TO_SEND;
        timeBeyondExpectedUs_ = 0;
        lastVitaGenerationTime_ = currentTimeInMicroseconds;
        packetWriteTimer_.stop();
        packetWriteTimer_.start();
    });
}