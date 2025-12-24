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
#include <set>
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
    using WaveformCallsignRxFn = std::function<void(FlexTcpTask&, std::string const&, void*)>;
    using WaveformGridSquareUpdateFn = std::function<void(FlexTcpTask&, std::string const&, void*)>;
    using WaveformFreqChangeFn = std::function<void(FlexTcpTask&, uint64_t, void*)>;
    using WaveformUserConnectedFn = std::function<void(FlexTcpTask&, void*)>;
    using WaveformUserDisconnectedFn = std::function<void(FlexTcpTask&, void*)>;
    using WaveformAddValidStreamIdentifiersFn = std::function<void(FlexTcpTask&, uint32_t, uint32_t, uint32_t, uint32_t, void*)>;
    using WaveformSnrMeterIdentifiersFn = std::function<void(FlexTcpTask&, uint16_t, void*)>;

    FlexTcpTask(int vitaPort);
    virtual ~FlexTcpTask();
    
    void setWaveformSnrMeterIdentifiersFn(WaveformSnrMeterIdentifiersFn fn, void* state)
    {
        waveformSnrMeterIdentifiersFn_ = std::move(fn);
        waveformSnrMeterIdentifiersState_ = state;
    }
    
    void setWaveformConnectedFn(WaveformConnectedFn fn, void* state)
    {
        waveformConnectedFn_ = std::move(fn);
        waveformConnectedState_ = state;
    }
    
    void setWaveformTransmitFn(WaveformTransmitFn fn, void* state)
    {
        waveformTransmitFn_ = std::move(fn);
        waveformTransmitState_ = state;
    }

    void setWaveformCallsignRxFn(WaveformCallsignRxFn fn, void* state)
    {
        waveformCallsignRxFn_ = std::move(fn);
        waveformCallsignRxState_ = state;
    }
    
    void setWaveformFreqChangeFn(WaveformFreqChangeFn fn, void* state)
    {
        waveformFreqChangeFn_ = std::move(fn);
        waveformFreqChangeState_ = state;
    }
    
    void setWaveformUserConnectedFn(WaveformUserConnectedFn fn, void* state)
    {
        waveformUserConnectedFn_ = std::move(fn);
        waveformUserConnectedState_ = state;
    }
    
    void setWaveformUserDisconnectedFn(WaveformUserDisconnectedFn fn, void* state)
    {
        waveformUserDisconnectedFn_ = std::move(fn);
        waveformUserDisconnectedState_ = state;
    }

    void setWaveformGridSquareUpdateFn(WaveformGridSquareUpdateFn fn, void* state)
    {
        waveformGridSquareUpdateFn_ = std::move(fn);
        waveformGridSquareUpdateState_ = state;
    }

    void setWaveformAddValidStreamIdentifiersFn(WaveformAddValidStreamIdentifiersFn fn, void* state)
    {
        waveformAddValidStreamIdentifiersFn_ = std::move(fn);
        waveformAddValidStreamIdentifiersState_ = state;
    }

    void addSpot(std::string const& callsign);

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

    WaveformAddValidStreamIdentifiersFn waveformAddValidStreamIdentifiersFn_;
    void* waveformAddValidStreamIdentifiersState_;

    WaveformSnrMeterIdentifiersFn waveformSnrMeterIdentifiersFn_;
    void* waveformSnrMeterIdentifiersState_;

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
    std::set<int> activeFreeDVSlices_;
    
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
    void createWaveform_(std::string const& name, std::string const& shortName, std::string const& underlyingMode);
    void cleanupWaveform_();
    
    void sendRadioCommand_(std::string const& command);
    void sendRadioCommand_(std::string const& command, std::function<void(unsigned int rv, std::string const& message)> fn);
    
    void processCommand_(std::string& command);
    void setFilter_(int low, int high);
    
    // Ping handling
    void pingRadio_(ThreadedTimer&);
};

#endif // FLEX_TCP_TASK_H
