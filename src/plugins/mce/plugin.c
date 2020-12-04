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

#include <ngf/plugin.h>
#include <ngf/core-dbus.h>
#include <dbus/dbus.h>
#include <mce/dbus-names.h>

#define LOG_CAT "mce: "
#define MCE_KEY "plugin.mce.data"

#define MCE_LED_PATTERN_KEY "mce.led_pattern"

N_PLUGIN_NAME        ("mce")
N_PLUGIN_VERSION     ("0.1")
N_PLUGIN_DESCRIPTION ("MCE plugin for handling backlight and led actions")

typedef struct _MceData
{
    NRequest       *request;
    NSinkInterface *iface;
    gchar          *pattern;
} MceData;

static GList *active_events;

static gboolean
toggle_pattern (NCore *core, const char *pattern, gboolean activate)
{
    DBusMessage *msg = NULL;
    gboolean     ret = FALSE;

    if (!pattern)
        return FALSE;

    msg = dbus_message_new_method_call (MCE_SERVICE, MCE_REQUEST_PATH, MCE_REQUEST_IF,
        activate ? MCE_ACTIVATE_LED_PATTERN : MCE_DEACTIVATE_LED_PATTERN);

    if (msg == NULL)
        return FALSE;

    if (!dbus_message_append_args (msg, DBUS_TYPE_STRING, &pattern, DBUS_TYPE_INVALID)) {
        dbus_message_unref (msg);
        return FALSE;
    }

    ret = n_dbus_async_call_full (core, NULL, NULL, DBUS_BUS_SYSTEM, msg);
    dbus_message_unref (msg);

    if (ret)
        N_DEBUG (LOG_CAT "%s >> led pattern %s %s.", __FUNCTION__, pattern, activate ? "activated" : "deactivated");

    return ret;
}

static DBusHandlerResult
mce_signal_filter (NCore *core, DBusConnection *connection, DBusMessage *msg, void *user_data)
{
    (void) core;
    (void) connection;
    (void) user_data;

    if (dbus_message_is_signal(msg, MCE_SIGNAL_IF, MCE_LED_PATTERN_DEACTIVATED_SIG)) {
        DBusError error;
        dbus_error_init(&error);

        gchar *pattern = NULL;

        if (!dbus_message_get_args(msg, &error, DBUS_TYPE_STRING, &pattern, DBUS_TYPE_INVALID)) {
            N_WARNING (LOG_CAT "%s >> failed to read MCE signal arguments, cause: %s", __FUNCTION__, error.message);
            dbus_error_free(&error);
        } else {
            GList *event;
            N_DEBUG (LOG_CAT "%s >> mce finished playing %s", __FUNCTION__, pattern);
            for (event = active_events; event != NULL; event = g_list_next(event)) {
                MceData *data = (MceData *) event->data;
                if (g_strcmp0(pattern, data->pattern) == 0) {
                    n_sink_interface_complete(data->iface, data->request);
                    N_DEBUG (LOG_CAT "%s >> led pattern %s complete", __FUNCTION__, data->pattern);
                    active_events = g_list_remove_all(active_events, data);
                    break;
                }
            }
        }
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
mce_sink_shutdown (NSinkInterface *iface)
{
    (void) iface;
    g_list_free(active_events);
}

static int
mce_sink_can_handle (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    const NProplist *props = n_request_get_properties (request);

    if (n_proplist_has_key (props, MCE_LED_PATTERN_KEY))
        return TRUE;

    return FALSE;
}

static int
mce_sink_prepare (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    (void) request;
    
    MceData *data = g_slice_new0 (MceData);

    data->request    = request;
    data->iface      = iface;

    n_request_store_data (request, MCE_KEY, data);
    n_sink_interface_synchronize (iface, request);
    
    return TRUE;
}

static int
mce_sink_play (NSinkInterface *iface, NRequest *request)
{
    const NProplist *props = n_request_get_properties (request);
    const gchar *pattern = NULL;
    NCore *core;

    MceData *data = (MceData*) n_request_get_data (request, MCE_KEY);
    g_assert (data != NULL);

    core = n_sink_interface_get_core (iface);

    pattern = n_proplist_get_string (props, MCE_LED_PATTERN_KEY);
    if (pattern != NULL) {
        data->pattern = g_strdup (pattern);
        if (toggle_pattern (core, pattern, TRUE)) {
            active_events = g_list_append(active_events, data);
        } else {
            g_free (data->pattern);
            data->pattern = NULL;
        }
    }

    return TRUE;
}

static int
mce_sink_pause (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    (void) request;

    return TRUE;
}

static void
mce_sink_stop (NSinkInterface *iface, NRequest *request)
{
    NCore *core;

    MceData *data = (MceData*) n_request_get_data (request, MCE_KEY);
    g_assert (data != NULL);

    core = n_sink_interface_get_core (iface);

    if (data->pattern) {
        toggle_pattern (core, data->pattern, FALSE);
        g_free (data->pattern);
        data->pattern = NULL;
    }

    active_events = g_list_remove_all(active_events, data);
    g_slice_free (MceData, data);
}

N_PLUGIN_LOAD (plugin)
{
    NCore *core;

    static const NSinkInterfaceDecl decl = {
        .name       = "mce",
        .type       = N_SINK_INTERFACE_TYPE_LEDS,
        .initialize = NULL,
        .shutdown   = mce_sink_shutdown,
        .can_handle = mce_sink_can_handle,
        .prepare    = mce_sink_prepare,
        .play       = mce_sink_play,
        .pause      = mce_sink_pause,
        .stop       = mce_sink_stop
    };

    core = n_plugin_get_core (plugin);
    g_assert (core);

    if (n_dbus_add_match (core, mce_signal_filter, NULL, DBUS_BUS_SYSTEM,
                          MCE_SIGNAL_IF,
                          MCE_SIGNAL_PATH,
                          MCE_LED_PATTERN_DEACTIVATED_SIG) == 0) {
        N_WARNING (LOG_CAT "failed to add filter");
        return FALSE;
    }

    n_plugin_register_sink (plugin, &decl);

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    NCore *core;

    core = n_plugin_get_core (plugin);
    n_dbus_remove_match_by_cb (core, mce_signal_filter);
}
