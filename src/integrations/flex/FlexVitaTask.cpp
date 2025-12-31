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

#include <future>
#include <chrono>
#include <cmath>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <poll.h>

#include "../pipeline/pipeline_defines.h"
#include "flex_defines.h"
#include "FlexVitaTask.h"
#include "FlexKeyValueParser.h"
#include "../util/logging/ulog.h"

constexpr float SHORT_TO_FLOAT_DIVIDER = 32767.0;
constexpr short FLOAT_TO_SHORT_MULTIPLIER = 32767;
const float RX_SCALE_FACTOR = std::expf(6.0f/20.0f * std::logf(10.0f));
const float TX_SCALE_FACTOR = std::expf(3.0f/20.0f * std::logf(10.0f));

using namespace std::placeholders;

FlexVitaTask::FlexVitaTask(std::shared_ptr<IRealtimeHelper> helper)
    : ThreadedObject("FlexVita")
    , socket_(-1)
    , discoverySocket_(-1)
    , rxStreamId_(0)
    , txStreamId_(0)
    , audioSeqNum_(0)
    , timeFracSeq_(0)
    , audioEnabled_(false)
    , isTransmitting_(false)
    , inputCtr_(0)
    , samplesRequired_(0)
    , helper_(std::move(helper))
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
    auto prom = std::make_shared<std::promise<void>>();
    auto fut = prom->get_future();
    enqueue_([&, prom]() {
        disconnect_();
        prom->set_value();
    });
    fut.wait();

    waitForAllTasksComplete_();
    
    delete[] packetArray_;
    delete callbackData_.infifo1;
    delete callbackData_.infifo2;
    delete callbackData_.outfifo1;
    delete callbackData_.outfifo2;
}

void FlexVitaTask::sendMeter(uint16_t meterId, float valueDb)
{
    enqueue_([this, meterId, valueDb]() {
        // Get free packet
        vita_packet* packet = &packetArray_[packetIndex_++];
        assert(packet != nullptr);
        if (packetIndex_ == MAX_VITA_PACKETS)
        {
            packetIndex_ = 0;
        }

        // Fil in packet with data
        packet->packet_type = VITA_PACKET_TYPE_EXT_DATA_WITH_STREAM_ID;
        packet->stream_id = METER_STREAM_ID;
        packet->class_id = METER_CLASS_ID;
        packet->timestamp_type = 0;
        packet->timestamp_int = 0;
        packet->timestamp_frac = 0;
        packet->meter[0].id = htons(meterId);
        packet->meter[0].value = htons((int32_t)(valueDb * 128) & 0xFFFF);
        
        size_t packet_len = VITA_PACKET_HEADER_SIZE + sizeof(uint32_t);
        packet->length = htons(packet_len >> 2); // Length is in 32-bit words

        int rv = send(socket_, (char*)packet, packet_len, 0);
        if (rv < 0)
        {
            // TBD: close and reopen socket
            constexpr int ERROR_BUFFER_LEN = 1024;
            char tmpBuf[ERROR_BUFFER_LEN];
            log_error("Got socket error %d (%s) while sending", errno, strerror_r(errno, tmpBuf, ERROR_BUFFER_LEN));
        }
    });
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

        // Convert short to float samples and save to packet.
        // Amplify signal as required.
        for (int index = 0; index < samplesRequired_; index++)
        {
            float fpSample = inputBuffer[index] / SHORT_TO_FLOAT_DIVIDER;
            if (transmitChannel) fpSample *= TX_SCALE_FACTOR;
            else fpSample *= RX_SCALE_FACTOR;
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

    discoverySocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (discoverySocket_ == -1)
    {
        log_error("Got socket error %d (%s) while creating discovery socket", errno, strerror_r(errno, tmpBuf, ERROR_BUFFER_LEN));
        assert(socket_ != -1);
        return;
    }

    // Bind to discovery port (4992)
    struct sockaddr_in ourSocketAddress;
    memset((char *) &ourSocketAddress, 0, sizeof(ourSocketAddress));
    ourSocketAddress.sin_family = AF_INET;
    ourSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    ourSocketAddress.sin_port = htons(VITA_PORT);
        
    auto rv = bind(discoverySocket_, (struct sockaddr*)&ourSocketAddress, sizeof(ourSocketAddress));
    if (rv == -1)
    {
        auto err = errno;
        log_error("Got socket error %d (%s) while binding", err, strerror_r(err, tmpBuf, ERROR_BUFFER_LEN));
    }
    assert(rv != -1);

    // Get local port for main socket
    memset((char *) &ourSocketAddress, 0, sizeof(ourSocketAddress));
    ourSocketAddress.sin_family = AF_INET;
    ourSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    ourSocketAddress.sin_port = htons(0);
    rv = bind(socket_, (struct sockaddr*)&ourSocketAddress, sizeof(ourSocketAddress));
    if (rv == -1)
    {
        auto err = errno;
        log_error("Got socket error %d (%s) while binding", err, strerror_r(err, tmpBuf, ERROR_BUFFER_LEN));
    }
    assert(rv != -1);

    socklen_t bindAddrLen = sizeof(ourSocketAddress);
    memset((char *) &ourSocketAddress, 0, sizeof(ourSocketAddress));
    rv = getsockname(socket_, (struct sockaddr*) &ourSocketAddress, &bindAddrLen);
    if (rv == -1)
    {
        auto err = errno;
        log_error("Got socket error %d (%s) while calling getsockname", err, strerror_r(err, tmpBuf, ERROR_BUFFER_LEN));
    }
    assert(rv != -1);

    udpPort_ = ntohs(ourSocketAddress.sin_port);

    fcntl (socket_, F_SETFL , O_NONBLOCK);
    fcntl (discoverySocket_, F_SETFL , O_NONBLOCK);

    rxTxThreadRunning_ = true;
    rxTxThread_ = std::thread(std::bind(&FlexVitaTask::rxTxThreadEntry_, this));
    
    inputCtr_ = 0;
}

void FlexVitaTask::disconnect_()
{
    rxTxThreadRunning_ = false;
    if (rxTxThread_.joinable())
    {
        rxTxThread_.join();
    }
    
    if (socket_ > 0)
    {
        close(socket_);
        socket_ = -1;

        if (discoverySocket_ > -1)
        {
            close(discoverySocket_);
            discoverySocket_ = -1;
        }
        
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

    struct pollfd fds[2];
    int numFds = 0;

    while (rxTxThreadRunning_)
    {
        memset(fds, 0, sizeof(fds));
        fds[0].fd = socket_;
        fds[0].events = POLLIN;

        if (discoverySocket_ > -1)
        {
            fds[1].fd = discoverySocket_;
            fds[1].events = POLLIN;
            numFds = 2;
        }
        else
        {
            numFds = 1;
        }
        
        if (poll(fds, numFds, VITA_IO_TIME_INTERVAL_US / 1000) > 0)
        {
            readPendingPackets_(fds, numFds);
        }
    }

    helper_->clearHelperRealTime();
}

void FlexVitaTask::readPendingPackets_(struct pollfd* fds, int numFds)
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
    
        int rv = 0;
        if (fds[0].revents != 0)
        {
            rv = recv(socket_, (char*)packet, sizeof(vita_packet), 0);
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

        if (numFds > 1 && fds[1].revents != 0)
        {
            packet = &packetArray_[packetIndex_++];
            assert(packet != nullptr);

            if (packetIndex_ == MAX_VITA_PACKETS)
            {
                packetIndex_ = 0;
            }

            rv = recv(discoverySocket_, (char*)packet, sizeof(vita_packet), 0);
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

void FlexVitaTask::clearStreamIds()
{
    log_info("Clearing registered stream IDs");

    txStreamIds_.clear();
    rxStreamIds_.clear();
}

void FlexVitaTask::registerStreamIds(uint32_t txInStreamId, uint32_t txOutStreamId, uint32_t rxInStreamId, uint32_t rxOutStreamId)
{
    log_info("Registering stream IDs: txin=%" PRIu32 " txout=%" PRIu32 " rxin=%" PRIu32 " rxout=%" PRIu32, txInStreamId, txOutStreamId, rxInStreamId, rxOutStreamId);
    txStreamIds_[txInStreamId] = txOutStreamId;
    rxStreamIds_[rxInStreamId] = rxOutStreamId;
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

        // Close discovery socket as it's no longer needed
        if (discoverySocket_ > -1)
        {
            close(discoverySocket_);
            discoverySocket_ = -1;
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
                rxStreamId_ = rxStreamIds_[htonl(packet->stream_id)];
                if (rxStreamId_ == 0) return;
                rxStreamId_ = packet->stream_id;
                inFifo = getAudioInput_(false);
            } 
            else 
            {
                // Packet contains transmit audio from user's microphone.
                txStreamId_ = txStreamIds_[htonl(packet->stream_id)];
                if (txStreamId_ == 0) return;
                txStreamId_ = packet->stream_id;
                //log_info("outputting on stream %08x, input on %08x", txStreamId_, packet->stream_id);
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
            while (i < half_num_samples)
            {
                union {
                    uint32_t intVal;
                    float floatVal;
                } temp;
                temp.intVal = ntohl(packet->if_samples[i << 1]);
                audioInputFloat[i++] = temp.floatVal;
            }
            for (i = 0; i < half_num_samples; i++)
            {
                audioInput[i] = tanhf(audioInputFloat[i]) * FLOAT_TO_SHORT_MULTIPLIER;
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
