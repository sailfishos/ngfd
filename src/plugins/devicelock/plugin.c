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

#define LOG_CAT "devicelock: "
#define DEVICE_LOCK_KEY "device_lock.state"

#define DEVICELOCK_SERVICE       "org.nemomobile.devicelock"
#define DEVICELOCK_IF            "org.nemomobile.lipstick.devicelock"
#define DEVICELOCK_PATH          "/devicelock"
#define DEVICELOCK_STATE_GET     "state"
#define DEVICELOCK_STATE_SIG     "stateChanged"

N_PLUGIN_NAME        ("devicelock")
N_PLUGIN_VERSION     ("0.1")
N_PLUGIN_DESCRIPTION ("Device lock tracking plugin")

static DBusConnection *system_bus = NULL;

static const char *
device_lock_state_to_string (int state)
{
    switch (state) {
    case 0:
        return "unlocked";
    case 1:
        return "locked";
    case 2:
        return "manager_lockout";
    case 3:
        return "temporary_lockout";
    case 4:
        return "permanent_lockout";
    default:
        return "undefined";
    }
}

static void
update_context_devicelock_state (NContext *context, int value)
{
    NValue *v = n_value_new ();

    n_value_set_string (v, device_lock_state_to_string (value));

    n_context_set_value (context, DEVICE_LOCK_KEY, v);
}

static void
query_devicelock_state_notify_cb (DBusPendingCall *pending, void *user_data)
{
    NContext *context = (NContext*) user_data;
    DBusMessage *msg = NULL;
    int state = 0;

    msg = dbus_pending_call_steal_reply (pending);
    if (!msg) {
        dbus_pending_call_unref (pending);
        return;
    }

    if (dbus_message_get_args (msg, NULL,
                               DBUS_TYPE_INT32, &state,
                               DBUS_TYPE_INVALID)) {
        N_DEBUG (LOG_CAT "initial state is '%s'",
            device_lock_state_to_string (state));

        update_context_devicelock_state (context, state);
    }

    dbus_message_unref (msg);
    dbus_pending_call_unref (pending);
}

static int
query_devicelock_state (DBusConnection *connection, NContext *context)
{
    DBusMessage *msg = NULL;
    DBusPendingCall *pending_call = NULL;

    msg = dbus_message_new_method_call (DEVICELOCK_SERVICE,
        DEVICELOCK_PATH, DEVICELOCK_IF, DEVICELOCK_STATE_GET);
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
        query_devicelock_state_notify_cb, context, NULL);

    dbus_message_unref (msg);

    return TRUE;
}

static DBusHandlerResult
filter_cb (DBusConnection *connection, DBusMessage *msg, void *data)
{
    (void) connection;
    
    NContext *context = (NContext*) data;
    int state = 0;

    if (dbus_message_is_signal (msg, DEVICELOCK_IF, DEVICELOCK_STATE_SIG) &&
        dbus_message_get_args  (msg, NULL,
                                DBUS_TYPE_INT32, &state,
                                DBUS_TYPE_INVALID))
    {
        N_DEBUG (LOG_CAT "state changed to %s",
            device_lock_state_to_string (state));

        update_context_devicelock_state (context, state);
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
                        "interface=" DEVICELOCK_IF ","
                        "path=" DEVICELOCK_PATH ","
                        "member=" DEVICELOCK_STATE_SIG,
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

    if (!query_devicelock_state (system_bus, context)) {
        N_WARNING (LOG_CAT "failed to query initial state");
    }

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    (void) plugin;
}
