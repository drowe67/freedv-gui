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

#ifndef FLEX_VITA_TASK_H
#define FLEX_VITA_TASK_H

#include <chrono>
#include <functional>
#include <thread>
#include <map>
#include <string>
#include <ctime>
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
    
    FlexVitaTask(std::shared_ptr<IRealtimeHelper> helper, bool randomUdpPort);
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
 
private:    
    paCallBackData callbackData_;
    struct sockaddr_in radioAddress_;
    int socket_;
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
    bool randomUdpPort_;
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
    
    GenericFIFO<short>* getAudioInput_(bool tx);
    GenericFIFO<short>* getAudioOutput_(bool tx);
    
    void openSocket_();
    void disconnect_();
    
    void rxTxThreadEntry_();
    void readPendingPackets_();
    void sendAudioOut_();
    
    void generateVitaPackets_(bool tx, uint32_t streamId);
    
    void onReceiveVitaMessage_(vita_packet* packet, int length);
};

#endif // FLEX_TCP_TASK_H
