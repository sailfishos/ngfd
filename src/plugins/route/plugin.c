/*
 * ngfd - Non-graphic feedback daemon
 *
 * Copyright (C) 2017 Jolla Ltd.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <ngf/plugin.h>
#include <ngf/core-dbus.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <ohm-ext/route.h>
#include "keys.h"

#define LOG_CAT "route: "

N_PLUGIN_NAME        ("route")
N_PLUGIN_VERSION     ("0.1")
N_PLUGIN_DESCRIPTION ("Audio route tracking plugin")

static void
update_context (NContext *context, guint output_type)
{
    NValue *v;

    v = n_value_new ();
    n_value_set_uint (v, output_type);
    n_context_set_value (context, CONTEXT_ROUTE_OUTPUT_TYPE_KEY, v);

    v = n_value_new ();
    n_value_set_string (v, output_type & OHM_EXT_ROUTE_TYPE_BUILTIN ? "builtin" : "external");
    n_context_set_value (context, CONTEXT_ROUTE_OUTPUT_CLASS_KEY, v);
}

static void
query_active_routes_cb (NCore *core, DBusMessage *msg, void *userdata)
{
    NContext       *context     = userdata;
    const char     *output      = NULL;
    const char     *input       = NULL;
    dbus_uint32_t   output_type = 0;
    dbus_uint32_t   input_type  = 0;

    (void) core;

    if (dbus_message_get_args (msg, NULL,
                               DBUS_TYPE_STRING, &output,
                               DBUS_TYPE_UINT32, &output_type,
                               DBUS_TYPE_STRING, &input,
                               DBUS_TYPE_UINT32, &input_type,
                               DBUS_TYPE_INVALID)) {
        N_DEBUG (LOG_CAT "initial routes are output '%s' (%u) input '%s' (%u) ",
                         output, output_type, input, input_type);

        update_context (context, output_type);
    }
}

static DBusHandlerResult
filter_cb (NCore *core, DBusConnection *connection, DBusMessage *msg, void *userdata)
{
    NContext      *context = userdata;
    const char    *name;
    dbus_uint32_t  route_type;

    (void) core;
    (void) connection;

    if (dbus_message_get_args (msg, NULL,
                               DBUS_TYPE_STRING, &name,
                               DBUS_TYPE_UINT32, &route_type,
                               DBUS_TYPE_INVALID))
    {
        if (route_type & OHM_EXT_ROUTE_TYPE_OUTPUT) {
            N_DEBUG (LOG_CAT "output route changed to %s (%u)", name, route_type);

            update_context (context, route_type);
        }
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

    if (n_dbus_add_match (core, filter_cb, context,
                          DBUS_BUS_SYSTEM,
                          OHM_EXT_ROUTE_MANAGER_INTERFACE,
                          OHM_EXT_ROUTE_MANAGER_PATH,
                          OHM_EXT_ROUTE_CHANGED_SIGNAL) == 0) {
        N_ERROR (LOG_CAT "failed to add signal handler");
        return FALSE;
    }

    if (!n_dbus_async_call (core, query_active_routes_cb, context,
                            DBUS_BUS_SYSTEM,
                            OHM_EXT_ROUTE_MANAGER_INTERFACE,
                            OHM_EXT_ROUTE_MANAGER_PATH,
                            OHM_EXT_ROUTE_MANAGER_INTERFACE,
                            OHM_EXT_ROUTE_ACTIVE_ROUTES_METHOD))
        N_WARNING (LOG_CAT "failed to query initial state");

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    NCore *core;

    core = n_plugin_get_core (plugin);
    g_assert (core);

    n_dbus_remove_match_by_cb (core, filter_cb);
}
