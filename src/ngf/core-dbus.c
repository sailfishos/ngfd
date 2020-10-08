/*
 * ngfd - Non-graphic feedback daemon
 *
 * Copyright (C) 2017 Jolla Ltd
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
#include <ngf/log.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus-gmain/dbus-gmain.h>

#include <ngf/core-dbus.h>
#include "core-internal.h"
#include "core-dbus-internal.h"

#define LOG_CAT "core-dbus: "

typedef struct n_dbus_bus {
    int             ref;
    DBusConnection *connection;
    gboolean        filter_set;
} n_dbus_bus;

struct NDBusHelper {
    NCore          *core;
    guint           id_counter;
    n_dbus_bus      bus[2];
    GHashTable     *matches;
};

typedef struct n_dbus_cb {
    guint           id;
    NDBusFilterFunc cb;
    void           *userdata;
} n_dbus_cb;

typedef struct n_dbus_match {
    NDBusHelper    *dbus;
    DBusBusType     type;
    char           *match_str;
    GSList         *callbacks;
} n_dbus_match;

typedef struct n_dbus_call {
    NCore          *core;
    DBusBusType     type;
    NDBusReplyFunc  cb;
    void           *userdata;
} n_dbus_call;

static char*
build_match_string (const char *iface, const char *path, const char *member)
{
    return g_strdup_printf ("type='signal'," "%s%s%s" "%s%s%s" "%s%s%s",
                            iface ? "interface='" : "",
                            iface ? iface : "",
                            iface ? "'," : "",
                            path ? "path='" : "",
                            path ? path : "",
                            path ? "'," : "",
                            member ? "member='" : "",
                            member ? member : "",
                            member ? "'" : "");
}

static const char*
bus_str (DBusBusType type)
{
    if (type == DBUS_BUS_SYSTEM)
        return "system";

    return "session";
}

static DBusHandlerResult
filter_cb (DBusConnection *connection, DBusMessage *msg, void *userdata)
{
    NDBusHelper    *dbus = userdata;
    n_dbus_match   *match;
    char           *match_str = NULL;
    const char     *iface;
    const char     *path;
    const char     *member;
    GSList         *i;
    int             ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    iface  = dbus_message_get_interface (msg);
    path   = dbus_message_get_path (msg);
    member = dbus_message_get_member (msg);

    if ((!iface && !path && !member) || dbus_message_get_type (msg) != DBUS_MESSAGE_TYPE_SIGNAL)
        goto done;

    match_str = build_match_string (iface, path, member);

    if (!(match = g_hash_table_lookup (dbus->matches, match_str)))
        goto done;

    for (i = match->callbacks; i; i = i->next) {
        n_dbus_cb *cb = i->data;
        int r;
        if ((r = cb->cb (dbus->core, connection, msg, cb->userdata)) != DBUS_HANDLER_RESULT_NOT_YET_HANDLED)
            ret = r;
    }

done:
    g_free (match_str);
    return ret;
}

static void
connection_ref (NDBusHelper *dbus, DBusBusType type)
{
    g_assert (dbus->bus[type].ref > 0);
    g_assert (dbus->bus[type].connection);

    dbus_connection_ref (dbus->bus[type].connection);
    dbus->bus[type].ref++;
}

static void
connection_unref (NDBusHelper *dbus, DBusBusType type)
{
    DBusConnection *connection;

    g_assert (dbus->bus[type].ref > 0);
    g_assert (dbus->bus[type].connection);

    connection = dbus->bus[type].connection;
    dbus->bus[type].ref--;

    if (dbus->bus[type].ref == 0) {
        if (dbus->bus[type].filter_set) {
            N_DEBUG (LOG_CAT "remove %s filter callback", bus_str(type));
            dbus_connection_remove_filter (connection, filter_cb, dbus);
        }
        dbus->bus[type].connection = NULL;
    }

    dbus_connection_unref (connection);
}

static DBusConnection*
connection_get (NDBusHelper *dbus, DBusBusType type, gboolean set_filter)
{
    if (dbus->bus[type].connection) {
        N_DEBUG (LOG_CAT "get cached %s bus", bus_str(type));
        connection_ref (dbus, type);
    } else {
        if ((dbus->bus[type].connection = dbus_bus_get (type, NULL))) {
            N_DEBUG (LOG_CAT "get initial %s bus", bus_str(type));
            dbus_gmain_set_up_connection (dbus->bus[type].connection, NULL);
        } else {
            N_DEBUG (LOG_CAT "failed to get %s bus", bus_str(type));
            goto fail;
        }
        dbus->bus[type].ref = 1;
    }

    if (set_filter && !dbus->bus[type].filter_set) {
        if (!dbus_connection_add_filter (dbus->bus[type].connection,
                                         filter_cb, dbus, NULL)) {
            N_ERROR (LOG_CAT "failed to add filter");
            goto fail;
        }

        dbus->bus[type].filter_set = TRUE;
    }

    return dbus->bus[type].connection;

fail:
    if (dbus->bus[type].connection)
        dbus_connection_unref (dbus->bus[type].connection);
    return NULL;
}

static void
match_add_callback (n_dbus_match *match, guint id, NDBusFilterFunc cb, void *userdata)
{
    n_dbus_cb *callback;

    g_assert (match);

    callback = g_new0 (n_dbus_cb, 1);
    callback->id        = id;
    callback->cb        = cb;
    callback->userdata  = userdata;

    match->callbacks = g_slist_append (match->callbacks, callback);
    connection_ref (match->dbus, match->type);
    N_DEBUG (LOG_CAT "add match callback '%s' -> %u : %p", match->match_str, callback->id, callback->cb);
}

static void
match_remove_callback (n_dbus_match *match, n_dbus_cb *cb)
{
    g_assert (match);
    g_assert (cb);

    N_DEBUG (LOG_CAT "remove match callback '%s' -> %u : %p", match->match_str, cb->id, cb->cb);
    match->callbacks = g_slist_remove (match->callbacks, cb);
    connection_unref (match->dbus, match->type);
    g_free (cb);
}

static void
match_remove_full (gpointer userdata)
{
    n_dbus_match    *match = userdata;
    n_dbus_cb       *cb;
    GSList          *i;

    while (match->callbacks) {
        i = match->callbacks;
        cb = i->data;
        match_remove_callback (match, cb);
    }

    g_assert (match->dbus->bus[match->type].connection);

    dbus_bus_remove_match (match->dbus->bus[match->type].connection,
                           match->match_str, NULL);
    connection_unref (match->dbus, match->type);

    N_DEBUG (LOG_CAT "remove match '%s'", match->match_str);
    g_free (match->match_str);
    g_free (match);
}

NDBusHelper*
n_dbus_helper_new (NCore *core)
{
    NDBusHelper *dbus;

    dbus            = g_new0 (NDBusHelper, 1);
    dbus->core      = core;
    dbus->matches   = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             NULL, match_remove_full);

    return dbus;
}

void
n_dbus_helper_free (NDBusHelper *dbus)
{
    g_assert (dbus);

    g_hash_table_destroy (dbus->matches);
    g_assert (dbus->bus[DBUS_BUS_SYSTEM].ref == 0);
    g_assert (!dbus->bus[DBUS_BUS_SYSTEM].connection);
    g_assert (dbus->bus[DBUS_BUS_SESSION].ref == 0);
    g_assert (!dbus->bus[DBUS_BUS_SESSION].connection);
    g_free (dbus);
}

guint
n_dbus_add_match (NCore            *core,
                  NDBusFilterFunc   cb,
                  void             *userdata,
                  DBusBusType       type,
                  const char       *iface,
                  const char       *path,
                  const char       *member)
{
    n_dbus_match   *match;
    DBusConnection *connection;
    char           *match_str;
    int             id;

    g_assert (core);
    g_assert (core->dbus);

    if (!(connection = connection_get (core->dbus, type, TRUE))) {
        N_ERROR (LOG_CAT "could not get %s bus", bus_str(type));
        return 0;
    }

    match_str = build_match_string (iface, path, member);

    if (!(match = g_hash_table_lookup (core->dbus->matches, match_str))) {
        N_DEBUG (LOG_CAT "new match '%s'", match_str);
        match           = g_new0 (n_dbus_match, 1);
        match->type     = type;
        match->dbus     = core->dbus;
        match->match_str= match_str;
        dbus_bus_add_match (connection, match->match_str, NULL);
        g_hash_table_insert (core->dbus->matches, match->match_str, match);
    } else
        g_free (match_str);

    id = ++core->dbus->id_counter;
    match_add_callback (match, id, cb, userdata);

    return id;
}

static gboolean
dbus_remove_match (NCore *core, guint match_id, NDBusFilterFunc match_cb)
{
    GHashTableIter  iter;
    gpointer        key, value;
    GSList         *i;
    n_dbus_match   *match   = NULL;
    gboolean        removed = FALSE;

    g_assert (core);
    g_assert (core->dbus);

    g_hash_table_iter_init (&iter, core->dbus->matches);

    while (g_hash_table_iter_next (&iter, &key, &value)) {
        match = value;
        for (i = match->callbacks; i; i = i->next) {
            n_dbus_cb *cb = i->data;

            if ((match_id && cb->id == match_id) ||
                (match_cb && cb->cb == match_cb)) {
                match_remove_callback (match, cb);
                removed = TRUE;
                break;
            }
        }

        if (removed)
            break;
    }

    if (match && !match->callbacks)
        g_hash_table_remove (core->dbus->matches, match->match_str);

    return removed;
}

void n_dbus_remove_match_by_id (NCore *core, guint match_id)
{
    if (!dbus_remove_match (core, match_id, NULL))
        N_WARNING (LOG_CAT "tried to remove match by id %u - not found", match_id);
}

void
n_dbus_remove_match_by_cb (NCore *core, NDBusFilterFunc cb)
{
    if (!dbus_remove_match (core, 0, cb))
        N_WARNING (LOG_CAT "tried to remove match by callback %p - not found", cb);
}

static void
async_call_cb (DBusPendingCall *pending, void *userdata)
{
    n_dbus_call *call = userdata;
    DBusMessage *msg  = NULL;

    if (!(msg = dbus_pending_call_steal_reply (pending))) {
        goto done;
    }

    call->cb (call->core, msg, call->userdata);
    dbus_message_unref (msg);

done:
    dbus_pending_call_unref (pending);
    connection_unref (call->core->dbus, call->type);
    g_free (call);
}

gboolean
n_dbus_async_call_full (NCore *core,
                        NDBusReplyFunc cb,
                        void *userdata,
                        DBusBusType type,
                        DBusMessage *msg)
{
    n_dbus_call     *call;
    DBusPendingCall *pending_call = NULL;
    DBusConnection  *connection   = NULL;

    g_assert (core);
    g_assert (core->dbus);
    g_assert (msg);

    if (!(connection = connection_get (core->dbus, type, FALSE)))
        goto fail;

    if (cb) {
        if (!dbus_connection_send_with_reply (connection,
                                              msg, &pending_call, -1))
            goto fail;

        if (!pending_call)
            goto fail;

        call            = g_new0 (n_dbus_call, 1);
        call->core      = core;
        call->type      = type;
        call->cb        = cb;
        call->userdata  = userdata;

        dbus_pending_call_set_notify (pending_call, async_call_cb, call, NULL);
    } else {
        if (!dbus_connection_send (connection, msg, NULL))
            goto fail;
        connection_unref (core->dbus, type);
    }

    return TRUE;

fail:
    if (connection)
        connection_unref (core->dbus, type);
    N_ERROR (LOG_CAT "failed to do async call");
    return FALSE;
}

gboolean
n_dbus_async_call (NCore *core,
                   NDBusReplyFunc cb,
                   void *userdata,
                   DBusBusType type,
                   const char *destination,
                   const char *path,
                   const char *iface,
                   const char *method)
{
    DBusMessage *msg;

    g_assert (core);
    g_assert (core->dbus);

    msg = dbus_message_new_method_call (destination,
                                        path,
                                        iface,
                                        method);
    if (!msg)
        goto fail;

    n_dbus_async_call_full (core, cb, userdata, type, msg);
    dbus_message_unref (msg);

    N_DEBUG (LOG_CAT "do async call %s %s %s.%s", destination, path, iface, method);

    return TRUE;

fail:
    N_ERROR (LOG_CAT "failed to do async call %s %s %s.%s", destination, path, iface, method);
    return FALSE;
}
