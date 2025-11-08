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
#include <future>
#include <string>
#include <sstream>
#include <inttypes.h>
#include <unistd.h>
#include <sys/socket.h>

#include "FlexTcpTask.h"
#include "FlexKeyValueParser.h"

#include "../util/logging/ulog.h"

using namespace std::placeholders;
using namespace std::chrono_literals;

FlexTcpTask::FlexTcpTask(int vitaPort)
    : commandHandlingTimer_(500, std::bind(&FlexTcpTask::commandResponseTimeout_, this, _1), true) /* time out waiting for command response after 0.5 second */
    , pingTimer_(10000, std::bind(&FlexTcpTask::pingRadio_, this, _1), true) /* pings radio every 10 seconds to verify connectivity */
    , sequenceNumber_(0)
    , activeSlice_(-1)
    , txSlice_(-1)
    , isTransmitting_(false)
    , isConnecting_(false)
    , vitaPort_(vitaPort)
{    
    // Initialize filter widths. These are sent to SmartSDR on mode changes.
    filterWidths_.push_back(FilterPair_(750, 2250)); // RADEV1
    
    // Default to RADEV1.
    currentWidth_ = filterWidths_[0];
}

FlexTcpTask::~FlexTcpTask()
{
    enqueue_([&]() {
        cleanupWaveform_();
    });
    std::this_thread::sleep_for(1s);
    
    std::shared_ptr<std::promise<void>> prom = std::make_shared<std::promise<void>>();
    auto fut = prom->get_future();
    enqueue_([prom]() {
        prom->set_value();
    });
    fut.wait();
}

void FlexTcpTask::onReceive_(char* buf, int length)
{
    for (int index = 0; index < length; index++)
    {
        // Append to input buffer. Then if we have a full line, we can process
        // accordingly.
        if (*buf == '\n')
        {
            std::string line = inputBuffer_.str();
            processCommand_(line);
            inputBuffer_.str("");
        }
        else
        {
            inputBuffer_.write(buf, 1);
        }
        buf++;
    }
}

void FlexTcpTask::socketFinalCleanup_(bool)
{
    // Report disconnection
    activeSlice_ = -1;
    isLSB_ = false;
    txSlice_ = -1;

    responseHandlers_.clear();
    inputBuffer_.clear();
    activeFreeDVSlices_.clear();

    commandHandlingTimer_.stop();
    pingTimer_.stop();
    isConnecting_ = false;
}

void FlexTcpTask::onConnect_()
{
    enqueue_([&]() {
        isConnecting_ = false;

        log_info("Connected to radio successfully");
        sequenceNumber_ = 0;
    
        // Report successful connection
        if (waveformConnectedFn_)
        {
            waveformConnectedFn_(*this, waveformConnectedState_);
        }
        
        // Start ping timer
        pingTimer_.start();
    });
}

void FlexTcpTask::onDisconnect_()
{
    pingTimer_.stop();
}

void FlexTcpTask::initializeWaveform_()
{
    // Send needed commands to initialize the waveform. This is from the reference
    // waveform implementation.
    createWaveform_("FreeDV-USB", "FDVU", "DIGU");
    createWaveform_("FreeDV-LSB", "FDVL", "DIGL");
    
    // subscribe to slice updates, needed to detect when we enter FDVU/FDVL mode
    sendRadioCommand_("sub slice all");

    // subscribe to GPS updates, needed for FreeDV Reporter
    sendRadioCommand_("sub gps all");
}

void FlexTcpTask::cleanupWaveform_()
{
    std::stringstream ss;
    
    // Change mode back to something that exists.
    if (activeSlice_ >= 0)
    {
        ss << "slice set " << activeSlice_ << " mode=";
        if (isLSB_) ss << "LSB";
        else ss << "USB";
        
        sendRadioCommand_(ss.str().c_str(), [&](unsigned int, std::string) {
            // Recursively call ourselves again to actually remove the waveform
            // once we get a response for this command.
            activeSlice_ = -1;
            activeFreeDVSlices_.clear();
            cleanupWaveform_();
        });
        
        return;
    }
    
    sendRadioCommand_("unsub slice all");
    sendRadioCommand_("waveform remove FreeDV-USB");
    sendRadioCommand_("waveform remove FreeDV-LSB", [&](unsigned int, std::string) {
        // We can disconnect after we've fully unregistered the waveforms.
        socketFinalCleanup_(false);
    });
}

void FlexTcpTask::createWaveform_(std::string name, std::string shortName, std::string underlyingMode)
{
    log_info("Creating waveform %s (abbreviated %s in SmartSDR)", name.c_str(), shortName.c_str());

    // Unregister waveform in case we didn't shut down cleanly.
    sendRadioCommand_(std::string("waveform remove ") + name);

    // Actually create the waveform.
    std::string waveformCommand = "waveform create name=" + name + " mode=" + shortName + " underlying_mode=" + underlyingMode + " version=2.0.0";
    std::string setPrefix = "waveform set " + name + " ";
    sendRadioCommand_(waveformCommand, [&, setPrefix](unsigned int rv, std::string) {
        if (rv == 0)
        {
            // Set the filter-related settings for the just-created waveform.
            sendRadioCommand_(setPrefix + "tx=1");
            sendRadioCommand_(setPrefix + "rx_filter depth=256");
            sendRadioCommand_(setPrefix + "tx_filter depth=256");

            // Link waveform to our UDP audio stream.
            std::stringstream ss;
            ss << "udpport=" << vitaPort_;
            sendRadioCommand_(setPrefix + ss.str().c_str());
        }
    });
}

void FlexTcpTask::sendRadioCommand_(std::string command)
{
    sendRadioCommand_(command, std::function<void(int rv, std::string message)>());
}

void FlexTcpTask::sendRadioCommand_(std::string command, std::function<void(unsigned int rv, std::string message)> fn)
{
    std::ostringstream ss;

    log_info("Sending '%s' as command %d", command.c_str(), sequenceNumber_);    
    ss << "C" << (sequenceNumber_) << "|" << command << "\n";
    
    send(ss.str().c_str(), ss.str().length());
    responseHandlers_[sequenceNumber_++] = fn;
    commandHandlingTimer_.stop();
    commandHandlingTimer_.start();
}

void FlexTcpTask::commandResponseTimeout_(ThreadedTimer&)
{
    enqueue_([&]() {
        // We timed out waiting for a response, just go ahead and call handlers so that
        // processing can continue.
        log_warn("Timed out waiting for response from radio.");
        for (auto& kvp : responseHandlers_)
        {
            if (kvp.second)
            {
                log_warn("Calling response handler for command %d", kvp.first);
                kvp.second(0xFFFFFFFF, "Timed out waiting for response from radio");
            }
        }
        responseHandlers_.clear();
    });
}

void FlexTcpTask::processCommand_(std::string& command)
{
    if (command[0] == 'V')
    {
        // Version information from radio
        log_info("Radio is using protocol version %s", &command.c_str()[1]);
    }
    else if (command[0] == 'H')
    {
        // Received connection's handle. We don't currently do anything with this other
        // than trigger waveform creation.
        log_info("Connection handle is %s", &command.c_str()[1]);
        initializeWaveform_();
    }
    else if (command[0] == 'R')
    {
        log_info("Received response %s", command.c_str());
        
        // Received response for a command.
        command.erase(0, 1);
        std::stringstream ss(command);
        int seq = 0;
        unsigned int rv = 0;
        char temp = 0;
        
        // Get sequence number and return value
        ss >> seq >> temp >> std::hex >> rv;
        
        if (rv != 0)
        {
            log_error("Command %d returned error %x", seq, rv);
        }
        
        // If we have a valid command handler, call it now
        if (responseHandlers_[seq])
        {
            responseHandlers_[seq](rv, ss.str());
        }
        responseHandlers_.erase(seq);

        // Stop timer if we're not waiting for any more responses.
        if (responseHandlers_.size() == 0)
        {
            commandHandlingTimer_.stop();
        }
    }
    else if (command[0] == 'S')
    {
        log_info("Received status update %s", command.c_str());
        
        command.erase(0, 1);
        std::stringstream ss(command);
        unsigned int clientId = 0;
        std::string statusName;
        
        ss >> std::hex >> clientId;
        char pipe = 0;
        ss >> pipe >> statusName;
        
        if (statusName == "slice")
        {
            log_info("Detected slice update");
            
            int sliceId = 0;
            ss >> std::dec >> sliceId;
            
            auto parameters = FlexKeyValueParser::GetCommandParameters(ss);

            auto tx = parameters.find("tx");
            if (tx != parameters.end() && tx->second == "1")
            {
                txSlice_ = sliceId;
            }

            auto rfFrequency = parameters.find("RF_frequency");
            if (rfFrequency != parameters.end())
            {
                sliceFrequencies_[sliceId] = rfFrequency->second;

                // Report new frequency to any listening reporters
                if (activeSlice_ == sliceId)
                {
                    // Frequency reported by Flex is in MHz but reporters expect
                    // it in Hz.
                    uint64_t freqHz = atof(rfFrequency->second.c_str()) * 1000000;
                    if (waveformFreqChangeFn_)
                    {
                        waveformFreqChangeFn_(*this, freqHz, waveformFreqChangeState_);
                    }
                }
            }
            
            auto isActive = parameters.find("in_use");
            if (isActive != parameters.end())
            {
                activeSlices_[sliceId] = isActive->second == "1" ? true : false;
                if (sliceId == activeSlice_ && !activeSlices_[sliceId])
                {
                    if (waveformUserDisconnectedFn_)
                    {
                        waveformUserDisconnectedFn_(*this, waveformUserDisconnectedState_);
                    }
                    activeFreeDVSlices_.erase(sliceId);
                    if (activeFreeDVSlices_.size() > 0)
                    {
                        activeSlice_ = *activeFreeDVSlices_.begin()
                    }
                    else
                    {
                        activeSlice_ = -1;
                    }
                }
            }
            
            auto mode = parameters.find("mode");
            if (mode != parameters.end())
            {
                if (mode->second == "FDVU" || mode->second == "FDVL")
                {
                    if (sliceId != activeSlice_)
                    {
                        log_info("Switching slice %d to FreeDV mode", sliceId);
                        
                        if (activeSlice_ == -1)
                        {
                            if (waveformUserConnectedFn_)
                            {
                                waveformUserConnectedFn_(*this, waveformUserConnectedState_);
                            }
                        }
                        else 
                        {
                            log_warn("Attempted to activate FDVU/FDVL from a second slice (id = %d, active = %d)", sliceId, activeSlice_);
                        }

                        // User wants to use the waveform.
                        activeSlice_ = sliceId;
                        activeFreeDVSlices_.insert(sliceId);

                        // Ensure that we connect to any reporting services as appropriate
                        uint64_t freqHz = atof(sliceFrequencies_[activeSlice_].c_str()) * 1000000;
                        if (waveformFreqChangeFn_)
                        {
                            waveformFreqChangeFn_(*this, freqHz, waveformFreqChangeState_);
                        }
                    }
                    
                    // Set the filter corresponding to the current mode.
                    isLSB_ = mode->second == "FDVL";
                    setFilter_(currentWidth_.first, currentWidth_.second);
                }
                else if (sliceId == activeSlice_)
                {
                    // Ensure that we disconnect from any reporting services as appropriate
                    if (waveformUserDisconnectedFn_)
                    {
                        waveformUserDisconnectedFn_(*this, waveformUserDisconnectedState_);
                    }

                    activeFreeDVSlices_.erase(sliceId);
                    if (activeFreeDVSlices_.size() > 0)
                    {
                        activeSlice_ = *activeFreeDVSlices_.begin()
                    }
                    else
                    {
                        activeSlice_ = -1;
                    }
                }
            }
        }
        else if (statusName == "interlock")
        {
            log_info("Detected interlock update");
            
            auto parameters = FlexKeyValueParser::GetCommandParameters(ss);
            auto state = parameters.find("state");
            auto source = parameters.find("source");
            
            if (state != parameters.end() && state->second == "PTT_REQUESTED" &&
                activeSlice_ == txSlice_ && source->second != "TUNE")
            {
                // Going into transmit mode
                log_info("Radio went into transmit");
                isTransmitting_ = true;
                
                if (waveformTransmitFn_)
                {
                    waveformTransmitFn_(*this, TRANSMITTING, waveformTransmitState_);
                }
            }
            else if (state != parameters.end() && state->second == "UNKEY_REQUESTED")
            {
                // Going back into receive, but not there yet. TX FIFO needs to be empty for 10ms
                // for radio to switch back to READY.
                log_info("Radio went out of transmit");
                if (waveformTransmitFn_)
                {
                    waveformTransmitFn_(*this, ENDING_TX, waveformTransmitState_);
                }
            }
            else if (state != parameters.end() && state->second == "READY")
            {
                isTransmitting_ = false;
                if (waveformTransmitFn_)
                {
                    waveformTransmitFn_(*this, RECEIVING, waveformTransmitState_);
                }
            }
        }
        else if (statusName == "radio")
        {
            log_info("Detected radio update");
            auto parameters = FlexKeyValueParser::GetCommandParameters(ss);
            auto callsign = parameters.find("callsign");
            if (callsign != parameters.end())
            {
                std::string callsignStr = callsign->second;
                log_info("Got callsign %s", callsignStr.c_str());
                if (waveformCallsignRxFn_)
                {
                    waveformCallsignRxFn_(*this, callsignStr, waveformCallsignRxState_);
                }
            }
        }
        else if (statusName == "gps")
        {
            log_info("Detected GPS update");
            auto parameters = FlexKeyValueParser::GetCommandParameters(ss, '#'); // XXX - different delimiter than other update types
            auto gridSquare = parameters.find("grid");
            if (gridSquare != parameters.end())
            {
                std::string gridSquareStr = gridSquare->second;
                log_info("Got grid square %s", gridSquareStr.c_str());
                if (waveformGridSquareUpdateFn_)
                {
                    waveformGridSquareUpdateFn_(*this, gridSquareStr, waveformGridSquareUpdateState_);
                }
            }
        }
        else
        {
            log_warn("Unknown status update type %s", statusName.c_str());
        }
    }
    else
    {
        log_warn("Got unhandled command %s", command.c_str());
    }
}

void FlexTcpTask::addSpot(std::string callsign)
{
    enqueue_([=]() {
        if (activeSlice_ >= 0)
        {
            std::stringstream ss;
            ss << "spot add rx_freq=" << sliceFrequencies_[activeSlice_] << " callsign=" << callsign << " mode=FREEDV timestamp=" << time(NULL); //lifetime_seconds=300";
            sendRadioCommand_(ss.str());
        }
    });
}

void FlexTcpTask::setFilter_(int low, int high)
{
    if (activeSlice_ >= 0)
    {
        int low_cut = low;
        int high_cut = high;

        if (isLSB_)
        {
            low_cut = -high;
            high_cut = -low;
        }

        std::stringstream ss;
        ss << "filt " << activeSlice_ << " " << low_cut << " " << high_cut;
        sendRadioCommand_(ss.str());
    }
}

void FlexTcpTask::pingRadio_(ThreadedTimer&)
{
    enqueue_([&]() {
        // Sends ping command to radio every ten seconds. We don't care about the
        // response, just that the TCP/IP subsystem doesn't error out while trying
        // to send the request. If the radio did go away (e.g. sudden power cut),
        // the send logic will take care of triggering reconnection.
        sendRadioCommand_("ping");
    });
}
