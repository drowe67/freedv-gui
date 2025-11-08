//=========================================================================
// Name:            ulog_logger.h
// Purpose:         Custom websocketpp logger using ulog log library,
//                  derived from basic logger.
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

#ifndef WEBSOCKETPP_LOGGER_ULOG_H
#define WEBSOCKETPP_LOGGER_ULOG_H

/* Need a way to print a message to the log
 *
 * - timestamps
 * - channels
 * - thread safe
 * - output to stdout or file
 * - selective output channels, both compile time and runtime
 * - named channels
 * - ability to test whether a log message will be printed at compile time
 *
 */

#include <websocketpp/logger/levels.hpp>

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/stdint.hpp>
#include <websocketpp/common/time.hpp>

#include <ctime>
#include <iostream>
#include <iomanip>
#include <string>

#include "logging/ulog.h"

namespace websocketpp {
namespace log {

template <typename concurrency, typename names>
class ulog {
public:
    ulog(channel_type_hint::value h =
        channel_type_hint::access)
      : m_static_channels(0xffffffff)
      , m_dynamic_channels(0) { (void)h; }

    ulog(std::ostream *)
      : m_static_channels(0xffffffff)
      , m_dynamic_channels(0) {}

    ulog(level c, channel_type_hint::value h =
        channel_type_hint::access)
      : m_static_channels(c)
      , m_dynamic_channels(0) { (void)h; }

    ulog(level c, std::ostream *)
      : m_static_channels(c)
      , m_dynamic_channels(0) {}

    /// Destructor
    ~ulog() {}

    /// Copy constructor
    ulog(ulog<concurrency,names> const & other)
     : m_static_channels(other.m_static_channels)
     , m_dynamic_channels(other.m_dynamic_channels)
    {}
    
#ifdef _WEBSOCKETPP_DEFAULT_DELETE_FUNCTIONS_
    // no copy assignment operator because of const member variables
    ulog & operator=(ulog<concurrency,names> const &) = delete;
#endif // _WEBSOCKETPP_DEFAULT_DELETE_FUNCTIONS_

#ifdef _WEBSOCKETPP_MOVE_SEMANTICS_
    /// Move constructor
    ulog(ulog<concurrency,names> && other) noexcept
     : m_static_channels(other.m_static_channels)
     , m_dynamic_channels(other.m_dynamic_channels)
    {}

#ifdef _WEBSOCKETPP_DEFAULT_DELETE_FUNCTIONS_
    // no move assignment operator because of const member variables
    ulog & operator=(ulog<concurrency,names> &&) = delete;
#endif // _WEBSOCKETPP_DEFAULT_DELETE_FUNCTIONS_

#endif // _WEBSOCKETPP_MOVE_SEMANTICS_

    void set_channels(level channels) {
        if (channels == names::none) {
            clear_channels(names::all);
            return;
        }

        scoped_lock_type lock(m_lock);
        m_dynamic_channels |= (channels & m_static_channels);
    }

    void clear_channels(level channels) {
        scoped_lock_type lock(m_lock);
        m_dynamic_channels &= ~channels;
    }

    /// Write a string message to the given channel
    /**
     * @param channel The channel to write to
     * @param msg The message to write
     */
    void write(level channel, std::string const & msg) {
        scoped_lock_type lock(m_lock);
        if (!this->dynamic_test(channel)) { return; }
        log_info("[%s] %s", names::channel_name(channel), msg.c_str());
    }

    /// Write a cstring message to the given channel
    /**
     * @param channel The channel to write to
     * @param msg The message to write
     */
    void write(level channel, char const * msg) {
        scoped_lock_type lock(m_lock);
        if (!this->dynamic_test(channel)) { return; }
        log_info("[%s] %s", names::channel_name(channel), msg);
    }

    _WEBSOCKETPP_CONSTEXPR_TOKEN_ bool static_test(level channel) const {
        return ((channel & m_static_channels) != 0);
    }

    bool dynamic_test(level channel) {
        return ((channel & m_dynamic_channels) != 0);
    }

protected:
    typedef typename concurrency::scoped_lock_type scoped_lock_type;
    typedef typename concurrency::mutex_type mutex_type;
    mutex_type m_lock;

private:
    // The timestamp does not include the time zone, because on Windows with the
    // default registry settings, the time zone would be written out in full,
    // which would be obnoxiously verbose.
    //
    // TODO: find a workaround for this or make this format user settable
    static std::ostream & timestamp(std::ostream & os) {
        std::time_t t = std::time(NULL);
        std::tm lt = lib::localtime(t);
        #ifdef _WEBSOCKETPP_PUTTIME_
            return os << std::put_time(&lt,"%Y-%m-%d %H:%M:%S");
        #else // Falls back to strftime, which requires a temporary copy of the string.
            char buffer[20];
            size_t result = std::strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S",&lt);
            return os << (result == 0 ? "Unknown" : buffer);
        #endif
    }

    level const m_static_channels;
    level m_dynamic_channels;
    std::ostream * m_out;
};

} // log
} // websocketpp

#endif // WEBSOCKETPP_LOGGER_ULOG_H
