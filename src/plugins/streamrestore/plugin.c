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
#define TRANSFORM_TO_CONTEXT_KEY_PREFIX  "transform-to-context."

#define CLAMP_VALUE(in_v,in_min,in_max) \
    ((in_v) <= (in_min) ? (in_min) : ((in_v) >= (in_max) ? (in_max) : (in_v)))

#define BASE_VOLUME(base, max, volume) (base + volume * (max - base) / 100)

#define TYPE_DEFAULT    (0)
#define TYPE_BUILTIN    (1)
#define TYPE_EXTERNAL   (2)

#define USE_VAL1        (0)
#define USE_VAL_MIN     (1)

#define VOLUME_MAX      (100)

N_PLUGIN_NAME        ("stream-restore")
N_PLUGIN_VERSION     ("0.2")
N_PLUGIN_DESCRIPTION ("Volumes using Pulseaudio DBus stream restore")

typedef struct transform_entry {
    char *name;
    char *src;
    char *dst;
    gboolean dst_is_context;
    int base;
    int max;
} transform_entry;

typedef struct context_entry {
    char *key;
    guint type;
    guint use_val;
    int val;
} context_entry;

typedef struct role_entry {
    guint ref;
    char *role;
    GSList *contexts;
    int volume;
} role_entry;

static GHashTable *stream_restore_role_map = NULL; /* contains GSLists of role_entry structs */
static GList      *transform_entries       = NULL; /* contains transform_entry entries */
static guint       output_route_type_val   = 0;
static NContext   *context                 = NULL;

static void context_value_changed_cb (NContext *context, const char *key,
                                      const NValue *old_value,
                                      const NValue *new_value,
                                      void *userdata);

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

static role_entry*
role_entry_ref (role_entry *e)
{
    e->ref++;
    return e;
}

static void
context_entry_free (gpointer data)
{
    context_entry *c = data;
    g_free (c->key);
    g_free (c);
}

static void
role_entry_free (role_entry *e)
{
    g_slist_free_full (e->contexts, context_entry_free);
    g_free (e->role);
    g_free (e);
}

static void
role_entry_unref (role_entry *e)
{
    g_assert (e->ref);
    e->ref--;
    if (e->ref == 0) {
        N_DEBUG (LOG_CAT "deleting entry for role '%s'", e->role);
        role_entry_free (e);
    }
}

static context_entry*
context_entry_new (const char *key, guint type, guint use_val)
{
    context_entry *c = g_new0 (context_entry, 1);
    c->key = g_strdup (key);
    c->type = type;
    c->use_val = use_val;
    return c;
}

static void
role_entry_add_context (role_entry *e, const char *key, guint type, guint use_val)
{
    context_entry *c;

    c = context_entry_new (key, type, use_val);
    e->contexts = g_slist_append (e->contexts, c);
    role_entry_ref (e);
}

static gboolean
parse_rule (role_entry *e, guint type, char *str)
{
    char *s;

    g_assert (e);

    if (g_str_has_prefix (str, "min:") &&
        (s = strstr (str + strlen ("min:"), ","))) {

        s[0] = '\0';
        s = s + 1;
        str = str + strlen ("min:");
        if (strlen (s) == 0 || strlen (str) == 0)
            return FALSE;

        role_entry_add_context (e, str, type, USE_VAL_MIN);
        role_entry_add_context (e, s, type, USE_VAL_MIN);
    } else
        role_entry_add_context (e, str, type, USE_VAL1);

    return TRUE;
}

static gboolean
parse_rules (role_entry *e, const char *identifier, guint type, const char *str)
{
    gboolean ret = FALSE;
    char *tmp;
    char *s;
    char *end;

    tmp = g_strdup (str);

    if ((s = g_strrstr (tmp, identifier)) &&
        (end = strstr (s + strlen (identifier), ")"))) {

        end[0] = '\0';
        s = s + strlen (identifier);
        if (strlen (s) < 1 || !parse_rule (e, type, s))
            goto done;
        ret = TRUE;
    }

done:
    g_free (tmp);
    return ret;
}

static void
role_entry_parse_rules (role_entry *e, const char *str)
{
    g_assert (e);
    g_assert (str);

    parse_rules (e, "builtin@(", TYPE_BUILTIN, str);
    parse_rules (e, "external@(", TYPE_EXTERNAL, str);

    if (!e->contexts)
        role_entry_add_context (e, str, TYPE_DEFAULT, USE_VAL1);
}

static role_entry*
role_entry_new (const char *role, const char *str)
{
    role_entry *e;

    e = g_new0 (role_entry, 1);
    e->ref = 0; /* role entry's reference count == context list size */
    e->role = g_strdup (role);
    role_entry_parse_rules (e, str);

    N_DEBUG (LOG_CAT "new role entry '%s'", e->role);

    return e;
}

static gboolean
role_entry_get_volume (role_entry *entry, int *volume)
{
    GSList *i;
    context_entry *c;

    g_assert (entry);
    g_assert (volume);

    *volume = VOLUME_MAX;

    for (i = entry->contexts; i; i = i->next) {
        c = i->data;

        if (c->type == TYPE_DEFAULT) {
            *volume = c->val;
            break;
        }

        if (output_route_type() == c->type) {
            if (c->use_val == USE_VAL1) {
                *volume = c->val;
                break;
            } else
                *volume = *volume < c->val ? *volume : c->val;
        }
    }

    if (entry->volume != *volume) {
        entry->volume = *volume;
        return TRUE;
    }

    return FALSE;
}

static gboolean
role_entry_update_volume (role_entry *entry, const char *key, int volume)
{
    gboolean volume_changed = FALSE;
    GSList *i;
    context_entry *c;

    g_assert (entry);
    g_assert (key);

    for (i = entry->contexts; i; i = i->next) {
        c = i->data;

        if (c->type == TYPE_DEFAULT) {
            if (c->val != volume) {
                c->val = volume;
                volume_changed = TRUE;
            }
            break;
        }

        if (!g_strcmp0 (key, c->key)) {
            if (c->val != volume) {
                c->val = volume;
                volume_changed = TRUE;
            }
        }
    }

    return volume_changed;
}

static gboolean
role_entry_update_and_get_volume (role_entry *entry, const char *key, int volume, int *new_volume)
{
    if (role_entry_update_volume (entry, key, volume))
        return role_entry_get_volume (entry, new_volume);

    return FALSE;
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
    GSList         *entries = NULL;
    GSList         *i       = NULL;
    role_entry     *entry   = NULL;
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
    while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &entries)) {

        value = n_context_get_value (context, key);
        if (!value) {
            N_DEBUG (LOG_CAT "no value found for role '%s', key '%s' from context",
                             entry->role, key);
            continue;
        }

        if (n_value_type (value) != N_VALUE_TYPE_INT) {
            N_WARNING (LOG_CAT "invalid value type for role '%s', key '%s'",
                               entry->role, key);
            continue;
        }

        for (i = entries; i; i = i->next) {
            entry = i->data;
            volume = n_value_get_int (value);
            if (role_entry_update_and_get_volume (entry, key, volume, &volume))
                volume_controller_update (entry->role, volume);
        }
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

    if (entry->dst_is_context) {
        NValue *v = n_value_new ();
        n_value_set_int (v, new_volume);
        N_DEBUG (LOG_CAT "stream %s value changed to %d - set context %s %d", stream_name, volume, entry->dst, new_volume);
        n_context_set_value (context, entry->dst, v);
    } else {
        N_DEBUG (LOG_CAT "stream %s value changed to %d - set %s %d", stream_name, volume, entry->dst, new_volume);
        volume_controller_update (entry->dst, new_volume);
    }
}

static void
add_transform_entry (const char *name, const char *values, gboolean dst_is_context)
{
    transform_entry *entry = NULL;
    gchar **split = NULL;
    gchar **iter  = NULL;
    int field = 0;

    g_assert (name);
    g_assert (values);

    N_DEBUG (LOG_CAT "add transform %sentry %s : %s",
                     dst_is_context ? "to context " : "", name, values);

    entry = (transform_entry*) g_malloc0 (sizeof (transform_entry));
    entry->name = g_strdup (name);
    entry->dst_is_context = dst_is_context;

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
    volume_controller_get_volume (entry->src);
    return;

error:
    if (split)
        g_strfreev (split);
    N_WARNING (LOG_CAT "bad transform entry %s : %s", name, values);
    transform_entry_free (entry);
}

static void
hash_table_add_cb (gpointer data, gpointer user_data)
{
    context_entry  *c       = data;
    role_entry     *e       = user_data;
    GSList         *entries = NULL;

    if ((entries = g_hash_table_lookup (stream_restore_role_map, c->key))) {
        entries = g_slist_append (entries, e);
    } else {
        entries = g_slist_append (entries, e);
        g_hash_table_insert (stream_restore_role_map,
                             g_strdup (c->key),
                             entries);

        /* listen to the context value changes */
        n_context_subscribe_value_change (context, c->key, context_value_changed_cb, NULL);
    }
}

static void
volume_add_role_key_cb (const char *key, const NValue *value, gpointer userdata)
{
    (void) key;
    (void) value;
    (void) userdata;

    const char *new_key = NULL;
    role_entry *new_entry;
    int         volume  = 0;

    if (g_str_has_prefix (key, ROLE_KEY_PREFIX)) {
        new_key = (const char*) key + strlen (ROLE_KEY_PREFIX);

        if (new_key) {
            if ((new_entry = role_entry_new (new_key, n_value_get_string (value))))
                g_slist_foreach (new_entry->contexts, hash_table_add_cb, new_entry);
        }
    }
    else if (g_str_has_prefix (key, SET_KEY_PREFIX)) {
        new_key = (const char*) key + strlen (SET_KEY_PREFIX);

        if (new_key) {
            volume = atoi (n_value_get_string (value));
            volume_controller_update (new_key, volume);
        }
    } else if (g_str_has_prefix (key, TRANSFORM_KEY_PREFIX)) {
        new_key = (const char*) key + strlen (TRANSFORM_KEY_PREFIX);

        if (new_key)
            add_transform_entry (new_key, n_value_get_string (value), FALSE);
    } else if (g_str_has_prefix (key, TRANSFORM_TO_CONTEXT_KEY_PREFIX)) {
        new_key = (const char*) key + strlen (TRANSFORM_TO_CONTEXT_KEY_PREFIX);

        if (new_key)
            add_transform_entry (new_key, n_value_get_string (value), TRUE);
    }
}

static void
context_value_changed_cb (NContext *context, const char *key,
                          const NValue *old_value,
                          const NValue *new_value,
                          void *userdata)
{
    (void) context;
    (void) old_value;
    (void) userdata;

    GSList     *entries;
    GSList     *i;
    role_entry *entry;
    int         volume = 0;

    if (!g_strcmp0 (key, CONTEXT_ROUTE_OUTPUT_TYPE_KEY)) {
        output_route_type_val = n_value_get_uint (new_value);
        N_DEBUG (LOG_CAT "route changes to %s", output_route_type_to_string());
        return;
    }

    entries = g_hash_table_lookup (stream_restore_role_map, key);
    if (!entries)
        return;

    if (n_value_type (new_value) != N_VALUE_TYPE_INT) {
        N_WARNING (LOG_CAT "invalid value type for role context key '%s'", key);
        return;
    }

    for (i = entries; i; i = i->next) {
        entry = i->data;
        volume = n_value_get_int (new_value);
        if (role_entry_update_and_get_volume (entry, key, volume, &volume))
            volume_controller_update (entry->role, volume);
    }
}

static void
media_state_changed_cb (const char *media_state, void *userdata)
{
    NValue *v;

    (void) userdata;

    v = n_value_new ();
    n_value_set_string (v, media_state);
    n_context_set_value (context, "media.state", v);
}

static void
entry_list_free (gpointer data)
{
    GSList *entries = data;

    g_slist_free_full (entries, (GDestroyNotify) role_entry_unref);
}

static void
role_map_key_free (gpointer key)
{
    n_context_unsubscribe_value_change (context, key, context_value_changed_cb);
    g_free (key);
}

N_PLUGIN_LOAD (plugin)
{
    NCore           *core    = NULL;
    const NProplist *params  = NULL;

    core = n_plugin_get_core (plugin);
    context = n_core_get_context (core);

    stream_restore_role_map = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     role_map_key_free, entry_list_free);

    volume_controller_initialize ();

    /* load the stream restore roles we are interested in. */

    params = n_plugin_get_params (plugin);
    n_proplist_foreach (params, volume_add_role_key_cb, NULL);

    /* connect to the init done hook to query the initial values for
       roles. */

    n_core_connect (core, N_CORE_HOOK_INIT_DONE, 0, init_done_cb, plugin);

    /* listen to the context value changes */

    n_context_subscribe_value_change (context, CONTEXT_ROUTE_OUTPUT_TYPE_KEY,
                                      context_value_changed_cb, NULL);

    media_state_changed_cb ("inactive", NULL);
    volume_controller_set_media_state_subscribe_cb (media_state_changed_cb, NULL);

    return TRUE;
}

static void
transform_entry_unsubscribe_free (gpointer data)
{
    transform_entry *entry = data;
    volume_controller_unsubscribe (entry->src);
    transform_entry_free (entry);
}

N_PLUGIN_UNLOAD (plugin)
{
    n_core_disconnect (n_plugin_get_core (plugin), N_CORE_HOOK_INIT_DONE,
                       init_done_cb, plugin);

    n_context_unsubscribe_value_change (context, CONTEXT_ROUTE_OUTPUT_TYPE_KEY,
                                        context_value_changed_cb);

    if (stream_restore_role_map) {
        g_hash_table_destroy (stream_restore_role_map);
        stream_restore_role_map = NULL;
    }
    if (transform_entries) {
        volume_controller_set_subscribe_cb (NULL, NULL);
        g_list_free_full (transform_entries, transform_entry_unsubscribe_free);
        transform_entries = NULL;
    }

    volume_controller_set_media_state_subscribe_cb (NULL, NULL);

    volume_controller_shutdown ();
}
