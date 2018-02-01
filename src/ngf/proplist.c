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

#include <ngf/log.h>
#include <ngf/proplist.h>

#define LOG_CAT "proplist: "

struct _NProplist {
    GHashTable *values;
};

static void    n_proplist_free_value    (gpointer data);
static void    n_proplist_replace_value (gpointer key, gpointer value, gpointer userdata);



static void
n_proplist_free_value (gpointer data)
{
    n_value_free ((NValue*) data);
}

static void
n_proplist_replace_value (gpointer key, gpointer value, gpointer userdata)
{
    GHashTable *values = (GHashTable*) userdata;
    NValue    *v       = (NValue*) value;

    g_hash_table_replace (values, g_strdup (key), n_value_copy (v));
}

NProplist*
n_proplist_new ()
{
    NProplist *proplist = NULL;

    proplist = g_slice_new0 (NProplist);
    proplist->values = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
        n_proplist_free_value);

    return proplist;
}

NProplist*
n_proplist_copy (const NProplist *source)
{
    NProplist *proplist = NULL;

    if (!source)
        return NULL;

    proplist = n_proplist_new ();
    g_hash_table_foreach (source->values, n_proplist_replace_value, proplist->values);
    return proplist;
}

NProplist*
n_proplist_copy_keys (const NProplist *source, GList *keys)
{
    NProplist  *proplist = NULL;
    NValue     *value    = NULL;
    GList      *iter     = NULL;

    proplist = n_proplist_new ();
    for (iter = g_list_first (keys); iter; iter = g_list_next (iter)) {
        if ((value = n_proplist_get (source, (const char*) iter->data))) {
            g_hash_table_insert (proplist->values,
                g_strdup ((const char*) iter->data), n_value_copy (value));
        }
    }

    return proplist;
}

void
n_proplist_merge (NProplist *target, const NProplist *source)
{
    if (!target || !source)
        return;

    g_hash_table_foreach (source->values, n_proplist_replace_value,
        target->values);
}

void
n_proplist_merge_keys (NProplist *target, const NProplist *source, GList *keys)
{
    NValue *value = NULL;
    GList  *iter  = NULL;

    if (!target || !source)
        return;

    if (!keys)
        n_proplist_merge (target, source);

    for (iter = g_list_first (keys); iter; iter = g_list_next (iter)) {
        if ((value = n_proplist_get (source, (const char*) iter->data))) {
            g_hash_table_replace (target->values,
                g_strdup ((const char*) iter->data), n_value_copy (value));
        }
    }
}

void
n_proplist_free (NProplist *proplist)
{
    if (!proplist)
        return;

    g_hash_table_destroy (proplist->values);
    g_slice_free (NProplist, proplist);
}

int
n_proplist_size (const NProplist *proplist)
{
    if (!proplist)
        return 0;

    return g_hash_table_size (proplist->values);
}

void
n_proplist_foreach (const NProplist *proplist, NProplistFunc func, gpointer userdata)
{
    const char     *key   = NULL;
    NValue         *value = NULL;
    GHashTableIter  iter;

    if (!proplist || !func)
        return;

    g_hash_table_iter_init (&iter, proplist->values);
    while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &value)) {
        func (key, value, userdata);
    }
}

gboolean
n_proplist_is_empty (const NProplist *proplist)
{
    return (proplist && g_hash_table_size (proplist->values) == 0) ? TRUE : FALSE;
}

gboolean
n_proplist_has_key (const NProplist *proplist, const char *key)
{
    return (proplist && g_hash_table_lookup (proplist->values, key) != NULL) ? TRUE : FALSE;
}

gboolean
n_proplist_match_exact (const NProplist *a, const NProplist *b)
{
    const char *key   = NULL;
    NValue     *value = NULL;
    NValue     *match = NULL;
    GHashTableIter iter;

    if (!a || !b)
        return FALSE;

    if (n_proplist_size (a) != n_proplist_size (b))
        return FALSE;

    /* check if the keys and values match. */

    g_hash_table_iter_init (&iter, a->values);
    while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &value)) {
        match = (NValue*) g_hash_table_lookup (b->values, key);
        if (!n_value_equals (value, match))
            return FALSE;
    }

    return TRUE;
}

void
n_proplist_unset (NProplist *proplist, const char *key)
{
    if (!proplist || !key)
        return;

    g_hash_table_remove (proplist->values, key);
}

void
n_proplist_set (NProplist *proplist, const char *key, const NValue *value)
{
    if (!proplist || !key || !value)
        return;

    g_hash_table_replace (proplist->values, g_strdup (key), (gpointer) value);
}

NValue*
n_proplist_get (const NProplist *proplist, const char *key)
{
    if (!proplist || !key)
        return NULL;

    return (NValue*) g_hash_table_lookup (proplist->values, key);
}

void
n_proplist_set_string (NProplist *proplist, const char *key, const char *value)
{
    NValue *v = NULL;

    if (!proplist || !key || !value)
        return;

    v = n_value_new ();
    n_value_set_string (v, value);
    n_proplist_set (proplist, key, v);
}

const char*
n_proplist_get_string (const NProplist *proplist, const char *key)
{
    NValue *value = NULL;

    if (!proplist || !key)
        return NULL;

    value = n_proplist_get ((NProplist*) proplist, key);
    return (value && n_value_type (value) == N_VALUE_TYPE_STRING) ?
        (const char*) n_value_get_string (value) : NULL;
}

gchar*
n_proplist_dup_string (const NProplist *proplist, const char *key)
{
    return g_strdup (n_proplist_get_string (proplist, key));
}

void
n_proplist_set_int (NProplist *proplist, const char *key, gint value)
{
    NValue *v = NULL;

    if (!proplist || !key)
        return;

    v = n_value_new ();
    n_value_set_int (v, value);
    n_proplist_set (proplist, key, v);
}

gint
n_proplist_get_int (const NProplist *proplist, const char *key)
{
    NValue *value = NULL;

    if (!proplist || !key)
        return 0;

    value = n_proplist_get (proplist, key);
    return (value && n_value_type (value) == N_VALUE_TYPE_INT) ?
        n_value_get_int (value) : 0;
}

void
n_proplist_set_uint (NProplist *proplist, const char *key, guint value)
{
    NValue *v = NULL;

    if (!proplist || !key)
        return;

    v = n_value_new ();
    n_value_set_uint (v, value);
    n_proplist_set (proplist, key, v);
}

guint
n_proplist_get_uint (const NProplist *proplist, const char *key)
{
    NValue *value = NULL;

    if (!proplist || !key)
        return 0;

    value = n_proplist_get (proplist, key);
    return (value && n_value_type (value) == N_VALUE_TYPE_UINT) ?
        n_value_get_uint (value) : 0;
}

void
n_proplist_set_bool (NProplist *proplist, const char *key, gboolean value)
{
    NValue *v = NULL;

    if (!proplist || !key)
        return;

    v = n_value_new ();
    n_value_set_bool (v, value);
    n_proplist_set (proplist, key, v);
}

gboolean
n_proplist_get_bool (const NProplist *proplist, const char *key)
{
    NValue *value = NULL;

    if (!proplist || !key)
        return FALSE;

    value = n_proplist_get ((NProplist*) proplist, key);
    return (value && n_value_type (value) == N_VALUE_TYPE_BOOL) ?
        n_value_get_bool (value) : FALSE;
}


void
n_proplist_set_pointer (NProplist *proplist, const char *key, gpointer value)
{
    NValue *v = NULL;

    if (!proplist || !key)
        return;

    v = n_value_new ();
    n_value_set_pointer (v, value);
    n_proplist_set (proplist, key, v);
}

gpointer
n_proplist_get_pointer (const NProplist *proplist, const char *key)
{
    NValue *value = NULL;

    if (!proplist || !key)
        return 0;

    value = n_proplist_get (proplist, key);
    return (value && n_value_type (value) == N_VALUE_TYPE_POINTER) ?
        n_value_get_pointer (value) : NULL;
}

void
n_proplist_dump (const NProplist *proplist)
{
    GHashTableIter iter;
    gchar *key = NULL;
    NValue *value = NULL;
    gchar *str_value = NULL;

    if (n_log_get_level() <= N_LOG_LEVEL_DEBUG) {
        g_hash_table_iter_init (&iter, proplist->values);
        while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &value)) {
            str_value = n_value_to_string (value);
            N_DEBUG (LOG_CAT "%s = %s", key, str_value);
            g_free (str_value);
        }
    }
}


