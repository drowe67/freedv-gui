//=========================================================================
// Name:            FlexTcpTask.h
// Purpose:         TCP handler for Flex waveform.
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

#ifndef FLEX_TCP_TASK_H
#define FLEX_TCP_TASK_H

#include <sstream>
#include <map>
#include <set>
#include <functional>
#include <future>

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

    void addSpot(std::string const& callsign, int snr, int timeoutSeconds);

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
    std::shared_ptr<std::promise<void>> deregisterPromise_;   // so we don't need to wait a fixed amount of time during deinit

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
