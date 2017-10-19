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
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <mce/dbus-names.h>

#define LOG_CAT "callstate: "
#define CALL_STATE_KEY "call_state.mode"

N_PLUGIN_NAME        ("callstate")
N_PLUGIN_VERSION     ("0.1")
N_PLUGIN_DESCRIPTION ("Call state tracking plugin")

static DBusConnection *system_bus = NULL;

static void
update_context_call_state (NContext *context, const char *value)
{
    NValue *v = n_value_new ();
    n_value_set_string (v, value);
    n_context_set_value (context, CALL_STATE_KEY, v);
}

static void
query_call_state_notify_cb (DBusPendingCall *pending, void *user_data)
{
    NContext *context = (NContext*) user_data;
    DBusMessage *msg = NULL;
    const char *call_state = NULL, *emergency_state = NULL;

    msg = dbus_pending_call_steal_reply (pending);
    if (!msg) {
        dbus_pending_call_unref (pending);
        return;
    }

    if (dbus_message_get_args (msg, NULL,
                               DBUS_TYPE_STRING, &call_state,
                               DBUS_TYPE_STRING, &emergency_state,
                               DBUS_TYPE_INVALID)) {
        N_DEBUG (LOG_CAT "initial state is '%s' (emergency_state is '%s')", call_state,
            emergency_state);

        update_context_call_state (context, call_state);
    }

    dbus_message_unref (msg);
    dbus_pending_call_unref (pending);
}

static int
query_call_state (DBusConnection *connection, NContext *context)
{
    DBusMessage *msg = NULL;
    DBusPendingCall *pending_call = NULL;

    msg = dbus_message_new_method_call (MCE_SERVICE,
        MCE_REQUEST_PATH, MCE_REQUEST_IF, MCE_CALL_STATE_GET);
    if (!msg)
        return FALSE;

    if (!dbus_connection_send_with_reply (connection,
        msg, &pending_call, -1)) {
        dbus_message_unref (msg);
        return FALSE;
    }

    if (!pending_call) {
        dbus_message_unref (msg);
        return FALSE;
    }

    dbus_pending_call_set_notify (pending_call,
        query_call_state_notify_cb, context, NULL);

    dbus_message_unref (msg);

    return TRUE;
}

static DBusHandlerResult
filter_cb (DBusConnection *connection, DBusMessage *msg, void *data)
{
    (void) connection;
    
    NContext *context = (NContext*) data;
    const char *call_state = NULL, *emergency_state = NULL;

    if (dbus_message_is_signal (msg, MCE_SIGNAL_IF, MCE_CALL_STATE_SIG) &&
        dbus_message_get_args  (msg, NULL,
                                DBUS_TYPE_STRING, &call_state,
                                DBUS_TYPE_STRING, &emergency_state,
                                DBUS_TYPE_INVALID))
    {
        N_DEBUG (LOG_CAT "state changed to %s (%s)",
            call_state, emergency_state);

        update_context_call_state (context, call_state);
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

N_PLUGIN_LOAD (plugin)
{
    NCore *core = NULL;
    NContext *context = NULL;
    DBusError error;

    core = n_plugin_get_core (plugin);
    g_assert (core != NULL);

    context = n_core_get_context (core);
    g_assert (context != NULL);

    dbus_error_init (&error);
    system_bus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);

    if (dbus_error_is_set (&error)) {
        N_WARNING (LOG_CAT "failed to open connection to system bus: %s",
            error.message);
        dbus_error_free (&error);
        return FALSE;
    }

    dbus_connection_setup_with_g_main (system_bus, NULL);

    dbus_bus_add_match (system_bus,
                        "interface=" MCE_SIGNAL_IF ","
                        "path=" MCE_SIGNAL_PATH ","
                        "member=" MCE_CALL_STATE_SIG,
                        &error);

    if (dbus_error_is_set (&error)) {
        N_WARNING (LOG_CAT "failed to add watch: %s",
            error.message);
        dbus_error_free (&error);
        return FALSE;
    }

    if (!dbus_connection_add_filter (system_bus, filter_cb, context, NULL)) {
        N_WARNING (LOG_CAT "failed to add filter");
        return FALSE;
    }

    if (!query_call_state (system_bus, context)) {
        N_WARNING (LOG_CAT "failed to query initial state");
    }

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    (void) plugin;
}
