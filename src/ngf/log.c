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

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <time.h>

#include <ngf/log.h>

#define LOG_CAT "log: "

static NLogLevel       _log_level       = N_LOG_LEVEL_ENTER;
static NLogTarget      _log_target      = N_LOG_TARGET_STDOUT;
static struct timespec _log_clock_start = { 0, 0 };

static int
n_log_syslog_priority_from_level (NLogLevel category)
{
    switch (category) {
        case N_LOG_LEVEL_ENTER:
            return LOG_DEBUG;

        case N_LOG_LEVEL_INFO:
            return LOG_INFO;

        case N_LOG_LEVEL_DEBUG:
            return LOG_DEBUG;

        case N_LOG_LEVEL_WARNING:
            return LOG_WARNING;

        case N_LOG_LEVEL_ERROR:
            return LOG_ERR;

        default:
            break;
    }

    return LOG_INFO;
}

void
n_log_set_level (NLogLevel level)
{
    _log_level = level;
}

NLogLevel
n_log_get_level ()
{
    return _log_level;
}

void
n_log_set_target (NLogTarget target)
{
    if (target == _log_target)
        return;

    _log_target = target;
    if (target == N_LOG_TARGET_SYSLOG) {
        openlog ("ngfd", 0, LOG_DAEMON);
        N_INFO (LOG_CAT "logging enabled to syslog");
    }
    else {
        N_INFO (LOG_CAT "logging enabled to stdout.");
        closelog();
    }
}

NLogTarget
n_log_get_target ()
{
    return _log_target;
}

static const char*
n_log_level_to_string (NLogLevel category)
{
    switch (category) {
        case N_LOG_LEVEL_ENTER:
            return "ENTER";

        case N_LOG_LEVEL_INFO:
            return "INFO";

        case N_LOG_LEVEL_DEBUG:
            return "DEBUG";

        case N_LOG_LEVEL_WARNING:
            return "WARNING";

        case N_LOG_LEVEL_ERROR:
            return "ERROR";

        default:
            break;
    }

    return "UNKNOWN";
}

static void
n_log_get_clock_stamp (char *buffer, size_t len)
{
    struct timespec res;
    struct timespec ts;
    long ms = 0;

    if (clock_gettime (CLOCK_MONOTONIC, &ts) < 0) {
        snprintf (buffer, len, "no_time");
        return;
    }

    res.tv_sec  = ts.tv_sec - _log_clock_start.tv_sec;
    res.tv_nsec = ts.tv_nsec - _log_clock_start.tv_nsec;
    ms = res.tv_nsec / 1000000;

    snprintf (buffer, len, "%lu.%.3lu",  (long) res.tv_sec, ms);
}

void
n_log_initialize (NLogLevel level)
{
    struct timespec ts;

    if (clock_gettime (CLOCK_MONOTONIC, &ts) < 0)
        return;

    _log_clock_start = ts;
    _log_level       = level;

    N_DEBUG (LOG_CAT "clock time reset");
}

void
n_log_message (NLogLevel category, const char *function, int line,
               const char *fmt, ...)
{
    char clock_stamp[256];
    char buf[256];

    (void) function;
    (void) line;

    if (category < _log_level || _log_target == N_LOG_TARGET_NONE)
        return;

    va_list fmt_args;
    va_start (fmt_args, fmt);
    vsnprintf (buf, 256, fmt, fmt_args);
    va_end (fmt_args);

    if (_log_target == N_LOG_TARGET_SYSLOG) {
        syslog (n_log_syslog_priority_from_level (category), "%s", buf);
    }
    else if (_log_target == N_LOG_TARGET_STDOUT) {
        n_log_get_clock_stamp (clock_stamp, 256);
        fprintf (stdout, "[%s] %s: %s\n", clock_stamp, n_log_level_to_string (category), buf);
    }
}
