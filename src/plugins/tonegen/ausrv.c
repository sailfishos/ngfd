/*************************************************************************
This file is part of ngfd / tone-generator

Copyright (C) 2010 Nokia Corporation.
              2015 Jolla Ltd.

This library is free software; you can redistribute
it and/or modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation
version 2.1 of the License.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
USA.
*************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <ngf/log.h>
#include <trace/trace.h>

#include "stream.h"
#include "ausrv.h"

#if PA_API_VERSION < 9
#error Invalid PulseAudio API version
#endif

#define DEFAULT_SERVER  "default Pulse Audio"
#define CONNECT_DELAY   10                              /* in seconds */

#define LOG_CAT "tonegen-ausrv: "

static void set_connection_status(struct ausrv *ausrv, bool connected);
static void context_callback(pa_context *, void *);
static void event_callback(pa_context *, pa_subscription_event_type_t,
                           uint32_t, void *);
static void connect_server(struct ausrv *);
static void restart_timer(struct ausrv *, int);
static void cancel_timer(struct ausrv *);

static const char *pa_client_name = "ngf-tonegen-plugin";


int ausrv_init(void)
{
    return 0;
}

void ausrv_exit(void)
{
}

struct ausrv *ausrv_create(struct tonegend *tonegend, const char *server)
{
    pa_glib_mainloop   *mainloop = NULL;
    struct ausrv       *ausrv;
    pa_mainloop_api    *mainloop_api;

    if ((ausrv = calloc(1, sizeof(struct ausrv))) == NULL) {
        N_ERROR(LOG_CAT "%s(): Can't allocate memory", __FUNCTION__);
        goto failed;
    }

    if ((mainloop = pa_glib_mainloop_new(NULL)) == NULL) {
        N_ERROR(LOG_CAT "%s(): pa_glib_mainloop_new() failed", __FUNCTION__);
        goto failed;
    }

    mainloop_api = pa_glib_mainloop_get_api(mainloop);

    if (pa_signal_init(mainloop_api) < 0) {
        N_ERROR(LOG_CAT "%s(): pa_signal_init() failed", __FUNCTION__);
        goto failed;
    }
    
    ausrv->tonegend = tonegend;
    ausrv->server   = strdup(server ?: DEFAULT_SERVER);
    ausrv->mainloop = mainloop;

    connect_server(ausrv);

    return ausrv;

 failed:
    if (mainloop != NULL)
        pa_glib_mainloop_free(mainloop);

    if (ausrv != NULL)
        free(ausrv);

    return NULL;

}

void ausrv_destroy(struct ausrv *ausrv)
{
    if (ausrv != NULL) {
        stream_kill_all(ausrv);

        if (ausrv->context != NULL)
            pa_context_unref(ausrv->context);
        
        if (ausrv->mainloop != NULL)
            pa_glib_mainloop_free(ausrv->mainloop);
        
        free(ausrv->server);
        free(ausrv);
    }
}


static void set_connection_status(struct ausrv *ausrv, bool connected)
{
    if (ausrv->connected != connected) {
        ausrv->connected = connected;
        TRACE("%s '%s' server", ausrv->connected ? "Connected to" : "Disconnected from", ausrv->server);
    }
}

static void context_callback(pa_context *context, void *userdata)
{
    struct ausrv *ausrv = (struct ausrv *)userdata;
    int           err   = 0;
    const char   *strerr;

    if (context == NULL) {
        N_ERROR(LOG_CAT "%s() called with zero context", __FUNCTION__);
        return;
    }

    if (ausrv == NULL || ausrv->context != context) {
        N_ERROR(LOG_CAT "%s(): Confused with data structures", __FUNCTION__);
        return;
    }

    switch (pa_context_get_state(context)) {

    case PA_CONTEXT_CONNECTING:
        TRACE("ausrv: connecting to server");
        set_connection_status(ausrv, false);
        break;
        
    case PA_CONTEXT_AUTHORIZING:
        TRACE("ausrv: authorizing");
        set_connection_status(ausrv, false);
        break;
        
    case PA_CONTEXT_SETTING_NAME:
        TRACE("ausrv: setting name");
        set_connection_status(ausrv, false);
        break;
        
    case PA_CONTEXT_READY:
        TRACE("ausrv: connection established.");
        set_connection_status(ausrv, true);
        cancel_timer(ausrv);
        N_DEBUG(LOG_CAT "PulseAudio OK");
        break;
        
    case PA_CONTEXT_TERMINATED:
        TRACE("ausrv: connection to server terminated");
        goto disconnect;
        
    case PA_CONTEXT_FAILED:
    default:
        if ((err = pa_context_errno(context)) != 0) {
            if ((strerr = pa_strerror(err)) == NULL)
                strerr = "<unknown>";

            N_ERROR(LOG_CAT "ausrv: server connection failure: %s", strerr);
        }

    disconnect:
        set_connection_status(ausrv, false);
        stream_kill_all(ausrv);
        restart_timer(ausrv, CONNECT_DELAY);
    }
}

static void event_callback(pa_context                   *context,
                           pa_subscription_event_type_t  type,
                           uint32_t                      idx,
                           void                         *userdata)
{
    struct ausrv *ausrv = (struct ausrv *)userdata;

    (void)idx;
  
    if (ausrv == NULL || ausrv->context != context)
        N_ERROR(LOG_CAT "%s(): Confused with data structures", __FUNCTION__);
    else {
        switch (type) {

        case PA_SUBSCRIPTION_EVENT_SINK:
            TRACE("Event sink");
            break;

        case PA_SUBSCRIPTION_EVENT_SOURCE:
            TRACE("Event source");
            break;

        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            TRACE("Event sink input");
            break;

        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            TRACE("Event source output");
            break;

        default:
            TRACE("Event %d", type);
            break;
        }
    }
}


static void retry_connect(pa_mainloop_api *api, pa_time_event *event,
                          const struct timeval *tv, void *data)
{
    struct ausrv *ausrv = (struct ausrv *)data;

    (void)api;
    (void)tv;
    
    if (event != ausrv->timer) {
        N_ERROR(LOG_CAT "%s(): Called with unknown timer (%p != %p)", __FUNCTION__,
                  event, ausrv->timer);
        return;
    }

    ausrv->timer = NULL;

    connect_server(ausrv);
}


static void restart_timer(struct ausrv *ausrv, int secs)
{
    pa_mainloop_api *api = pa_glib_mainloop_get_api(ausrv->mainloop);
    struct timeval   tv;

    gettimeofday(&tv, NULL);
    tv.tv_sec += secs;
    
    if (ausrv->timer != NULL)
        api->time_restart(ausrv->timer, &tv);
    else
        ausrv->timer = api->time_new(api, &tv, retry_connect, (void *)ausrv);
}


static void cancel_timer(struct ausrv *ausrv)
{
    pa_mainloop_api *api;
    
    if (ausrv->timer != NULL) {
        api = pa_glib_mainloop_get_api(ausrv->mainloop);
        api->time_free(ausrv->timer);
        ausrv->timer = NULL;
    }
}


static void connect_server(struct ausrv *ausrv)
{
    pa_mainloop_api *api    = pa_glib_mainloop_get_api(ausrv->mainloop);
    char            *server = ausrv->server;

    cancel_timer(ausrv);


    if (server != NULL && !strcmp(ausrv->server, DEFAULT_SERVER))
        server = NULL;
    
    
    /*
     * Note: It is not possible to reconnect a context if it ever gets
     *     disconnected. If we have a context here, get rid of it and
     *     allocate a new one.
     */
    if (ausrv->context != NULL) {
        pa_context_set_state_callback(ausrv->context, NULL, NULL);
        pa_context_set_subscribe_callback(ausrv->context, NULL, NULL);
        pa_context_unref(ausrv->context);
        ausrv->context = NULL;
    }
    
    if ((ausrv->context = pa_context_new(api, pa_client_name)) == NULL) {
        N_ERROR(LOG_CAT "%s(): pa_context_new() failed, exiting", __FUNCTION__);
        exit(1);
    }
    
    pa_context_set_state_callback(ausrv->context, context_callback, ausrv);
    pa_context_set_subscribe_callback(ausrv->context, event_callback, ausrv);


    N_DEBUG(LOG_CAT "Trying to connect to %s...", server ?: DEFAULT_SERVER);
    pa_context_connect(ausrv->context, server, PA_CONTEXT_NOAUTOSPAWN, NULL);
}
