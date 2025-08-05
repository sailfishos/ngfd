/*
 * ngfd - Non-graphic feedback daemon
 *
 * Copyright (C) 2018 Jolla Ltd.
 * Contact: Juho Hämäläinen <juho.hamalainen@jolla.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <ngf/plugin.h>

#define NULL_KEY        "sink.null"
#define NULL_DATA_KEY   "data." NULL_KEY
#define LOG_CAT         "null: "

N_PLUGIN_NAME        ("null")
N_PLUGIN_VERSION     ("0.1")
N_PLUGIN_DESCRIPTION ("Null sink plugin")

typedef struct NullSinkData
{
    NRequest       *request;
    NSinkInterface *iface;
    guint           source_id;
} NullSinkData;

static int
null_sink_can_handle (NSinkInterface *iface, NRequest *request)
{
    const NProplist *props;

    (void) iface;

    if ((props = n_request_get_properties (request))) {
        if (n_proplist_has_key (props, NULL_KEY)) {
            N_DEBUG (LOG_CAT "sink can handle");
            return TRUE;
        }
    }

    return FALSE;
}

static int
null_sink_prepare (NSinkInterface *iface, NRequest *request)
{
    NullSinkData *data;

    data          = g_slice_new0 (NullSinkData);
    data->request = request;
    data->iface   = iface;

    n_request_store_data (request, NULL_DATA_KEY, data);
    n_sink_interface_synchronize (iface, request);

    return TRUE;
}

static gboolean
play_cb (gpointer userdata)
{
    NullSinkData *data = userdata;
    g_assert (data);

    data->source_id = 0;
    n_sink_interface_complete (data->iface, data->request);

    return FALSE;
}

static int
null_sink_play (NSinkInterface *iface, NRequest *request)
{
    NullSinkData *data;

    (void) iface;

    N_DEBUG (LOG_CAT "sink play");

    data = n_request_get_data (request, NULL_DATA_KEY);
    g_assert (data);

    data->source_id = g_idle_add (play_cb, data);

    return TRUE;
}

static void
null_sink_stop (NSinkInterface *iface, NRequest *request)
{
    NullSinkData *data;

    (void) iface;

    N_DEBUG (LOG_CAT "sink stop");

    data = n_request_get_data (request, NULL_DATA_KEY);
    if (!data)
        return;

    n_request_store_data (request, NULL_DATA_KEY, NULL);

    if (data->source_id > 0)
        g_source_remove (data->source_id);

    g_slice_free (NullSinkData, data);
}

N_PLUGIN_LOAD (plugin)
{
    static const NSinkInterfaceDecl decl = {
        .name       = "null",
        .type       = "null",
        .initialize = NULL,
        .shutdown   = NULL,
        .can_handle = null_sink_can_handle,
        .prepare    = null_sink_prepare,
        .play       = null_sink_play,
        .pause      = NULL,
        .stop       = null_sink_stop
    };

    n_plugin_register_sink (plugin, &decl);

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    (void) plugin;
}
