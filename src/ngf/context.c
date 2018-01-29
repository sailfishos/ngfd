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

#include "context-internal.h"

#define LOG_CAT "context: "

typedef struct _NContextSubscriber
{
    gpointer  userdata;
    NContextValueChangeFunc callback;
} NContextSubscriber;

typedef struct _NContextKey
{
    GList      *subscribers;    /* value:NContextSubscriber     */
} NContextKey;

struct _NContext
{
    NProplist  *values;
    GHashTable *keys;           /* key:gchar value:NContextKey  */
    GList      *all_keys;       /* value:NContextSubscriber     */
};

static void
broadcast_list (NContext *context, GList *list, const char *key,
                const NValue *old_value, const NValue *new_value)
{
    NContextSubscriber *subscriber = NULL;
    GList              *iter       = NULL;

    for (iter = g_list_first (list); iter; iter = g_list_next (iter)) {
        subscriber = (NContextSubscriber*) iter->data;
        subscriber->callback (context, key, old_value, new_value, subscriber->userdata);
    }
}

static void
n_context_broadcast_change (NContext *context, const char *key,
                            const NValue *old_value, const NValue *new_value)
{
    NContextKey        *context_key= NULL;
    gchar              *old_str    = NULL;
    gchar              *new_str    = NULL;

    old_str = n_value_to_string ((NValue*) old_value);
    new_str = n_value_to_string ((NValue*) new_value);

    N_DEBUG (LOG_CAT "broadcasting value change for '%s': %s -> %s", key,
        old_str, new_str);

    g_free (new_str);
    g_free (old_str);

    if ((context_key = g_hash_table_lookup (context->keys, key)))
        broadcast_list (context, context_key->subscribers, key, old_value, new_value);

    broadcast_list (context, context->all_keys, key, old_value, new_value);
}

void
n_context_set_value (NContext *context, const char *key,
                     NValue *value)
{
    NValue *old_value = NULL;

    if (!context || !key)
        return;

    old_value = n_value_copy (n_proplist_get (context->values, key));
    n_proplist_set (context->values, key, value);
    n_context_broadcast_change (context, key, old_value, value);
    n_value_free (old_value);
}

const NValue*
n_context_get_value (NContext *context, const char *key)
{
    if (!context || !key)
        return NULL;

    return (const NValue*) n_proplist_get (context->values, key);
}

int
n_context_subscribe_value_change (NContext *context, const char *key,
                                  NContextValueChangeFunc callback,
                                  void *userdata)
{
    NContextKey        *context_key = NULL;
    NContextSubscriber *subscriber  = NULL;

    if (!context || !callback)
        return FALSE;

    subscriber = g_new0 (NContextSubscriber, 1);
    subscriber->callback = callback;
    subscriber->userdata = userdata;

    if (key) {
        if (!(context_key = g_hash_table_lookup (context->keys, key))) {
            context_key = g_new0 (NContextKey, 1);
            g_hash_table_insert (context->keys, g_strdup (key), context_key);
        }
        context_key->subscribers = g_list_append (context_key->subscribers, subscriber);
    } else
        context->all_keys = g_list_append (context->all_keys, subscriber);

    N_DEBUG (LOG_CAT "subscriber added for key '%s'", key ? key : "<all keys>");

    return TRUE;
}

static void
remove_from_list (GList **list, NContextValueChangeFunc callback)
{
    NContextSubscriber  *subscriber  = NULL;
    GList               *iter        = NULL;

    for (iter = g_list_first (*list); iter; iter = g_list_next (iter)) {
        subscriber = (NContextSubscriber*) iter->data;

        if (subscriber->callback == callback) {
            *list = g_list_remove (*list, subscriber);
            g_free (subscriber);
            break;
        }
    }
}

void
n_context_unsubscribe_value_change (NContext *context, const char *key,
                                    NContextValueChangeFunc callback)
{
    NContextKey *context_key  = NULL;

    if (!context || !key || !callback)
        return;

    if (key) {
        if ((context_key = g_hash_table_lookup (context->keys, key))) {
            remove_from_list (&context_key->subscribers, callback);
            if (!context_key->subscribers)
                g_hash_table_remove (context->keys, key);
        }
    } else
        remove_from_list (&context->all_keys, callback);
}

NContext*
n_context_new ()
{
    NContext *context = NULL;

    context = g_new0 (NContext, 1);
    context->values = n_proplist_new ();
    context->keys = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free, g_free);
    return context;
}

void
n_context_free (NContext *context)
{
    g_list_free_full (context->all_keys, g_free);
    g_hash_table_destroy (context->keys);
    n_proplist_free (context->values);
    g_free (context);
}
