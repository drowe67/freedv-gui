//=========================================================================
// Name:            FlexVitaTask.h
// Purpose:         Audio handler for Flex waveform.
//
// Authors:         Mooneer Salem
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================

#ifndef FLEX_VITA_TASK_H
#define FLEX_VITA_TASK_H

#include <chrono>
#include <functional>
#include <thread>
#include <map>
#include <string>
#include <ctime>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../util/ThreadedObject.h"
#include "../util/IRealtimeHelper.h"
#include "vita.h"

#include "../pipeline/paCallbackData.h"

/// @brief Handles the VITA UDP socket for Flex 6000/8000 series radios.
class FlexVitaTask : public ThreadedObject
{
public:
    using RadioDiscoveredFn = std::function<void(FlexVitaTask&, std::string const&, std::string const&, void*)>;
    
    enum { VITA_PORT = 4992 }; // Default VITA port if we're discovering other radios on the network.
    
    FlexVitaTask(std::shared_ptr<IRealtimeHelper> helper, float volumeAdjustmentDecibel = 0.0f);
    virtual ~FlexVitaTask();
    
    // Indicates to VitaTask that we've connected to the radio's TCP port.
    // This allows VitaTask to begin routing audio appropriately.
    void radioConnected(const char* ip);
    
    void setOnRadioDiscoveredFn(RadioDiscoveredFn fn, void* st)
    {
        enqueue_([this, fn = std::move(fn), st]() {
            onRadioDiscoveredFn_ = fn;
            onRadioDiscoveredFnState_ = st;
        });
    }
    
    paCallBackData* getCallbackData()
    {
        return &callbackData_;
    }
    
    void enableAudio(bool enabled);
    void setTransmit(bool tx);

    void setEndingTx(bool endingTx)
    {
        pendingEndTx_ = endingTx;
    }

    int getPort() const { return udpPort_; }
   
    void registerStreamIds(uint32_t txInStreamId, uint32_t txOutStreamId, uint32_t rxInStreamId, uint32_t rxOutStreamId);
    void clearStreamIds();

    void sendMeter(uint16_t meterId, float valueDb);
 
private:    
    paCallBackData callbackData_;
    struct sockaddr_in radioAddress_;
    int socket_;
    int discoverySocket_;
    std::string ip_;
    uint32_t rxStreamId_;
    uint32_t txStreamId_;
    uint32_t audioSeqNum_;
    int timeFracSeq_;
    bool audioEnabled_;
    bool isTransmitting_;
    int inputCtr_;
    int samplesRequired_;
    std::shared_ptr<IRealtimeHelper> helper_;
    std::thread rxTxThread_;
    bool rxTxThreadRunning_;
    bool pendingEndTx_;
    int udpPort_;
    std::map<uint32_t, uint32_t> txStreamIds_; // txIn -> txOut
    std::map<uint32_t, uint32_t> rxStreamIds_; // rxIn -> rxOut

    // vita packet cache -- preallocate on startup
    // to reduce the amount of latency when sending packets 
    // to the radio.
    vita_packet* packetArray_;
    int packetIndex_;
    
    // Event handlers
    RadioDiscoveredFn onRadioDiscoveredFn_;
    void* onRadioDiscoveredFnState_;

    // Volume adjustment
    float volumeAdjustmentScaleFactor_;
    
    GenericFIFO<short>* getAudioInput_(bool tx);
    GenericFIFO<short>* getAudioOutput_(bool tx);
    
    void openSocket_();
    void disconnect_();
    
    void rxTxThreadEntry_();
    void readPendingPackets_(struct pollfd* fds, int numFds);
    void sendAudioOut_();
    
    void generateVitaPackets_(bool tx, uint32_t streamId);
    
    void onReceiveVitaMessage_(vita_packet* packet, int length);
};

#endif // FLEX_TCP_TASK_H
