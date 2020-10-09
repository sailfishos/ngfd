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

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus-gmain/dbus-gmain.h>
#include <stdint.h>
#include <string.h>

#include <ngf/log.h>
#include <ngf/value.h>
#include <ngf/proplist.h>
#include <ngf/plugin.h>
#include <ngf/request.h>
#include <ngf/inputinterface.h>

N_PLUGIN_NAME        ("dbus")
N_PLUGIN_VERSION     ("0.1")
N_PLUGIN_DESCRIPTION ("D-Bus interface")

#include "com.nokia.NonGraphicFeedback1.Backend.xml.h"

#define LOG_CAT "dbus: "

#define NGF_DBUS_PROXY_NAME   "com.nokia.NonGraphicFeedback1"
#define NGF_DBUS_NAME         "com.nokia.NonGraphicFeedback1.Backend"
#define NGF_DBUS_PATH         "/com/nokia/NonGraphicFeedback1"
#define NGF_DBUS_IFACE        "com.nokia.NonGraphicFeedback1"

#define NGF_DBUS_STATUS       "Status"
#define NGF_DBUS_METHOD_PLAY  "Play"
#define NGF_DBUS_METHOD_STOP  "Stop"
#define NGF_DBUS_METHOD_PAUSE "Pause"
#define NGF_DBUS_METHOD_DEBUG "internal_debug"

#define NGF_DBUS_PROPERTY_NAME "dbus.event.client"

#define DBUS_CLIENT_MATCH "type='signal',sender='org.freedesktop.DBus',member='NameOwnerChanged'"

#define DBUS_MCE_NAME         "com.nokia.mce"

#define DBUSIF_REQUEST_LIMIT    "request_limit"
#define DBUSIF_CLIENT_LIMIT     "client_limit"
#define DEFAULT_REQUEST_LIMIT   (16)
#define DEFAULT_CLIENT_LIMIT    (64)

/* from ngf/core-player.h */
#define N_DBUS_EVENT_FAILED     (0)
#define N_DBUS_EVENT_COMPLETED  (1)

static uint32_t          dbusif_max_requests;
static uint32_t          dbusif_max_clients;

static gboolean          msg_parse_variant       (DBusMessageIter *iter,
                                                  NProplist *proplist,
                                                  const char *key);
static gboolean          msg_parse_dict          (DBusMessageIter *iter,
                                                  NProplist *proplist);
static gboolean          msg_get_properties      (DBusMessageIter *iter,
                                                  NProplist **properties);
static DBusHandlerResult dbusif_message_function (DBusConnection *connection,
                                                  DBusMessage *msg,
                                                  void *userdata);

static NRequest*         dbusif_lookup_request   (NInputInterface *iface,
                                                  uint32_t event_id);
static int               dbusif_initialize       (NInputInterface *iface);
static void              dbusif_shutdown         (NInputInterface *iface);
static void              dbusif_send_error       (NInputInterface *iface,
                                                  NRequest *request,
                                                  const char *err_msg);
static void              dbusif_send_reply       (NInputInterface *iface,
                                                  NRequest *request,
                                                  int code);

typedef struct _DBusInterfaceData
{
    DBusConnection  *connection;
    NInputInterface *iface;
    GSList *clients; // Internal cache of all clients currently connected
    uint32_t client_count;
} DBusInterfaceData;

typedef struct _DBusInterfaceClient
{
    uint32_t    ref;
    uint32_t    active_requests;
    char        name[1];
} DBusInterfaceClient;

static gboolean
msg_parse_variant (DBusMessageIter *iter, NProplist *proplist, const char *key)
{
    DBusMessageIter variant;

    const char *str_value = NULL;
    dbus_uint32_t uint_value;
    dbus_int32_t int_value;
    dbus_bool_t boolean_value;

    if (!key)
        return FALSE;

    if (dbus_message_iter_get_arg_type (iter) != DBUS_TYPE_VARIANT)
        return FALSE;

    dbus_message_iter_recurse (iter, &variant);

    switch (dbus_message_iter_get_arg_type (&variant)) {
        case DBUS_TYPE_STRING:
            dbus_message_iter_get_basic (&variant, &str_value);
            n_proplist_set_string (proplist, key, str_value);
            return TRUE;

        case DBUS_TYPE_UINT32:
            dbus_message_iter_get_basic (&variant, &uint_value);
            n_proplist_set_uint (proplist, key, uint_value);
            return TRUE;

        case DBUS_TYPE_INT32:
            dbus_message_iter_get_basic (&variant, &int_value);
            n_proplist_set_int (proplist, key, int_value);
            return TRUE;

        case DBUS_TYPE_BOOLEAN:
            dbus_message_iter_get_basic (&variant, &boolean_value);
            n_proplist_set_bool (proplist, key, boolean_value ? TRUE : FALSE);
            return TRUE;

        default:
            break;
    }

    return FALSE;
}

static gboolean
msg_parse_dict (DBusMessageIter *iter, NProplist *proplist)
{
    const char      *key = NULL;
    DBusMessageIter  dict;

    /* Recurse to the dict entry */

    if (dbus_message_iter_get_arg_type (iter) != DBUS_TYPE_DICT_ENTRY)
        return FALSE;

    dbus_message_iter_recurse (iter, &dict);

    /* Get the key for the dict entry */

    if (dbus_message_iter_get_arg_type (&dict) != DBUS_TYPE_STRING)
        return FALSE;

    dbus_message_iter_get_basic (&dict, &key);
    dbus_message_iter_next (&dict);

    /* Parse the variant contents */
    if (!msg_parse_variant (&dict, proplist, key))
        return FALSE;

    return TRUE;
}

static gboolean
msg_get_properties (DBusMessageIter *iter, NProplist **properties)
{
    NProplist       *p = NULL;
    DBusMessageIter  array;

    if (dbus_message_iter_get_arg_type (iter) != DBUS_TYPE_ARRAY)
        return FALSE;

    p = n_proplist_new ();

    dbus_message_iter_recurse (iter, &array);
    while (dbus_message_iter_get_arg_type (&array) != DBUS_TYPE_INVALID) {
        (void) msg_parse_dict (&array, p);
        dbus_message_iter_next (&array);
    }

    *properties = p;

    return TRUE;
}

static void
dbusif_ack (DBusConnection *connection, DBusMessage *msg, uint32_t event_id)
{
    DBusMessage *reply = NULL;

    reply = dbus_message_new_method_return (msg);
    if (reply) {
        dbus_message_append_args (reply, DBUS_TYPE_UINT32, &event_id, DBUS_TYPE_INVALID);
        dbus_connection_send (connection, reply, NULL);
        dbus_message_unref (reply);
    }
}

static void
dbusif_reply_error (DBusConnection *connection, DBusMessage *msg,
                    const char *error_name, const char *error_message)
{
    DBusMessage *reply = NULL;
    const char *dbus_error_name;
    const char *dbus_error_msg;

    dbus_error_name = error_name ?: DBUS_ERROR_FAILED;
    dbus_error_msg = error_message ?: "Unknown error.";
    N_DEBUG (LOG_CAT "reply error: %s (%s)", dbus_error_name, dbus_error_msg);
    reply = dbus_message_new_error (msg, dbus_error_name, dbus_error_msg);
    if (reply) {
        dbus_connection_send (connection, reply, NULL);
        dbus_message_unref (reply);
    }
}

static DBusInterfaceClient*
client_new (const char *client_name)
{
    DBusInterfaceClient *c;

    c = g_malloc (sizeof (*c) + strlen (client_name));
    c->ref = 1;
    c->active_requests = 0;
    strcpy(c->name, client_name);
    N_DEBUG (LOG_CAT ">> new client (%s)", c->name);

    return c;
}

static DBusInterfaceClient*
client_ref (DBusInterfaceClient *client)
{
    client->ref++;
    return client;
}

static void
client_free (DBusInterfaceClient *client)
{
    g_free (client);
}

static void
client_unref (DBusInterfaceClient *client)
{
    client->ref--;
    g_assert(client->ref != (uint32_t) -1);
    if (client->ref == 0)
        client_free (client);
}

static inline void
client_request_new (DBusInterfaceClient *client)
{
    client->active_requests++;
}

static inline void
client_request_done (DBusInterfaceClient *client)
{
    if (client->active_requests == 0)
        N_ERROR (LOG_CAT "client '%s' active requests 0", client->name);
    else
        client->active_requests--;
}

static DBusInterfaceClient*
client_list_find (DBusInterfaceData *idata, const char *client_name)
{
    GSList              *search;
    DBusInterfaceClient *c;

    for (search = idata->clients; search; search = g_slist_next(search)) {
        c = search->data;
        if (g_str_equal (client_name, c->name))
            return c;
    }

    return NULL;
}

static void
client_list_remove (DBusInterfaceData *idata, DBusInterfaceClient *client)
{
    GSList *search;

    if ((search = g_slist_find (idata->clients, client))) {
        idata->clients = g_slist_delete_link (idata->clients, search);
        idata->client_count--;
    } else
        N_ERROR (LOG_CAT "cannot find client %s from client list.", client->name);
}

static void
client_list_add (DBusInterfaceData *idata, DBusInterfaceClient *client)
{
    idata->clients = g_slist_append (idata->clients, client);
    idata->client_count++;
}

static DBusHandlerResult
dbusif_play_handler (DBusConnection *connection, DBusMessage *msg,
                     NInputInterface *iface)
{
    DBusInterfaceData   *idata      = NULL;
    const char          *event      = NULL;
    NProplist           *properties = NULL;
    NRequest            *request    = NULL;
    DBusMessageIter      iter;
    const char          *sender     = NULL;
    DBusInterfaceClient *client     = NULL;
    const char          *error      = NULL;

    idata = n_input_interface_get_userdata (iface);

    // We won't launch events without proper sender
    if ((sender = dbus_message_get_sender (msg)) == NULL)
        goto fail;

    if (!(client = client_list_find(idata, sender))) {
        if (idata->client_count >= dbusif_max_clients) {
            error = "Too many simultaneous clients.";
            goto limits;
        }
        client = client_new (sender);
        client_list_add (idata, client);
    } else if (client->active_requests >= dbusif_max_requests) {
        error = "Too many simultaneous requests.";
        goto limits;
    }

    dbus_message_iter_init (msg, &iter);
    if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_STRING)
        goto fail;

    dbus_message_iter_get_basic (&iter, &event);
    dbus_message_iter_next (&iter);

    if (!msg_get_properties (&iter, &properties))
        goto fail;

    client_ref (client);
    client_request_new (client);

    n_proplist_set_pointer (properties, NGF_DBUS_PROPERTY_NAME, client);
    request = n_request_new_with_event_and_properties (event, properties);
    n_proplist_free (properties);

    N_INFO (LOG_CAT ">> play received for event '%s' with id '%u' (client %s : %u active request(s))",
                    event, n_request_get_id (request), client->name, client->active_requests);

    // Reply internal event_id immediately
    dbusif_ack (connection, msg, n_request_get_id (request));

    n_input_interface_play_request (iface, request);

    return DBUS_HANDLER_RESULT_HANDLED;

limits:
    dbusif_reply_error (connection, msg, DBUS_ERROR_LIMITS_EXCEEDED, error);
    return DBUS_HANDLER_RESULT_HANDLED;

fail:
    dbusif_reply_error (connection, msg, DBUS_ERROR_INVALID_ARGS, "Malformed method call.");
    return DBUS_HANDLER_RESULT_HANDLED;
}

static NRequest*
dbusif_lookup_request (NInputInterface *iface, uint32_t event_id)
{
    g_assert (iface != NULL);

    NCore     *core            = NULL;
    NRequest  *request         = NULL;
    GList     *active_requests = NULL;
    GList     *iter            = NULL;

    if (event_id == 0)
        return NULL;

    core = n_input_interface_get_core (iface);
    active_requests = n_core_get_requests (core);

    for (iter = g_list_first (active_requests); iter; iter = g_list_next (iter)) {
        request = (NRequest*) iter->data;

        if (n_request_get_id (request) == event_id)
            return request;
    }

    return NULL;
}

static void
dbusif_stop_all (NInputInterface *iface)
{
    g_assert (iface != NULL);

    NCore     *core            = NULL;
    NRequest  *request         = NULL;
    GList     *active_requests = NULL;
    GList     *iter            = NULL;

    core = n_input_interface_get_core (iface);
    active_requests = n_core_get_requests (core);

    for (iter = g_list_first (active_requests); iter; iter = g_list_next (iter)) {
        request = (NRequest*) iter->data;
        n_input_interface_stop_request (iface, request, 0);
    }
}

static void
dbusif_stop_by_client (DBusInterfaceData *idata, DBusInterfaceClient *by_client)
{
    g_assert (idata != NULL);
    g_assert (by_client);

    NCore               *core               = NULL;
    NRequest            *request            = NULL;
    GList               *active_requests    = NULL;
    GList               *iter               = NULL;
    NProplist           *properties         = NULL;
    DBusInterfaceClient *client             = NULL;

    core = n_input_interface_get_core (idata->iface);
    active_requests = n_core_get_requests (core);

    for (iter = g_list_first (active_requests); iter; iter = g_list_next (iter)) {
        request = (NRequest*) iter->data;

        properties = (NProplist*) n_request_get_properties (request);
        if (!properties)
            continue;

        client = n_proplist_get_pointer (properties, NGF_DBUS_PROPERTY_NAME);
        if (client == by_client)
            n_input_interface_stop_request (idata->iface, request, 0);
    }
}

static DBusHandlerResult
dbusif_stop_handler (DBusConnection *connection, DBusMessage *msg,
                     NInputInterface *iface)
{
    DBusInterfaceData   *idata      = NULL;
    dbus_uint32_t        event_id   = 0;
    NRequest            *request    = NULL;
    const char          *sender     = NULL;
    const char          *error      = NULL;

    idata = n_input_interface_get_userdata (iface);

    if ((sender = dbus_message_get_sender (msg)) == NULL) {
        error = "Unknown sender.";
        goto access;
    }

    if (!client_list_find(idata, sender)) {
        error = "Unknown client.";
        goto access;
    }

    if (!dbus_message_get_args (msg, NULL,
                                DBUS_TYPE_UINT32, &event_id,
                                DBUS_TYPE_INVALID))
    {
        error = "Malformed method call.";
        goto args;
    }

    N_INFO (LOG_CAT ">> stop received for id '%u'", event_id);

    request = dbusif_lookup_request (iface, event_id);

    if (!request) {
        error = "No event with given id found.";
        goto args;
    }

    n_input_interface_stop_request (iface, request, 0);

    dbusif_ack (connection, msg, event_id);

    return DBUS_HANDLER_RESULT_HANDLED;

access:
    dbusif_reply_error (connection, msg, DBUS_ERROR_ACCESS_DENIED, error);

args:
    dbusif_reply_error (connection, msg, DBUS_ERROR_INVALID_ARGS, error);
    return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
dbusif_debug_handler (DBusConnection *connection, DBusMessage *msg,
                      NInputInterface *iface)
{
    DBusMessage         *reply          = NULL;
    DBusInterfaceData   *idata          = NULL;
    GSList              *search         = NULL;
    DBusInterfaceClient *client         = NULL;
    uint32_t             total_clients  = 0;
    uint32_t             total_requests = 0;

    idata = n_input_interface_get_userdata (iface);

    N_INFO (LOG_CAT "==== DUMP STATS ====");

    for (search = idata->clients; search; search = g_slist_next(search)) {
        client = (DBusInterfaceClient *) search->data;
        N_INFO (LOG_CAT "client %s  ref %d, active_requests %u/%u",
                        client->name, client->ref,
                        client->active_requests, dbusif_max_requests);
        total_requests += client->active_requests;
        total_clients++;
    }

    N_INFO (LOG_CAT "total clients %u/%u, per-client max requests %u , active requests %u",
                    total_clients, dbusif_max_clients,
                    dbusif_max_requests, total_requests);
    N_INFO (LOG_CAT "====================");

    if (!dbus_message_get_no_reply (msg)) {
        reply = dbus_message_new_method_return (msg);
        if (reply) {
            dbus_connection_send (connection, reply, NULL);
            dbus_message_unref (reply);
        }
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dbusif_pause_handler (DBusConnection *connection, DBusMessage *msg,
                      NInputInterface *iface)
{
    DBusInterfaceData  *idata       = NULL;
    const char         *sender      = NULL;
    dbus_uint32_t       event_id    = 0;
    dbus_bool_t         pause       = FALSE;
    NRequest           *request     = NULL;
    const char         *error       = NULL;

    idata = n_input_interface_get_userdata (iface);

    if ((sender = dbus_message_get_sender (msg)) == NULL) {
        error = "Unknown sender.";
        goto access;
    }

    if (!client_list_find(idata, sender)) {
        error = "Unknown client.";
        goto access;
    }

    if (!dbus_message_get_args (msg, NULL,
                                DBUS_TYPE_UINT32, &event_id,
                                DBUS_TYPE_BOOLEAN, &pause,
                                DBUS_TYPE_INVALID))
    {
        error = "Malformed method call.";
        goto args;
    }

    N_INFO (LOG_CAT ">> %s received for id '%u'", pause ? "pause" : "resume",
        event_id);

    request = dbusif_lookup_request (iface, event_id);

    if (!request) {
        error = "No event with given id found.";
        goto args;
    }

    if (pause)
        (void) n_input_interface_pause_request (iface, request);
    else
        (void) n_input_interface_play_request (iface, request);

    dbusif_ack (connection, msg, event_id);

    return DBUS_HANDLER_RESULT_HANDLED;

access:
    dbusif_reply_error (connection, msg, DBUS_ERROR_ACCESS_DENIED, error);
    return DBUS_HANDLER_RESULT_HANDLED;

args:
    dbusif_reply_error (connection, msg, DBUS_ERROR_INVALID_ARGS, error);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static void
dbusif_disconnect_handler (NInputInterface *iface, const gchar *client_name)
{
    DBusInterfaceData   *idata  = NULL;
    DBusInterfaceClient *client = NULL;

    g_assert (iface);
    g_assert (client_name);

    idata = n_input_interface_get_userdata (iface);

    if ((client = client_list_find (idata, client_name))) {
        N_INFO (LOG_CAT ">> client disconnect (%s)", client->name);
        dbusif_stop_by_client (idata, client);
        client_list_remove (idata, client);
        client_unref (client);
    }
}

static DBusHandlerResult
dbusif_introspect_handler (DBusConnection *connection, DBusMessage *msg)
{

    N_DEBUG (LOG_CAT "Introspect was called from %s", dbus_message_get_sender(msg));

    DBusMessage *reply = NULL;

    reply = dbus_message_new_method_return (msg);
    if (reply) {
        dbus_message_append_args (reply, DBUS_TYPE_STRING, &dbus_plugin_introspect_string, DBUS_TYPE_INVALID);
        dbus_connection_send (connection, reply, NULL);
        dbus_message_unref (reply);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dbusif_message_function (DBusConnection *connection, DBusMessage *msg,
                         void *userdata)
{
    NInputInterface *iface  = (NInputInterface*) userdata;
    const char      *member = dbus_message_get_member (msg);
    DBusError error = DBUS_ERROR_INIT;
    gchar *component = NULL;
    gchar *s1 = NULL;
    gchar *s2 = NULL;

    if (dbus_message_is_signal (msg, "org.freedesktop.DBus", "NameOwnerChanged")) {
        if (!dbus_message_get_args
            (msg, &error,
            DBUS_TYPE_STRING, &component,
            DBUS_TYPE_STRING, &s1,
            DBUS_TYPE_STRING, &s2,
            DBUS_TYPE_INVALID)) {
            if (dbus_error_is_set (&error)) {
                N_WARNING (LOG_CAT "D-Bus error: %s", error.message);
                dbus_error_free (&error);
            }
        } else {
            if (component && g_str_equal (component, "org.freedesktop.ohm")) {
                N_INFO (LOG_CAT "Ohmd restarted, stopping all requests");
                dbusif_stop_all (iface);
            } else if (component && g_str_equal (component, DBUS_MCE_NAME)) {
                N_INFO (LOG_CAT DBUS_MCE_NAME " restarted, stopping all requests");
                dbusif_stop_all (iface);
            } else if (component)
                dbusif_disconnect_handler (iface, component);
        }

        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (member == NULL)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (dbus_message_has_interface (msg, "org.freedesktop.DBus.Introspectable"))
        return dbusif_introspect_handler (connection, msg);

    if (!dbus_message_has_interface (msg, NGF_DBUS_IFACE))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (g_str_equal (member, NGF_DBUS_METHOD_PLAY))
        return dbusif_play_handler (connection, msg, iface);

    else if (g_str_equal (member, NGF_DBUS_METHOD_STOP))
        return dbusif_stop_handler (connection, msg, iface);

    else if (g_str_equal (member, NGF_DBUS_METHOD_PAUSE))
        return dbusif_pause_handler (connection, msg, iface);

    else if (g_str_equal (member, NGF_DBUS_METHOD_DEBUG))
        return dbusif_debug_handler (connection, msg, iface);

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int
dbusif_initialize (NInputInterface *iface)
{
    static struct DBusObjectPathVTable method = {
        .message_function = dbusif_message_function
    };

    DBusInterfaceData *idata;
    DBusError error;
    int       ret;

    idata = g_new0 (DBusInterfaceData, 1);
    idata->iface = iface;
    n_input_interface_set_userdata (iface, idata);

    dbus_error_init (&error);
    idata->connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
    if (!idata->connection) {
        N_ERROR (LOG_CAT "failed to get system bus: %s", error.message);
        goto error;
    }

    dbus_gmain_set_up_connection (idata->connection, NULL);

    ret = dbus_bus_request_name (idata->connection, NGF_DBUS_NAME,
        DBUS_NAME_FLAG_REPLACE_EXISTING, &error);

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        if (dbus_error_is_set (&error))
            N_ERROR (LOG_CAT "failed to get unique name: %s", error.message);
        goto error;
    }

    if (!dbus_connection_register_object_path (idata->connection,
        NGF_DBUS_PATH, &method, iface))
        goto error;

    /* Monitor for ohmd restarts and disconnecting clients*/
    dbus_bus_add_match (idata->connection, DBUS_CLIENT_MATCH, NULL);
    dbus_connection_add_filter (idata->connection, dbusif_message_function, iface, NULL);

    return TRUE;

error:
    g_free (idata);
    if (dbus_error_is_set (&error))
        dbus_error_free (&error);
    return FALSE;
}

static void
dbusif_shutdown (NInputInterface *iface)
{
    DBusInterfaceData *idata;

    idata = n_input_interface_get_userdata (iface);

    if (idata && idata->connection)
        dbus_connection_unref (idata->connection);

    g_free (idata);
}

static void
dbusif_send_error (NInputInterface *iface, NRequest *request,
                   const char *err_msg)
{
    N_DEBUG (LOG_CAT "error occurred for request '%s': %s",
        n_request_get_name (request), err_msg);

    dbusif_send_reply (iface, request, N_DBUS_EVENT_FAILED);
}

static void
dbusif_send_reply (NInputInterface *iface, NRequest *request, int code)
{
    DBusInterfaceData   *idata   = NULL;
    DBusMessage         *msg     = NULL;
    const NProplist     *props   = NULL;
    guint               event_id = 0;
    DBusInterfaceClient *client  = NULL;
    guint               status   = N_DBUS_EVENT_FAILED;

    idata = n_input_interface_get_userdata (iface);

    props  = n_request_get_properties (request);
    event_id = n_request_get_id (request);
    status = code;

    if (event_id == 0)
        return;

    N_DEBUG (LOG_CAT "sending reply for request '%s' (event.id=%d) with code %d",
        n_request_get_name (request), event_id, code);

    if ((msg = dbus_message_new_signal (NGF_DBUS_PATH,
                                        NGF_DBUS_IFACE,
                                        NGF_DBUS_STATUS)) == NULL) {
        N_WARNING (LOG_CAT "failed to construct signal.");
        goto end;
    }

    dbus_message_append_args (msg,
        DBUS_TYPE_UINT32, &event_id,
        DBUS_TYPE_UINT32, &status,
        DBUS_TYPE_INVALID);

    dbus_connection_send (idata->connection, msg, NULL);
    dbus_message_unref (msg);

end:
    if (code == N_DBUS_EVENT_FAILED || code == N_DBUS_EVENT_COMPLETED) {
        client = n_proplist_get_pointer (props, NGF_DBUS_PROPERTY_NAME);
        client_request_done (client);
        client_unref (client);
    }
}

int
n_plugin__load (NPlugin *plugin)
{
    static const NInputInterfaceDecl iface = {
        .name       = "dbus",
        .initialize = dbusif_initialize,
        .shutdown   = dbusif_shutdown,
        .send_error = dbusif_send_error,
        .send_reply = dbusif_send_reply
    };

    const NProplist *props;
    const char *value;

    dbusif_max_requests = DEFAULT_REQUEST_LIMIT;
    dbusif_max_clients = DEFAULT_CLIENT_LIMIT;

    props = n_plugin_get_params (plugin);

    if (n_proplist_has_key (props, DBUSIF_REQUEST_LIMIT) &&
        (value = n_proplist_get_string (props, DBUSIF_REQUEST_LIMIT))) {
        dbusif_max_requests = atoi (value);
    }

    if (n_proplist_has_key (props, DBUSIF_CLIENT_LIMIT) &&
        (value = n_proplist_get_string (props, DBUSIF_CLIENT_LIMIT))) {
        dbusif_max_clients = atoi (value);
    }

    /* register the DBus interface as the NInputInterface */
    n_plugin_register_input (plugin, &iface);

    return 1;
}

void
n_plugin__unload (NPlugin *plugin)
{
    (void) plugin;
}
