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

#define LOG_CAT "callstate: "
#define CALL_STATE_KEY "call_state.mode"

N_PLUGIN_NAME        ("callstate")
N_PLUGIN_VERSION     ("0.1")
N_PLUGIN_DESCRIPTION ("Call state tracking plugin")

static void
update_context_call_state (NContext *context, const char *value)
{
    NValue *v = n_value_new ();
    n_value_set_string (v, value);
    n_context_set_value (context, CALL_STATE_KEY, v);
}

static void
query_call_state_notify_cb (NCore *core, DBusMessage *msg, void *userdata)
{
    NContext   *context         = userdata;
    const char *call_state      = NULL;
    const char *emergency_state = NULL;

    (void) core;

    if (dbus_message_get_args (msg, NULL,
                               DBUS_TYPE_STRING, &call_state,
                               DBUS_TYPE_STRING, &emergency_state,
                               DBUS_TYPE_INVALID)) {
        N_DEBUG (LOG_CAT "initial state is '%s' (emergency_state is '%s')", call_state,
            emergency_state);

        update_context_call_state (context, call_state);
    }
}

static DBusHandlerResult
filter_cb (NCore *core, DBusConnection *connection, DBusMessage *msg, void *userdata)
{
    NContext   *context         = userdata;
    const char *call_state      = NULL;
    const char *emergency_state = NULL;

    (void) core;
    (void) connection;

    if (dbus_message_get_args  (msg, NULL,
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
    NCore    *core;
    NContext *context;

    core = n_plugin_get_core (plugin);
    g_assert (core);

    context = n_core_get_context (core);
    g_assert (context);

    if (n_dbus_add_match (core, filter_cb, context, DBUS_BUS_SYSTEM,
                          MCE_SIGNAL_IF,
                          MCE_SIGNAL_PATH,
                          MCE_CALL_STATE_SIG) == 0) {
        N_WARNING (LOG_CAT "failed to add filter");
        return FALSE;
    }

    if (!n_dbus_async_call (core, query_call_state_notify_cb, context, DBUS_BUS_SYSTEM,
                            MCE_SERVICE,
                            MCE_REQUEST_PATH,
                            MCE_REQUEST_IF,
                            MCE_CALL_STATE_GET))
        N_WARNING (LOG_CAT "failed to query initial state");

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    NCore    *core;

    core = n_plugin_get_core (plugin);
    n_dbus_remove_match_by_cb (core, filter_cb);
}
