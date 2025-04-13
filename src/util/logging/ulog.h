// *************************************************************************
//
// ulog v6.0.0 - A simple customizable logging library.
// https://github.com/an-dr/microlog
//
// *************************************************************************
//
// Original implementation by rxi: https://gitlab.com/rxi
// Modified by Andrei Gramakov: https://agramakov.me, mail@agramakov.me
//
// Copyright (c) 2024 Andrei Gramakov. All rights reserved.
//
// This file is licensed under the terms of the MIT license.
// For a copy, see: https://opensource.org/licenses/MIT
//
// *************************************************************************

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>


/* ============================================================================
   Configuration options
===============================================================================

| Feature               | Feature Default | Controlling Options               |
|-----------------------|-----------------|-----------------------------------|
| FEATURE_TIME          | OFF             | ULOG_HAVE_TIME                    |
| FEATURE_COLOR         | ON              | ULOG_NO_COLOR                     |
| FEATURE_CUSTOM_PREFIX | OFF             | ULOG_CUSTOM_PREFIX_SIZE           |
| FEATURE_FILE_STRING   | ON              | ULOG_HIDE_FILE_STRING             |
| FEATURE_SHORT_LEVELS  | OFF             | ULOG_SHORT_LEVEL_STRINGS          |
| FEATURE_EMOJI_LEVELS  | OFF             | ULOG_USE_EMOJI                    |
| FEATURE_EXTRA_OUTPUTS | OFF             | ULOG_EXTRA_OUTPUTS                |
| FEATURE_TOPICS        | OFF             | ULOG_TOPICS_NUM                   |

============================================================================ */

#ifdef ULOG_HAVE_TIME
#define FEATURE_TIME true
#else
#define FEATURE_TIME false
#endif


#ifndef ULOG_NO_COLOR
#define FEATURE_COLOR true
#else
#define FEATURE_COLOR false
#endif


#if ULOG_CUSTOM_PREFIX_SIZE > 0
#define FEATURE_CUSTOM_PREFIX true
#define CFG_CUSTOM_PREFIX_SIZE ULOG_CUSTOM_PREFIX_SIZE
#else
#define FEATURE_CUSTOM_PREFIX false
#define CFG_CUSTOM_PREFIX_SIZE 0
#endif


#ifndef ULOG_HIDE_FILE_STRING
#define FEATURE_FILE_STRING true
#else
#define FEATURE_FILE_STRING false
#endif


#if ULOG_EXTRA_OUTPUTS > 0
#define FEATURE_EXTRA_OUTPUTS true
#define CFG_EXTRA_OUTPUTS ULOG_EXTRA_OUTPUTS
#else
#define FEATURE_EXTRA_OUTPUTS false
#define CFG_EXTRA_OUTPUTS 0
#endif


#ifdef ULOG_SHORT_LEVEL_STRINGS
#define FEATURE_SHORT_LEVELS true
#else
#define FEATURE_SHORT_LEVELS false
#endif


#ifdef ULOG_USE_EMOJI

#define FEATURE_EMOJI_LEVELS true

#if FEATURE_SHORT_LEVELS
#define FEATURE_SHORT_LEVELS false
#warning "ULOG_USE_EMOJI overrides ULOG_SHORT_LEVEL_STRINGS! Disable ULOG_SHORT_LEVEL_STRINGS"
#endif  // FEATURE_SHORT_LEVELS

#else  // ULOG_USE_EMOJI

#define FEATURE_EMOJI_LEVELS false

#endif  // ULOG_USE_EMOJI

#if ULOG_TOPICS_NUM > 0
#define FEATURE_TOPICS true
#define CFG_TOPICS_DINAMIC_ALLOC false
#define CFG_TOPICS_NUM ULOG_TOPICS_NUM

#elif ULOG_TOPICS_NUM == -1
#define FEATURE_TOPICS true
#define CFG_TOPICS_DINAMIC_ALLOC true
#define CFG_TOPICS_NUM -1

#else  // ULOG_TOPICS_NUM == 0
#define FEATURE_TOPICS false

#endif  // ULOG_TOPICS_NUM


/* ============================================================================
   Core Functionality
============================================================================ */

#if FEATURE_TIME
#include <time.h>
#endif

enum { LOG_TRACE,
       LOG_DEBUG,
       LOG_INFO,
       LOG_WARN,
       LOG_ERROR,
       LOG_FATAL };

#define log_trace(...) ulog_log(LOG_TRACE, __FILE__, __LINE__, NULL, __VA_ARGS__)
#define log_debug(...) ulog_log(LOG_DEBUG, __FILE__, __LINE__, NULL, __VA_ARGS__)
#define log_info(...) ulog_log(LOG_INFO, __FILE__, __LINE__, NULL, __VA_ARGS__)
#define log_warn(...) ulog_log(LOG_WARN, __FILE__, __LINE__, NULL, __VA_ARGS__)
#define log_error(...) ulog_log(LOG_ERROR, __FILE__, __LINE__, NULL, __VA_ARGS__)
#define log_fatal(...) ulog_log(LOG_FATAL, __FILE__, __LINE__, NULL, __VA_ARGS__)


/// @brief Event structure
typedef struct {
    const char *message;          // Message format string
    va_list message_format_args;  // Format arguments

#if FEATURE_TOPICS
    int topic;
#endif

#if FEATURE_TIME
    struct tm *time;
#endif

    const char *file;  // Event file name
    int line;          // Event line number
    int level;         // Event debug level
} ulog_Event;

typedef void (*ulog_LogFn)(ulog_Event *ev, void *arg);

/// @brief Returns the string representation of the level
const char *ulog_get_level_string(int level);

/// @brief Sets the debug level
/// @param level - Debug level
void ulog_set_level(int level);

/// @brief Sets the quiet mode
/// @param enable - Quiet mode
void ulog_set_quiet(bool enable);

/// @brief Write event content to a buffer as a log message
/// @param ev - Event
/// @param out_buf - Output buffer
/// @param out_buf_size - Output buffer size
/// @return 0 if success, -1 if failed
int ulog_event_to_cstr(ulog_Event *ev, char *out, size_t out_size);

/// @brief Logs the message
/// @param level - Debug level
/// @param file - File name
/// @param line - Line number
/// @param topic - Topic name
/// @param message - Message format string
/// @param ... - Format arguments
void ulog_log(int level, const char *file, int line, const char *topic, const char *message, ...);

/* ============================================================================
   Core Functionality: Thread Safety
============================================================================ */

typedef void (*ulog_LockFn)(bool lock, void *lock_arg);

/// @brief  Sets the lock function and user data
/// @param function - Lock function
/// @param lock_arg - User data
void ulog_set_lock(ulog_LockFn function, void *lock_arg);

/* ============================================================================
   Feature: Custom Prefix
============================================================================ */
#if FEATURE_CUSTOM_PREFIX

typedef void (*ulog_PrefixFn)(ulog_Event *ev, char *prefix, size_t prefix_size);

/// @brief Sets the prefix function
/// @param function - Prefix function
void ulog_set_prefix_fn(ulog_PrefixFn function);

#else  // FEATURE_CUSTOM_PREFIX

#define ulog_set_prefix_fn(...)

#endif  // FEATURE_CUSTOM_PREFIX

/* ============================================================================
   Feature: Extra Outputs
============================================================================ */
#if FEATURE_EXTRA_OUTPUTS

/// @brief Adds a callback
/// @param function - Callback function
/// @param arg - Optional argument that will be added to the event to be
///              processed by the callback
/// @param level - Debug level
/// @return 0 if success, -1 if failed
int ulog_add_callback(ulog_LogFn function, void *arg, int level);

/// @brief Add file callback
/// @param fp - File pointer
/// @param level - Debug level
/// @return 0 if success, -1 if failed
int ulog_add_fp(FILE *fp, int level);

#else  // FEATURE_EXTRA_OUTPUTS

#define ulog_add_callback(...)
#define ulog_add_fp(...)

#endif  // FEATURE_EXTRA_OUTPUTS

/* ============================================================================
   Feature: Log Topics
============================================================================ */

#define TOPIC_NOT_FOUND 0x7FFFFFFF
#define logt_trace(TOPIC_NAME, ...) ulog_log(LOG_TRACE, __FILE__, __LINE__, TOPIC_NAME, __VA_ARGS__)
#define logt_debug(TOPIC_NAME, ...) ulog_log(LOG_DEBUG, __FILE__, __LINE__, TOPIC_NAME, __VA_ARGS__)
#define logt_info(TOPIC_NAME, ...) ulog_log(LOG_INFO, __FILE__, __LINE__, TOPIC_NAME, __VA_ARGS__)
#define logt_warn(TOPIC_NAME, ...) ulog_log(LOG_WARN, __FILE__, __LINE__, TOPIC_NAME, __VA_ARGS__)
#define logt_error(TOPIC_NAME, ...) ulog_log(LOG_ERROR, __FILE__, __LINE__, TOPIC_NAME, __VA_ARGS__)
#define logt_fatal(TOPIC_NAME, ...) ulog_log(LOG_FATAL, __FILE__, __LINE__, TOPIC_NAME, __VA_ARGS__)

#if FEATURE_TOPICS

/// @brief Adds a topic
/// @param topic_name
/// @param enable
/// @return Topic ID if success, -1 if failed
int ulog_add_topic(const char *topic_name, bool enable);

/// @brief Gets the topic ID
/// @param topic_name
/// @return  Topic ID if success, -1 if failed, TOPIC_NOT_FOUND if not found
int ulog_get_topic_id(const char *topic_name);

/// @brief Enables the topic
/// @param topic_name - Topic name
/// @return 0 if success, -1 if failed
int ulog_enable_topic(const char *topic_name);

/// @brief Disables the topic
/// @param topic_name - Topic name
/// @return 0 if success, -1 if failed
int ulog_disable_topic(const char *topic_name);

/// @brief Enables all topics
int ulog_enable_all_topics(void);

/// @brief Disables all topics
int ulog_disable_all_topics(void);

#else  // FEATURE_TOPICS

#define ulog_add_topic(...)
#define ulog_get_topic_id(...)
#define ulog_enable_topic(...)
#define ulog_disable_topic(...)
#define ulog_enable_all_topics(...)
#define ulog_disable_all_topics(...)

#endif  // FEATURE_TOPICS

#ifdef __cplusplus
}
#endif
