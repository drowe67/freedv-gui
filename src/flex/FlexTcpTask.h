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

#ifndef FLEX_TCP_TASK_H
#define FLEX_TCP_TASK_H

#include <sstream>
#include <map>
#include <functional>

#include "../util/TcpConnectionHandler.h"
#include "../util/ThreadedTimer.h"

/// @brief Handles the main TCP/IP socket for Flex 6000/8000 series radios.
class FlexTcpTask : public TcpConnectionHandler
{
public:
    enum TxState { RECEIVING, TRANSMITTING, ENDING_TX };
    using WaveformConnectedFn = std::function<void(FlexTcpTask&, void*)>;
    using WaveformTransmitFn = std::function<void(FlexTcpTask&, TxState, void*)>;
    using WaveformCallsignRxFn = std::function<void(FlexTcpTask&, std::string, void*)>;
    using WaveformGridSquareUpdateFn = std::function<void(FlexTcpTask&, std::string, void*)>;
    using WaveformFreqChangeFn = std::function<void(FlexTcpTask&, uint64_t, void*)>;
    using WaveformUserConnectedFn = std::function<void(FlexTcpTask&, void*)>;
    using WaveformUserDisconnectedFn = std::function<void(FlexTcpTask&, void*)>;

    FlexTcpTask(int vitaPort);
    virtual ~FlexTcpTask();
    
    void setWaveformConnectedFn(WaveformConnectedFn fn, void* state)
    {
        waveformConnectedFn_ = fn;
        waveformConnectedState_ = state;
    }
    
    void setWaveformTransmitFn(WaveformTransmitFn fn, void* state)
    {
        waveformTransmitFn_ = fn;
        waveformTransmitState_ = state;
    }

    void setWaveformCallsignRxFn(WaveformCallsignRxFn fn, void* state)
    {
        waveformCallsignRxFn_ = fn;
        waveformCallsignRxState_ = state;
    }
    
    void setWaveformFreqChangeFn(WaveformFreqChangeFn fn, void* state)
    {
        waveformFreqChangeFn_ = fn;
        waveformFreqChangeState_ = state;
    }
    
    void setWaveformUserConnectedFn(WaveformUserConnectedFn fn, void* state)
    {
        waveformUserConnectedFn_ = fn;
        waveformUserConnectedState_ = state;
    }
    
    void setWaveformUserDisconnectedFn(WaveformUserDisconnectedFn fn, void* state)
    {
        waveformUserDisconnectedFn_ = fn;
        waveformUserDisconnectedState_ = state;
    }

    void setWaveformGridSquareUpdateFn(WaveformGridSquareUpdateFn fn, void* state)
    {
        waveformGridSquareUpdateFn_ = fn;
        waveformGridSquareUpdateState_ = state;
    }

    void addSpot(std::string callsign);

protected:
    virtual void onConnect_() override;
    virtual void onDisconnect_() override;
    virtual void onReceive_(char* buf, int length) override;
    
private:
    WaveformConnectedFn waveformConnectedFn_;
    void* waveformConnectedState_;
    
    WaveformTransmitFn waveformTransmitFn_;
    void* waveformTransmitState_;

    WaveformCallsignRxFn waveformCallsignRxFn_;
    void* waveformCallsignRxState_;

    WaveformUserConnectedFn waveformUserConnectedFn_;
    void* waveformUserConnectedState_;

    WaveformUserDisconnectedFn waveformUserDisconnectedFn_;
    void* waveformUserDisconnectedState_;

    WaveformFreqChangeFn waveformFreqChangeFn_;
    void* waveformFreqChangeState_;

    WaveformGridSquareUpdateFn waveformGridSquareUpdateFn_;
    void* waveformGridSquareUpdateState_;

    std::stringstream inputBuffer_;
    ThreadedTimer commandHandlingTimer_;
    ThreadedTimer pingTimer_;
    int sequenceNumber_;
    std::string ip_;
    int activeSlice_;
    bool isLSB_;
    int txSlice_;
    bool isTransmitting_;
    bool isConnecting_;
    int vitaPort_;

    std::map<int, std::string> sliceFrequencies_;
    std::map<int, bool> activeSlices_;
    
    using FilterPair_ = std::pair<int, int>; // Low/high cut in Hz.
    std::vector<FilterPair_> filterWidths_;
    FilterPair_ currentWidth_;
    
    using HandlerMapFn_ = std::function<void(unsigned int rv, std::string message)>;
    std::map<int, HandlerMapFn_> responseHandlers_;
    
    void connect_(ThreadedTimer&);
    void disconnect_();
    void socketFinalCleanup_(bool reconnect);
    void commandResponseTimeout_(ThreadedTimer&);

    void initializeWaveform_();
    void createWaveform_(std::string name, std::string shortName, std::string underlyingMode);
    void cleanupWaveform_();
    
    void sendRadioCommand_(std::string command);
    void sendRadioCommand_(std::string command, std::function<void(unsigned int rv, std::string message)> fn);
    
    void processCommand_(std::string& command);
    void setFilter_(int low, int high);
    
    // Ping handling
    void pingRadio_(ThreadedTimer&);
};

#endif // FLEX_TCP_TASK_H
