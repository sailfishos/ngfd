/*
 * ngfd - Non-graphic feedback daemon
 *
 * Copyright (C) 2010 Nokia Corporation.
 * Contact: Xun Chen <xun.chen@nokia.com>
 *
 * This work is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef N_LOG_H
#define N_LOG_H

#include <stdarg.h>

typedef enum _NLogTarget
{
    /** Suppress logging */
    N_LOG_TARGET_NONE = 0,
    
    /** Direct logging to stdout */
    N_LOG_TARGET_STDOUT,
    
    /** Direct logging to syslog */
    N_LOG_TARGET_SYSLOG
} NLogTarget;

/** Logging levels. Selected level also includes all messages from higher levels */
typedef enum _NLogLevel
{
    /** Function enter messages */
    N_LOG_LEVEL_ENTER   = 0,
    /** Debug messages */
    N_LOG_LEVEL_DEBUG   = 1,
    /** Info messages */
    N_LOG_LEVEL_INFO    = 2,
    /** Warning messages */
    N_LOG_LEVEL_WARNING = 3,
    /** Error messages */
    N_LOG_LEVEL_ERROR   = 4,
    /** Suppress logging */
    N_LOG_LEVEL_NONE    = 5
} NLogLevel;

/** Initialize logging with selected level
 * @param level Logging level
 */
void n_log_initialize (NLogLevel level);

/** Change logging level
 * @param level Logging level
 */
void n_log_set_level  (NLogLevel level);

/** Select log target
 * @param target Log target
 */
void n_log_set_target (NLogTarget target);

/** Get current log target
 * @param target Log target
 */
NLogTarget n_log_get_target ();

/** Log message. Use convenience functions to send actual messages.
 * @param level Logging level
 * @param function Function to which the message is related to
 * @param line Code line where log message was sent
 * @param fmt printf style formatting string
 * @param ... Variables
 */
void n_log_message    (NLogLevel level, const char *function, int line, const char *fmt, ...);

/** Log function enter message */
#define N_ENTER(...) \
    do {  n_log_message (N_LOG_LEVEL_ENTER, (const char*) __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)
    
/** Log debug message */
#define N_DEBUG(...) \
    do { n_log_message (N_LOG_LEVEL_DEBUG, (const char*) __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)

/** Log info message */
#define N_INFO(...) \
    do { n_log_message (N_LOG_LEVEL_INFO, (const char*) __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)

/** Log warning message */
#define N_WARNING(...) \
    do { n_log_message (N_LOG_LEVEL_WARNING, (const char*) __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)

/** Log error message */
#define N_ERROR(...) \
    do { n_log_message (N_LOG_LEVEL_ERROR, (const char*) __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)

#endif /* N_LOG_H */
