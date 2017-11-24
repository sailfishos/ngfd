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
        return "code_entry_lockout";
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
query_devicelock_state_notify_cb (NCore *core, DBusMessage *msg, void *data)
{
    NContext       *context = data;
    int             state   = 0;

    (void) core;

    if (dbus_message_get_args (msg, NULL,
                               DBUS_TYPE_INT32, &state,
                               DBUS_TYPE_INVALID)) {
        N_DEBUG (LOG_CAT "initial state is '%s'",
            device_lock_state_to_string (state));

        update_context_devicelock_state (context, state);
    }
}

static DBusHandlerResult
filter_cb (NCore *core, DBusConnection *connection, DBusMessage *msg, void *data)
{
    (void) core;
    (void) connection;

    NContext   *context = data;
    int         state = 0;

    if (dbus_message_get_args (msg, NULL,
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
    NCore    *core;
    NContext *context;

    core = n_plugin_get_core (plugin);
    g_assert (core);

    context = n_core_get_context (core);
    g_assert (context);

    if (n_dbus_add_match (core,
                          filter_cb,
                          context,
                          DBUS_BUS_SYSTEM,
                          DEVICELOCK_IF,
                          DEVICELOCK_PATH,
                          DEVICELOCK_STATE_SIG) == 0) {
        N_ERROR (LOG_CAT "failed to listen for state signal");
        return FALSE;
    }

    if (!n_dbus_async_call (core,
                            query_devicelock_state_notify_cb,
                            context,
                            DBUS_BUS_SYSTEM,
                            DEVICELOCK_SERVICE,
                            DEVICELOCK_PATH,
                            DEVICELOCK_IF,
                            DEVICELOCK_STATE_GET)) {
        N_WARNING (LOG_CAT "failed to query initial state");
    }

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    NCore *core;

    core = n_plugin_get_core (plugin);
    n_dbus_remove_match_by_cb (core, filter_cb);
}
