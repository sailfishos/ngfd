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

#include <string.h>
#include <stdlib.h>
#include <ngf/plugin.h>
#include "volume-controller.h"
#include <ohm-ext/route.h>
#include "../route/keys.h"

#define LOG_CAT         "stream-restore: "
#define ROLE_KEY_PREFIX "role."
#define SET_KEY_PREFIX  "set."
#define TRANSFORM_KEY_PREFIX  "transform."

#define CLAMP_VALUE(in_v,in_min,in_max) \
    ((in_v) <= (in_min) ? (in_min) : ((in_v) >= (in_max) ? (in_max) : (in_v)))

#define BASE_VOLUME(base, max, volume) (base + volume * (max - base) / 100)

N_PLUGIN_NAME        ("stream-restore")
N_PLUGIN_VERSION     ("0.2")
N_PLUGIN_DESCRIPTION ("Volumes using Pulseaudio DBus stream restore")

typedef struct transform_entry {
    char *name;
    char *src;
    char *dst;
    int base;
    int max;
} transform_entry;

static GHashTable *stream_restore_role_map = NULL;
static GList      *transform_entries       = NULL; /* contains transform_entry entries */
static guint       output_route_type_val   = 0;

static const char*
output_route_type_to_string ()
{
    if (output_route_type_val & OHM_EXT_ROUTE_TYPE_BUILTIN)
        return "builtin";
    else if (output_route_type_val & (OHM_EXT_ROUTE_TYPE_WIRED | OHM_EXT_ROUTE_TYPE_WIRELESS))
        return "external";

    return "unknown";
}

static guint
output_route_type ()
{
    if (output_route_type_val & OHM_EXT_ROUTE_TYPE_BUILTIN)
        return TYPE_BUILTIN;
    else if (output_route_type_val & (OHM_EXT_ROUTE_TYPE_WIRED | OHM_EXT_ROUTE_TYPE_WIRELESS))
        return TYPE_EXTERNAL;

    return TYPE_DEFAULT;
}

static void
init_done_cb (NHook *hook, void *data, void *userdata)
{
    (void) hook;
    (void) data;

    NPlugin        *plugin  = (NPlugin*) userdata;
    NCore          *core    = n_plugin_get_core (plugin);
    NContext       *context = n_core_get_context (core);
    const char     *key     = NULL;
    const char     *role    = NULL;
    const NValue   *value   = NULL;
    int             volume  = 0;
    GHashTableIter  iter;

    /* query initial route */
    value = n_context_get_value (context, CONTEXT_ROUTE_OUTPUT_TYPE_KEY);
    output_route_type_val = n_value_get_uint (value);
    N_DEBUG (LOG_CAT "initial route type %s", output_route_type_to_string());

    /* query the initial volume values from the keys that we care
       about. */

    g_hash_table_iter_init (&iter, stream_restore_role_map);
    while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &role)) {
        value = (NValue*) n_context_get_value (context, key);
        if (!value) {
            N_WARNING (LOG_CAT "no value for found role '%s', key '%s' from context",
                role, key);
            continue;
        }

        if (n_value_type (value) != N_VALUE_TYPE_INT) {
            N_WARNING (LOG_CAT "invalid value type for role '%s', key '%s'",
                role, key);
            continue;
        }

        volume = n_value_get_int (value);
        (void) volume_controller_update (role, volume);
    }
}

static void
transform_entry_free (transform_entry *entry)
{
    if (entry) {
        if (entry->name)
            g_free (entry->name);
        if (entry->src)
            g_free (entry->src);
        if (entry->dst)
            g_free (entry->dst);
        g_free (entry);
    }
}

static void
volume_changed_cb (const char *stream_name, int volume, transform_entry *entry, void *userdata)
{
    int new_volume;

    (void) userdata;

    new_volume = BASE_VOLUME(entry->base, entry->max, volume);

    N_DEBUG (LOG_CAT "stream %s value changed to %d - set %s %d", stream_name, volume, entry->dst, new_volume);
    (void) volume_controller_update (entry->dst, new_volume);
}

static void
add_transform_entry (const char *name, const char *values)
{
    transform_entry *entry = NULL;
    gchar **split = NULL;
    gchar **iter  = NULL;
    int field = 0;

    g_assert (name);
    g_assert (values);

    N_DEBUG (LOG_CAT "add transform entry %s : %s", name, values);

    entry = (transform_entry*) g_malloc0 (sizeof (transform_entry));
    entry->name = g_strdup (name);

    split = g_strsplit (values, ";", -1);
    for (iter = split; *iter; ++iter) {
        switch (field) {
            case 0: entry->src = g_strdup (*iter); break;
            case 1: entry->dst = g_strdup (*iter); break;
            case 2: entry->base = CLAMP_VALUE (atoi (*iter), 0, 100); break;
            case 3: entry->max  = CLAMP_VALUE (atoi (*iter), 0, 100); break;
            default: goto error; /* too many fields */
        }
        field++;
    }

    if (field != 4)
        goto error;

    g_strfreev (split);

    transform_entries = g_list_append (transform_entries, entry);
    volume_controller_set_subscribe_cb ((volume_controller_subscribe_cb) volume_changed_cb, NULL);
    volume_controller_subscribe (entry->src, entry);
    return;

error:
    if (split)
        g_strfreev (split);
    N_WARNING (LOG_CAT "bad transform entry %s : %s", name, values);
    transform_entry_free (entry);
}

static void
volume_add_role_key_cb (const char *key, const NValue *value, gpointer userdata)
{
    (void) key;
    (void) value;
    (void) userdata;

    const char *new_key = NULL;
    int         volume  = 0;

    if (g_str_has_prefix (key, ROLE_KEY_PREFIX)) {
        new_key = (const char*) key + strlen (ROLE_KEY_PREFIX);

        if (new_key) {
            g_hash_table_replace (stream_restore_role_map,
                n_value_dup_string ((NValue*) value), g_strdup (new_key));
        }
    }
    else if (g_str_has_prefix (key, SET_KEY_PREFIX)) {
        new_key = (const char*) key + strlen (SET_KEY_PREFIX);

        if (new_key) {
            volume = atoi (n_value_get_string ((NValue*) value));
            (void) volume_controller_update (new_key, volume);
        }
    } else if (g_str_has_prefix (key, TRANSFORM_KEY_PREFIX)) {
        new_key = (const char*) key + strlen (TRANSFORM_KEY_PREFIX);

        if (new_key)
            add_transform_entry (new_key, n_value_get_string ((const NValue*) value));
    }
}

void context_value_changed_cb (NContext *context, const char *key,
                               const NValue *old_value,
                               const NValue *new_value,
                               void *userdata)
{
    (void) context;
    (void) old_value;
    (void) userdata;

    const char *role   = NULL;
    int         volume = 0;

    if (!g_strcmp0 (key, CONTEXT_ROUTE_OUTPUT_TYPE_KEY)) {
        output_route_type_val = n_value_get_uint (new_value);
        N_DEBUG (LOG_CAT "route changes to %s", output_route_type_to_string());
        return;
    }

    role = g_hash_table_lookup (stream_restore_role_map, key);
    if (!role)
        return;

    if (n_value_type ((NValue*) new_value) != N_VALUE_TYPE_INT) {
        N_WARNING (LOG_CAT "invalid value type for role '%s', key '%s'",
            role, key);
    }

    volume = n_value_get_int ((NValue*) new_value);
    (void) volume_controller_update (role, volume);
}

N_PLUGIN_LOAD (plugin)
{
    NCore     *core    = NULL;
    NContext  *context = NULL;
    NProplist *params  = NULL;

    stream_restore_role_map = g_hash_table_new_full (g_str_hash, g_str_equal,
        g_free, g_free);

    volume_controller_initialize ();

    /* load the stream restore roles we are interested in. */

    params = (NProplist*) n_plugin_get_params (plugin);
    n_proplist_foreach (params, volume_add_role_key_cb, NULL);

    /* connect to the init done hook to query the initial values for
       roles. */

    core = n_plugin_get_core (plugin);

    (void) n_core_connect (core, N_CORE_HOOK_INIT_DONE, 0,
        init_done_cb, plugin);

    /* listen to the context value changes */

    context = n_core_get_context (core);
    n_context_subscribe_value_change (context, NULL, context_value_changed_cb,
        NULL);

    return TRUE;
}

static void
transform_entry_unsubscribe_free (transform_entry *entry)
{
    volume_controller_unsubscribe (entry->src);
    transform_entry_free (entry);
}

N_PLUGIN_UNLOAD (plugin)
{
    (void) plugin;

    if (stream_restore_role_map) {
        g_hash_table_destroy (stream_restore_role_map);
        stream_restore_role_map = NULL;
    }
    if (transform_entries) {
        volume_controller_set_subscribe_cb (NULL, NULL);
        g_list_free_full (transform_entries, (GDestroyNotify) transform_entry_unsubscribe_free);
        transform_entries = NULL;
    }

    volume_controller_shutdown ();
}
