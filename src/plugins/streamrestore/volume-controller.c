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

#include <stdlib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include "volume-controller.h"

#include <ngf/log.h>

#define LOG_CAT                 "stream-restore: "

#define PULSE_CORE_PATH         "/org/pulseaudio/core1"
#define PULSE_CORE_IF           "org.PulseAudio.Core1"
#define STREAM_RESTORE_PATH     "/org/pulseaudio/stream_restore1"
#define STREAM_RESTORE_IF       "org.PulseAudio.Ext.StreamRestore1"
#define STREAM_ENTRY_IF         STREAM_RESTORE_IF ".RestoreEntry"

#define NEW_ENTRY_MEMBER        "NewEntry"
#define ENTRY_REMOVED_MEMBER    "EntryRemoved"
#define VOLUME_UPDATED_MEMBER   "VolumeUpdated"

#define NEW_ENTRY_SIGNAL        STREAM_RESTORE_IF "." NEW_ENTRY_MEMBER
#define ENTRY_REMOVED_SIGNAL    STREAM_RESTORE_IF "." ENTRY_REMOVED_MEMBER
#define VOLUME_UPDATED_SIGNAL   STREAM_ENTRY_IF "." VOLUME_UPDATED_MEMBER

#define DBUS_PROPERTIES_IF      "org.freedesktop.DBus.Properties"
#define PULSE_LOOKUP_DEST       "org.PulseAudio1"
#define PULSE_LOOKUP_PATH       "/org/pulseaudio/server_lookup1"
#define PULSE_LOOKUP_IF         "org.PulseAudio.ServerLookup1"
#define PULSE_LOOKUP_ADDRESS    "Address"

#define ADD_ENTRY_METHOD        "AddEntry"
#define LISTEN_FOR_METHOD       "ListenForSignal"
#define STOP_LISTEN_FOR_METHOD  "StopListeningForSignal"
#define DISCONNECTED_SIG        "Disconnected"
#define RETRY_TIMEOUT           2
#define VOLUME_SCALE_VALUE      65536

#define TO_PA_VOL(volume)       ((gdouble) (volume > 100 ? 100 : volume) / 100.0) * VOLUME_SCALE_VALUE
#define FROM_PA_VOL(volume)     ((gdouble) volume / (gdouble) VOLUME_SCALE_VALUE * 100.0)

typedef struct _QueueItem
{
    gchar *role;
    int    volume;
} QueueItem;

typedef struct _SubscribeItem
{
    char *stream_name;
    char *object_path;
    void *data;
} SubscribeItem;

static GQueue         *volume_queue    = NULL;
static DBusConnection *volume_bus      = NULL;
static guint           volume_retry_id = 0;
// Session bus is used to get PulseAudio dbus socket address when PULSE_DBUS_SERVER environment
// variable is not set
static DBusConnection *volume_session_bus   = NULL;
static gchar          *volume_pulse_address = NULL;

// subscribe_map contains all volume level change subscriptions (SubscribeItem), with stream_name as the key.
// object_map contains all volume change subscriptions (SubscribeItem), with object_path as the key.
// subscribe_map owns the SubscribeItems.
// When volume updated event comes from PulseAudio, object_map is used to find the corresponding
// SubscribeItem, since VolumeUpdated event has object_path as the only signal argument.
static GHashTable     *subscribe_map                     = NULL;
static GHashTable     *object_map                        = NULL;

// object_map_complete is TRUE when all SubscribeItems are in both subscribe_map and object_map.
static gboolean        object_map_complete               = FALSE;
static void           *subscribe_userdata                = NULL;
static volume_controller_subscribe_cb subscribe_callback = NULL;
static gboolean        queue_subscribe                   = FALSE;

static gboolean          retry_timeout_cb           (gpointer userdata);
static DBusHandlerResult filter_cb                  (DBusConnection *connection, DBusMessage *msg, void *data);
static void              append_volume              (DBusMessageIter *iter, guint volume);
static gboolean          add_entry                  (const char *role, guint volume);
static void              process_queued_ops         ();
static void              connect_to_pulseaudio      ();
static void              disconnect_from_pulseaudio ();
static void              retry_connect              ();
static void              get_address_reply_cb       (DBusPendingCall *pending, void *data);

static gchar*            get_object_name            (const char *obj_path);
static gchar*            get_object_path            (const char *obj_path);
static void              listen_for_signal          (const char *signal, const char **objects);
static void              stop_listen_for_signal     (const char *signal);
static void              update_object_map_listen   ();
static gboolean          get_volume                 (DBusMessage *msg, int *volume);

static void
retry_connect()
{
    volume_retry_id = g_timeout_add_seconds(RETRY_TIMEOUT,
                                            retry_timeout_cb, NULL);
}

static void
get_address_reply_cb(DBusPendingCall *pending, void *data)
{
    DBusMessageIter iter;
    DBusMessageIter sub;
    int current_type;
    char *address = NULL;
    DBusMessage *msg = NULL;

    (void) data;

    msg = dbus_pending_call_steal_reply(pending);
    if (!msg) {
        N_WARNING(LOG_CAT "Could not get reply from pending call.");
        goto retry;
    }

    dbus_message_iter_init(msg, &iter);

    // Reply string is inside DBUS_TYPE_VARIANT
    while ((current_type = dbus_message_iter_get_arg_type(&iter)) != DBUS_TYPE_INVALID) {

        if (current_type == DBUS_TYPE_VARIANT) {
            dbus_message_iter_recurse(&iter, &sub);

            while ((current_type = dbus_message_iter_get_arg_type(&sub)) != DBUS_TYPE_INVALID) {
                if (current_type == DBUS_TYPE_STRING)
                    dbus_message_iter_get_basic(&sub, &address);
                dbus_message_iter_next(&sub);
            }
        }

        dbus_message_iter_next(&iter);
    }

    if (address) {
        N_DEBUG (LOG_CAT "Got PulseAudio DBus address: %s", address);
        volume_pulse_address = g_strdup(address);

        // Unref sesssion bus connection, it is not needed anymore.
        // Real communication is done with peer-to-peer connection.
        dbus_connection_unref(volume_session_bus);
        volume_session_bus = NULL;
    }

    // Always retry connection, if address was determined, it is used
    // to get peer-to-peer connection, if address wasn't determined,
    // we'll need to reconnect and retry anyway.
retry:
    if (msg)
        dbus_message_unref(msg);
    if (pending)
        dbus_pending_call_unref(pending);

    retry_connect();
}

static gboolean
retry_timeout_cb (gpointer userdata)
{
    (void) userdata;

    N_DEBUG (LOG_CAT "Retry connecting to PulseAudio");

    disconnect_from_pulseaudio ();
    connect_to_pulseaudio ();

    return FALSE;
}

static gboolean
get_volume (DBusMessage *msg, int *volume)
{
    gboolean success = FALSE;
    DBusMessageIter msg_iter;
    DBusMessageIter array_iter;
    DBusMessageIter struct_iter;
    dbus_uint32_t channel;
    dbus_uint32_t vol;
    int channel_count = 0;
    long average_volume = 0;

    g_assert (msg);
    g_assert (volume);

    dbus_message_iter_init (msg, &msg_iter);
    dbus_message_iter_recurse (&msg_iter, &array_iter);

    while (dbus_message_iter_get_arg_type (&array_iter) != DBUS_TYPE_INVALID) {

        dbus_message_iter_recurse (&array_iter, &struct_iter);
        dbus_message_iter_get_basic (&struct_iter, &channel);

        dbus_message_iter_next (&struct_iter);
        dbus_message_iter_get_basic (&struct_iter, &vol);

        dbus_message_iter_next (&array_iter);

        average_volume += (long) vol;
        channel_count++;
    }

    if (channel_count > 0) {
        *volume = (int) (average_volume / channel_count);
        success = TRUE;
    }

    return success;
}

static DBusHandlerResult
filter_cb (DBusConnection *connection, DBusMessage *msg, void *data)
{
    (void) connection;
    (void) data;

    const char *obj_path;
    char *stream_name;
    GList *list, *i;
    SubscribeItem *item;
    int volume = 0;

    if (dbus_message_has_interface (msg, DBUS_INTERFACE_LOCAL) &&
        dbus_message_has_path      (msg, DBUS_PATH_LOCAL) &&
        dbus_message_has_member    (msg, DISCONNECTED_SIG))
    {
        N_DEBUG (LOG_CAT "pulseaudio disconnected, reconnecting in %d seconds",
            RETRY_TIMEOUT);

        disconnect_from_pulseaudio ();

        // After disconnecting re-query stream restore object names.
        if (subscribe_map && object_map) {
            list = g_hash_table_get_values (subscribe_map);

            for (i = g_list_first (list); i; i = g_list_next (i)) {
                SubscribeItem *item = (SubscribeItem*) i->data;
                if (item->object_path) {
                    g_free (item->object_path);
                    item->object_path = NULL;
                }
            }
            g_list_free (list);
            queue_subscribe = TRUE;
        }
        // If PulseAudio is restarting path to runtime files may change so we'll
        // have to request DBus address again.
        if (volume_pulse_address) {
            g_free(volume_pulse_address);
            volume_pulse_address = NULL;
        }
        retry_connect();
    }
    else if (subscribe_callback &&
             dbus_message_has_interface (msg, STREAM_RESTORE_IF) &&
             dbus_message_has_path      (msg, STREAM_RESTORE_PATH) &&
             dbus_message_has_member    (msg, NEW_ENTRY_MEMBER))
    {
        if (!dbus_message_get_args (msg, NULL, DBUS_TYPE_OBJECT_PATH, &obj_path, DBUS_TYPE_INVALID)) {
            N_WARNING (LOG_CAT "failed to get arguments for new entry");
            goto end;
        }

        // If the entry is in subscribe_map but not in object_map, look up the entry
        // and add to object_map
        if (!object_map_complete && !g_hash_table_lookup (object_map, obj_path)) {

            if ((stream_name = get_object_name (obj_path))) {

                if ((item = g_hash_table_lookup (subscribe_map, stream_name))) {
                    item->object_path = g_strdup (obj_path);
                    N_DEBUG (LOG_CAT "stream restore entry for %s appeared (%s)", item->stream_name, item->object_path);
                    update_object_map_listen ();
                }
                g_free (stream_name);
            }
        }
    }
    else if (subscribe_callback &&
             dbus_message_has_interface (msg, STREAM_RESTORE_IF) &&
             dbus_message_has_path      (msg, STREAM_RESTORE_PATH) &&
             dbus_message_has_member    (msg, ENTRY_REMOVED_MEMBER))
    {
        if (!dbus_message_get_args (msg, NULL, DBUS_TYPE_OBJECT_PATH, &obj_path, DBUS_TYPE_INVALID)) {
            N_WARNING (LOG_CAT "failed to get arguments for removed entry");
            goto end;
        }

        if ((item = g_hash_table_lookup (object_map, obj_path))) {
            g_hash_table_remove (object_map, item->object_path);
            g_free (item->object_path);
            item->object_path = NULL;
            update_object_map_listen ();
            N_DEBUG (LOG_CAT "removed entry %s from object map (%s)", item->stream_name, obj_path);
        }
    }
    else if (subscribe_callback &&
             dbus_message_has_interface (msg, STREAM_ENTRY_IF) &&
             dbus_message_has_member    (msg, VOLUME_UPDATED_MEMBER))
    {
        if (!(obj_path = dbus_message_get_path (msg)))
            goto end;

        if ((item = g_hash_table_lookup (object_map, obj_path))) {
            N_DEBUG (LOG_CAT "volume updated for stream %s (%s)", item->stream_name, item->object_path);
            if (get_volume (msg, &volume)) {
                subscribe_callback (item->stream_name, FROM_PA_VOL(volume), item->data, subscribe_userdata);
            }
        }
    }

end:
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
append_volume (DBusMessageIter *iter, guint volume)
{
    DBusMessageIter array, str;

    dbus_uint32_t pos = 0;
    dbus_uint32_t vol = volume;

    dbus_message_iter_open_container  (iter, DBUS_TYPE_ARRAY, "(uu)", &array);
    dbus_message_iter_open_container  (&array, DBUS_TYPE_STRUCT, NULL, &str);

    dbus_message_iter_append_basic    (&str, DBUS_TYPE_UINT32, &pos);
    dbus_message_iter_append_basic    (&str, DBUS_TYPE_UINT32, &vol);

    dbus_message_iter_close_container (&array, &str);
    dbus_message_iter_close_container (iter, &array);
}

static gboolean
add_entry (const char *role, guint volume)
{
    DBusMessage     *msg     = NULL;
    DBusMessage     *reply   = NULL;
    const char      *empty   = "";
    gboolean         success = FALSE;
    dbus_bool_t      muted   = FALSE;
    dbus_bool_t      apply   = TRUE;
    dbus_uint32_t    vol     = 0;
    DBusMessageIter  iter;
    DBusError        error;

    if (!volume_bus || !role)
        return FALSE;

    /* convert the volume from 0-100 to PA_VOLUME_NORM range */
    vol = TO_PA_VOL(volume);

    dbus_error_init (&error);
    msg = dbus_message_new_method_call (0, STREAM_RESTORE_PATH,
        STREAM_RESTORE_IF, ADD_ENTRY_METHOD);

    if (msg == NULL)
        goto done;

    dbus_message_iter_init_append  (msg, &iter);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &role);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &empty);
    append_volume                  (&iter, vol);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_BOOLEAN, &muted);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_BOOLEAN, &apply);

    reply = dbus_connection_send_with_reply_and_block (volume_bus,
        msg, -1, &error);

    if (!reply) {
        if (dbus_error_is_set (&error)) {
            N_WARNING (LOG_CAT "failed to update volume role '%s': %s",
                role, error.message);
        }

        goto done;
    }

    N_DEBUG (LOG_CAT "volume for role '%s' set to %d", role, vol);
    success = TRUE;

done:
    dbus_error_free (&error);

    if (reply) dbus_message_unref (reply);
    if (msg)   dbus_message_unref (msg);

    return success;
}

static void
listen_for_signal (const char *signal, const char **objects)
{
    DBusMessage     *msg      = NULL;
    const char      *empty[1] = { NULL };
    int              count;

    g_assert (volume_bus);
    g_assert (signal);

    msg = dbus_message_new_method_call (NULL,
                                        PULSE_CORE_PATH,
                                        PULSE_CORE_IF,
                                        LISTEN_FOR_METHOD);

    if (!msg)
        goto done;

    if (!objects)
        objects = empty;
    for (count = 0; objects[count]; count++);

    dbus_message_append_args (msg,
                              DBUS_TYPE_STRING, &signal,
                              DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &objects, count,
                              DBUS_TYPE_INVALID);


    if (dbus_connection_send (volume_bus, msg, NULL)) {
        int i;
        N_DEBUG (LOG_CAT "listen for signal %s", signal);
        for (i = 0; i < count; i++)
            N_DEBUG (LOG_CAT "- object path: %s", objects[i]);
    }

done:
    if (msg)
        dbus_message_unref (msg);
}

static void
stop_listen_for_signal (const char *signal)
{
    DBusMessage     *msg    = NULL;

    g_assert (volume_bus);
    g_assert (signal);

    msg = dbus_message_new_method_call (NULL,
                                        PULSE_CORE_PATH,
                                        PULSE_CORE_IF,
                                        STOP_LISTEN_FOR_METHOD);

    if (!msg)
        goto done;

    dbus_message_append_args (msg,
                              DBUS_TYPE_STRING, &signal,
                              DBUS_TYPE_INVALID);

    if (dbus_connection_send (volume_bus, msg, NULL))
        N_DEBUG (LOG_CAT "stop listening for signal %s", signal);

done:
    if (msg)
        dbus_message_unref (msg);
}

static gchar*
get_object_name (const char *obj_path)
{
    DBusMessage     *msg         = NULL;
    DBusMessage     *reply       = NULL;
    const gchar     *stream_name = NULL;
    gchar           *ret         = NULL;
    const gchar     *iface       = STREAM_ENTRY_IF;
    const gchar     *addr        = "Name";
    DBusError        error;
    DBusMessageIter  iter;
    DBusMessageIter  sub;
    int              current_type;

    g_assert (volume_bus);
    g_assert (obj_path);

    dbus_error_init (&error);

    msg = dbus_message_new_method_call(STREAM_ENTRY_IF,
                                       obj_path,
                                       DBUS_PROPERTIES_IF,
                                       "Get");

    if (msg == NULL)
        goto done;

    if (!dbus_message_append_args(msg,
                                  DBUS_TYPE_STRING, &iface,
                                  DBUS_TYPE_STRING, &addr,
                                  DBUS_TYPE_INVALID))
        goto done;

    reply = dbus_connection_send_with_reply_and_block (volume_bus,
                                                       msg, -1, &error);

    if (!reply) {
        if (dbus_error_is_set (&error)) {
            N_WARNING (LOG_CAT "couldn't get object name for %s: %s",
                       obj_path, error.message);
        }

        goto done;
    }

    dbus_message_iter_init(reply, &iter);

    // Reply string is inside DBUS_TYPE_VARIANT
    while ((current_type = dbus_message_iter_get_arg_type(&iter)) != DBUS_TYPE_INVALID) {

        if (current_type == DBUS_TYPE_VARIANT) {
            dbus_message_iter_recurse(&iter, &sub);

            while ((current_type = dbus_message_iter_get_arg_type(&sub)) != DBUS_TYPE_INVALID) {
                if (current_type == DBUS_TYPE_STRING)
                    dbus_message_iter_get_basic(&sub, &stream_name);
                dbus_message_iter_next(&sub);
            }
        }

        dbus_message_iter_next(&iter);
    }

    if (!stream_name) {
        N_WARNING (LOG_CAT "failed to get stream name");
        goto done;
    }

    ret = g_strdup (stream_name);

done:
    dbus_error_free (&error);

    if (reply) dbus_message_unref (reply);
    if (msg)   dbus_message_unref (msg);

    return ret;
}

static gchar*
get_object_path (const char *stream_name)
{
    DBusMessage     *msg     = NULL;
    DBusMessage     *reply   = NULL;
    const gchar     *obj_path= NULL;
    gchar           *ret     = NULL;
    DBusError        error;

    g_assert (volume_bus);
    g_assert (stream_name);

    dbus_error_init (&error);
    msg = dbus_message_new_method_call (NULL,
                                        STREAM_RESTORE_PATH,
                                        STREAM_RESTORE_IF,
                                        "GetEntryByName");

    if (msg == NULL)
        goto done;

    dbus_message_append_args (msg, DBUS_TYPE_STRING, &stream_name, DBUS_TYPE_INVALID);

    reply = dbus_connection_send_with_reply_and_block (volume_bus,
                                                       msg, -1, &error);

    if (!reply) {
        if (dbus_error_is_set (&error)) {
            N_DEBUG (LOG_CAT "couldn't get object path for %s: %s",
                     stream_name, error.message);
        }

        goto done;
    }

    if (!dbus_message_get_args (reply, NULL, DBUS_TYPE_OBJECT_PATH, &obj_path, DBUS_TYPE_INVALID)) {
        N_WARNING (LOG_CAT "failed to get object path");
        goto done;
    }

    ret = g_strdup (obj_path);

done:
    dbus_error_free (&error);

    if (reply) dbus_message_unref (reply);
    if (msg)   dbus_message_unref (msg);

    return ret;
}

// Tries to update all items from subscribe_map to object_map, and after this
// listens for stream restore entry signals coming from entries with object paths
// in object_map.
// If object_map is incomplete (doesn't contain all items from subscribe_map), then
// object_map_complete is FALSE.
static void
update_object_map_listen ()
{
    const char **obj_paths;
    GList *subscription_items, *i;
    int j = 0;

    if (!volume_bus || !subscribe_map || !object_map)
        return;

    g_hash_table_remove_all (object_map);
    obj_paths = g_malloc0 (sizeof (char*) * (g_hash_table_size (subscribe_map) + 1));
    subscription_items = g_hash_table_get_values (subscribe_map);

    for (i = g_list_first (subscription_items); i; i = g_list_next (i)) {
        SubscribeItem *item = (SubscribeItem*) i->data;
        if (!item->object_path)
            item->object_path = get_object_path (item->stream_name);
        if (item->object_path) {
            g_hash_table_insert (object_map, item->object_path, item);
            obj_paths[j++] = item->object_path;
        }
    }
    obj_paths[j] = NULL;

    g_list_free (subscription_items);

    listen_for_signal (VOLUME_UPDATED_SIGNAL, obj_paths);

    if (g_hash_table_size (subscribe_map) == g_hash_table_size (object_map))
        object_map_complete = TRUE;
    else
        object_map_complete = FALSE;

    g_free (obj_paths);
}

static void
process_queued_ops ()
{
    QueueItem *queued_volume = NULL;

    while ((queued_volume = g_queue_pop_head (volume_queue)) != NULL) {
        N_DEBUG (LOG_CAT "processing queued volume for role '%s', volume %d ",
            queued_volume->role, queued_volume->volume);
        add_entry (queued_volume->role, queued_volume->volume);

        g_free (queued_volume->role);
        g_slice_free (QueueItem, queued_volume);
    }

    // listen for objects here
    if (queue_subscribe) {
        listen_for_signal (NEW_ENTRY_SIGNAL, NULL);
        listen_for_signal (ENTRY_REMOVED_SIGNAL, NULL);
        update_object_map_listen ();
        queue_subscribe = FALSE;
    }
}

static gboolean
connect_peer_to_peer()
{
    DBusError   error;

    dbus_error_init (&error);
    volume_bus = dbus_connection_open (volume_pulse_address, &error);

    if (dbus_error_is_set (&error)) {
        N_WARNING (LOG_CAT "failed to open connection to pulseaudio: %s",
            error.message);
        dbus_error_free (&error);
        return FALSE;
    }

    dbus_connection_setup_with_g_main (volume_bus, NULL);

    if (!dbus_connection_add_filter (volume_bus, filter_cb, NULL, NULL)) {
        N_WARNING (LOG_CAT "failed to add filter");
        return FALSE;
    }

    process_queued_ops();

    return TRUE;
}

static gboolean
connect_get_address()
{
    DBusError error;
    DBusMessage *msg = NULL;
    DBusPendingCall *pending = NULL;
    const char *iface = PULSE_LOOKUP_IF;
    const char *addr = PULSE_LOOKUP_ADDRESS;

    dbus_error_init (&error);

    if (volume_session_bus && !dbus_connection_get_is_connected(volume_session_bus)) {
        dbus_connection_unref(volume_session_bus);
        volume_session_bus = NULL;
    }

    if (!volume_session_bus)
        volume_session_bus = dbus_bus_get(DBUS_BUS_SESSION, &error);

    if (dbus_error_is_set(&error)) {
        N_WARNING(LOG_CAT "failed to open connection to session bus: %s", error.message);
        dbus_error_free (&error);
        goto fail;
    }

    dbus_connection_setup_with_g_main(volume_session_bus, NULL);

    if (!(msg = dbus_message_new_method_call(PULSE_LOOKUP_DEST,
                                             PULSE_LOOKUP_PATH,
                                             DBUS_PROPERTIES_IF,
                                             "Get")))
        goto fail;

    if (!dbus_message_append_args(msg,
                                  DBUS_TYPE_STRING, &iface,
                                  DBUS_TYPE_STRING, &addr,
                                  DBUS_TYPE_INVALID))
        goto fail;

    if (!dbus_connection_send_with_reply(volume_session_bus,
                                         msg,
                                         &pending,
                                         DBUS_TIMEOUT_USE_DEFAULT))
        goto fail;

    if (!pending)
        goto fail;

    if (!dbus_pending_call_set_notify(pending, get_address_reply_cb, NULL, NULL))
        goto fail;

    dbus_message_unref(msg);

    return TRUE;

fail:
    if (pending) {
        dbus_pending_call_cancel(pending);
        dbus_pending_call_unref(pending);
    }
    if (msg)
        dbus_message_unref(msg);

    return FALSE;
}

static void
connect_to_pulseaudio ()
{
    const char *pulse_address = NULL;
    gboolean success;

    if (!volume_pulse_address && (pulse_address = getenv ("PULSE_DBUS_SERVER")))
        volume_pulse_address = g_strdup(pulse_address);

    if (volume_pulse_address)
        success = connect_peer_to_peer();
    else
        success = connect_get_address();

    if (!success)
        retry_connect();
}

static void
disconnect_from_pulseaudio ()
{
    if (volume_retry_id > 0) {
        g_source_remove (volume_retry_id);
        volume_retry_id = 0;
    }

    if (volume_bus) {
        dbus_connection_unref (volume_bus);
        volume_bus = NULL;
    }
}

int
volume_controller_initialize ()
{
    if ((volume_queue = g_queue_new ()) == NULL)
        return FALSE;

    volume_pulse_address = NULL;

    connect_to_pulseaudio();

    return TRUE;
}

void
volume_controller_shutdown ()
{
    disconnect_from_pulseaudio ();

    if (volume_queue) {
        g_queue_free (volume_queue);
        volume_queue = NULL;
    }

    if (volume_session_bus) {
        dbus_connection_unref(volume_session_bus);
        volume_session_bus = NULL;
    }

    if (volume_pulse_address) {
        g_free(volume_pulse_address);
        volume_pulse_address = NULL;
    }
}

int
volume_controller_update (const char *role, int volume)
{
    QueueItem *item = NULL;

    if (!role)
        return FALSE;

    if (!volume_bus) {
        N_DEBUG (LOG_CAT "volume controller not ready, queueing op.");

        item = g_slice_new0 (QueueItem);
        item->role   = g_strdup (role);
        item->volume = volume;
        g_queue_push_tail (volume_queue, item);
        return TRUE;
    }

    return add_entry (role, volume);
}

void
subscribe_item_free (SubscribeItem *item)
{
    if (item) {
        if (item->stream_name)
            g_free (item->stream_name);
        if (item->object_path)
            g_free (item->object_path);
        g_free (item);
    }
}

void
volume_controller_subscribe  (const char *stream_name, void *data)
{
    SubscribeItem *item = NULL;
    gboolean first      = FALSE;

    g_assert (stream_name);

    if (!subscribe_map) {
        subscribe_map = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               NULL, (GDestroyNotify) subscribe_item_free);
        object_map = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
        first = TRUE;
    }

    item = (SubscribeItem*) g_malloc0 (sizeof (SubscribeItem));
    item->stream_name = g_strdup (stream_name);
    item->data = data;

    g_hash_table_insert (subscribe_map, item->stream_name, item);

    if (first && volume_bus) {
        listen_for_signal (NEW_ENTRY_SIGNAL, NULL);
        listen_for_signal (ENTRY_REMOVED_SIGNAL, NULL);
    }

    if (volume_bus)
        update_object_map_listen ();
    else {
        N_DEBUG (LOG_CAT "volume controller not ready, queueing signal listening.");
        queue_subscribe = TRUE;
    }
}

void
volume_controller_unsubscribe (const char *stream_name)
{
    SubscribeItem *item;

    g_assert (stream_name);

    if (subscribe_map && (item = g_hash_table_lookup (subscribe_map, stream_name))) {
        if (item->object_path) {
            g_hash_table_remove (object_map, item->object_path);

            if (volume_bus)
                update_object_map_listen ();
        }

        g_hash_table_remove (subscribe_map, stream_name);
    }

    if (subscribe_map && g_hash_table_size (subscribe_map) == 0) {
        if (volume_bus) {
            stop_listen_for_signal (NEW_ENTRY_SIGNAL);
            stop_listen_for_signal (ENTRY_REMOVED_SIGNAL);
        }
        g_hash_table_unref (subscribe_map);
        subscribe_map = NULL;
        g_hash_table_unref (object_map);
        object_map = NULL;
    }
}

void
volume_controller_set_subscribe_cb (volume_controller_subscribe_cb cb, void *userdata)
{
    subscribe_callback = cb;
    subscribe_userdata = userdata;
}
