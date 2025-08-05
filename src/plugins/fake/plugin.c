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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <ngf/plugin.h>

#define FAKE_KEY "plugin.fake.data"
#define LOG_CAT  "fake: "

typedef struct _FakeData
{
    NRequest       *request;
    NSinkInterface *iface;
    guint           timeout_id;
} FakeData;

N_PLUGIN_NAME        ("fake")
N_PLUGIN_VERSION     ("0.1")
N_PLUGIN_DESCRIPTION ("Fake plugin")

static int
fake_sink_initialize (NSinkInterface *iface)
{
    (void) iface;
    N_DEBUG (LOG_CAT "sink initialize");
    return TRUE;
}

static void
fake_sink_shutdown (NSinkInterface *iface)
{
    (void) iface;
    N_DEBUG (LOG_CAT "sink shutdown");
}

static int
fake_sink_can_handle (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    (void) request;

    N_DEBUG (LOG_CAT "sink can_handle");
    return TRUE;
}

static int
fake_sink_prepare (NSinkInterface *iface, NRequest *request)
{
    N_DEBUG (LOG_CAT "sink prepare");

    FakeData *data = g_slice_new0 (FakeData);

    data->request    = request;
    data->iface      = iface;
    data->timeout_id = 0;

    n_request_store_data (request, FAKE_KEY, data);
    n_sink_interface_synchronize (iface, request);

    return TRUE;
}

static gboolean
timeout_cb (gpointer userdata)
{
    N_DEBUG (LOG_CAT "sink play timeout");

    FakeData *data = (FakeData*) userdata;
    g_assert (data != NULL);

    data->timeout_id = 0;
    n_sink_interface_complete (data->iface, data->request);

    return FALSE;
}

static int
fake_sink_play (NSinkInterface *iface, NRequest *request)
{
    N_DEBUG (LOG_CAT "sink play");

    (void) iface;

    FakeData *data = (FakeData*) n_request_get_data (request, FAKE_KEY);
    g_assert (data != NULL);

    data->timeout_id = g_timeout_add_seconds (2, timeout_cb, data);

    return TRUE;
}

static int
fake_sink_pause (NSinkInterface *iface, NRequest *request)
{
    N_DEBUG (LOG_CAT "sink pause");

    (void) iface;
    (void) request;

    return TRUE;
}

static void
fake_sink_stop (NSinkInterface *iface, NRequest *request)
{
    N_DEBUG (LOG_CAT "sink stop");

    (void) iface;

    FakeData *data = (FakeData*) n_request_get_data (request, FAKE_KEY);
    if (!data)
        return;

    n_request_store_data (request, FAKE_KEY, NULL);

    if (data->timeout_id > 0) {
        g_source_remove (data->timeout_id);
        data->timeout_id = 0;
    }

    g_slice_free (FakeData, data);
}

N_PLUGIN_LOAD (plugin)
{
    N_DEBUG (LOG_CAT "plugin load");

    static const NSinkInterfaceDecl decl = {
        .name       = "fake",
        .initialize = fake_sink_initialize,
        .shutdown   = fake_sink_shutdown,
        .can_handle = fake_sink_can_handle,
        .prepare    = fake_sink_prepare,
        .play       = fake_sink_play,
        .pause      = fake_sink_pause,
        .stop       = fake_sink_stop
    };

    n_plugin_register_sink (plugin, &decl);

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    (void) plugin;

    N_DEBUG (LOG_CAT "plugin unload");
}
