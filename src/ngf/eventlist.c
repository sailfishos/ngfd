/*
 * ngfd - Non-graphic feedback daemon
 *
 * Copyright (C) 2018 Jolla Ltd.
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

#include <string.h>
#include <glib.h>
#include <ngf/log.h>
#include "ngf/event.h"
#include "event-internal.h"
#include "eventrule-internal.h"
#include "eventlist-internal.h"

#define LOG_CAT "event-list: "

#define UNSET_KEY_PREFIX "%unset."
#define UNSET_EVENT_STR  "%unset_event"

static NEvent*      event_list_add_event        (NEventList *eventlist, NEvent *event);
static void         parse_defines               (NCore *core, GKeyFile *keyfile,
                                                 const char *group, GHashTable **defines);
static void         event_list_free_cb          (gpointer in_key, gpointer in_data,
                                                 gpointer userdata);
static void         event_rule_free_cb          (gpointer data);
static void         cache_rule_context_cb       (NContext *context, const char *key,
                                                 const NValue *old_value, const NValue *new_value,
                                                 void *userdata);
static void         match_event_rule_cb         (gpointer data, gpointer userdata);
static void         event_dump_value_cb         (const char *key, const NValue *value,
                                                 gpointer userdata);
static gint         sort_event_cb               (gconstpointer a, gconstpointer b);
static const char*  strip_prefix                (const char *group, const char *prefix);
static void         subscribe_event_rules_cb    (gpointer data, gpointer userdata);
static void         unsubscribe_event_rules_cb  (gpointer data, gpointer userdata);

typedef struct _NEventMatchResult
{
    NRequest *request;
    NContext *context;
    gboolean  has_match;
} NEventMatchResult;

NEventList*
n_event_list_new (NCore *core)
{
    NEventList *el;

    el              = g_new0 (NEventList, 1);
    el->core        = core;
    el->event_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    return el;
}

static void
event_dump_value_cb (const char *key, const NValue *value, gpointer userdata)
{
    (void) userdata;

    gchar *value_str = n_value_to_string ((NValue*) value);
    N_DEBUG (LOG_CAT "+ %s = %s", key, value_str);
    g_free (value_str);
}

static gint
sort_event_cb (gconstpointer a, gconstpointer b)
{
    const NEvent *ea = (const NEvent*) a;
    const NEvent *eb = (const NEvent*) b;
    const NEventRule *rule;

    guint a_context_rules = 0;
    guint a_request_rules = 0;
    guint b_context_rules = 0;
    guint b_request_rules = 0;

    const GSList *i;

    if (ea->priority != eb->priority)
        return ea->priority > eb->priority ? -1 : 1;

    for (i = ea->rules; i; i = i->next) {
        rule = i->data;
        rule->target == N_EVENT_RULE_CONTEXT ? a_context_rules++ : a_request_rules++;
    }

    for (i = eb->rules; i; i = i->next) {
        rule = i->data;
        rule->target == N_EVENT_RULE_CONTEXT ? b_context_rules++ : b_request_rules++;
    }

    if (a_request_rules != b_request_rules)
        return (a_request_rules > b_request_rules) ? -1 : 1;

    return (a_context_rules > b_context_rules) ? -1 : ((a_context_rules < b_context_rules) ? 1 : 0);
}

static void
match_key (const char *key, const char *prefix, const char **result)
{
    if (*result)
        return;

    if (g_str_has_prefix (key, prefix))
        *result = key;
}

static void
find_unset_event_cb (const char *key, const NValue *value, gpointer userdata)
{
    (void) value;
    match_key (key, UNSET_EVENT_STR, userdata);
}

static void
find_unset_key_cb (const char *key, const NValue *value, gpointer userdata)
{
    (void) value;
    match_key (key, UNSET_KEY_PREFIX, userdata);
}

/* Event may change when calling this function, so returned event
 * pointer should be used if it needs to be manipulated after adding
 * to eventlist. */
static NEvent*
event_list_add_event (NEventList *eventlist, NEvent *event)
{
    g_assert (eventlist);
    g_assert (event);

    GList  *event_list = NULL;
    GList  *iter       = NULL;
    NEvent *found      = NULL;

    /* get the event list for the specific event name. */

    event_list = g_hash_table_lookup (eventlist->event_table, event->name);

    /* iterate through the event list and try to find an event that has the
       same rules. */

    for (iter = g_list_first (event_list); iter; iter = g_list_next (iter)) {
        found = iter->data;

        if (n_event_rules_equal (found, event)) {
            const char *key = NULL;

            /* match found. merge the properties to the pre-existing event
               and free the new one. */

            n_proplist_foreach (event->properties, find_unset_event_cb, &key);
            if (key) {
                N_DEBUG (LOG_CAT "removing event '%s'", found->name);
                n_event_rules_dump (found, LOG_CAT);

                /* first remove event from all events list.. */
                eventlist->event_list = g_list_remove (eventlist->event_list, found);
                /* then from event specific entry list */
                event_list = g_list_remove (event_list, found);
                if (event_list)
                    g_hash_table_replace (eventlist->event_table, g_strdup (found->name), event_list);
                else
                    g_hash_table_remove (eventlist->event_table, found->name);
                n_event_free (found);
                n_event_free (event);

                return NULL;
            }

            N_DEBUG (LOG_CAT "merging event '%s'", found->name);
            n_event_rules_dump (found, LOG_CAT);

            while (TRUE) {
                key = NULL;
                n_proplist_foreach (event->properties, find_unset_key_cb, &key);
                if (!key)
                    break;
                n_proplist_unset (found->properties, key + (sizeof (UNSET_KEY_PREFIX) - 1));
                n_proplist_unset (event->properties, key);
            }
            n_proplist_merge (found->properties, event->properties);
            n_event_free (event);

            N_DEBUG (LOG_CAT "merged properties:", found->name);
            n_proplist_foreach (found->properties, event_dump_value_cb, NULL);

            return found;
        }
    }

    /* completely new event, add it to the list and sort it. */

    N_DEBUG (LOG_CAT "new event '%s'", event->name);
    if (n_event_rules_size (event) > 0)
        n_event_rules_dump (event, LOG_CAT);
    else
        N_DEBUG (LOG_CAT "+ default");

    N_DEBUG (LOG_CAT "properties");
    n_proplist_foreach (event->properties, event_dump_value_cb, NULL);

    event_list = g_list_append (event_list, event);
    event_list = g_list_sort (event_list, sort_event_cb);
    g_hash_table_replace (eventlist->event_table, g_strdup (event->name), event_list);

    eventlist->event_list = g_list_append (eventlist->event_list, event);

    return event;
}

static void
parse_defines (NCore *core, GKeyFile *keyfile,
               const char *group, GHashTable **defines)
{
    NProplist  *proplist = NULL;
    const char *name;
    char *key;

    g_assert (core);
    g_assert (keyfile);
    g_assert (group);
    g_assert (defines);

    if ((name = strip_prefix (group, N_EVENT_GROUP_ENTRY_DEFINE))) {
        if (!*defines)
            *defines = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free, (GDestroyNotify) n_proplist_free);

        proplist = n_event_parse_properties (keyfile, group, core->key_types, NULL);
        key = g_strdup (name);
        g_strstrip (key);
        g_hash_table_insert (*defines, key, proplist);
    }
}

static const char*
strip_prefix (const char *group, const char *prefix)
{
    const char *name = NULL;
    const char *tmp;
    size_t sep;

    if (!g_str_has_prefix (group, prefix))
        goto done;

    tmp = group + strlen (prefix);
    if (strlen (tmp) < 1)
        goto done;

    sep = strspn (tmp, " ");
    tmp = tmp + sep;

    if (strlen (tmp) < 1)
        goto done;

    name = tmp;

done:
    return name;
}

int
n_event_list_parse_keyfile (NEventList *eventlist, GKeyFile *keyfile)
{
    GHashTable *defines   = NULL;
    gchar    **group_list = NULL;
    gchar    **group      = NULL;
    NEvent    *event      = NULL;
    int        parsed     = 0;

    group_list = g_key_file_get_groups (keyfile, NULL);

    /* each unique group is considered as an event, even if split within
       separate files. */

    for (group = group_list; *group; ++group)
        parse_defines (eventlist->core, keyfile, *group, &defines);

    for (group = group_list; *group; ++group) {
        event = n_event_new_from_group (&eventlist->rule_list, keyfile, *group,
                                        eventlist->core->key_types, defines);
        if (event) {
            event = event_list_add_event (eventlist, event);
            if (event)
                g_slist_foreach (event->rules, subscribe_event_rules_cb,
                                 n_core_get_context (eventlist->core));
            parsed++;
        }
    }

    if (defines)
        g_hash_table_destroy (defines);
    g_strfreev      (group_list);

    return parsed;
}

GList*
n_event_list_get_events (NEventList *eventlist)
{
    g_assert (eventlist);

    return eventlist->event_list;
}

guint
n_event_list_size (const NEventList *eventlist)
{
    g_assert (eventlist);

    return g_list_length (eventlist->event_list);
}

static void
event_list_free_cb (gpointer in_key, gpointer in_data, gpointer userdata)
{
    (void) in_key;
    (void) userdata;

    GList      *event_list = in_data;
    GList      *iter       = NULL;
    NEvent     *event      = NULL;

    for (iter = g_list_first (event_list); iter; iter = g_list_next (iter)) {
        event = iter->data;
        n_event_free (event);
    }

    g_list_free (event_list);
}

static void
event_rule_free_cb (gpointer data)
{
    NEventRule *rule = data;
    n_event_rule_free (rule);
}

void
n_event_list_free (NEventList *eventlist)
{
    g_assert (eventlist);

    g_slist_foreach (eventlist->rule_list, unsubscribe_event_rules_cb,
                     n_core_get_context (eventlist->core));

    g_slist_free_full    (eventlist->rule_list, event_rule_free_cb);
    g_list_free          (eventlist->event_list);
    g_hash_table_foreach (eventlist->event_table, event_list_free_cb, NULL);
    g_hash_table_destroy (eventlist->event_table);
    g_free (eventlist);
}

static void
cache_rule_context_cb (NContext *context,
                       const char *key,
                       const NValue *old_value,
                       const NValue *new_value,
                       void *userdata)
{
    NEventRule *rule = userdata;

    (void) context;

    g_assert (rule->target == N_EVENT_RULE_CONTEXT);

    if (n_event_rule_cached_value_set (rule, n_event_rule_match (rule, new_value)) &&

        n_log_get_level() <= N_LOG_LEVEL_DEBUG) {

        char *old_value_str;
        char *new_value_str;
        char *rule_value_str;

        rule_value_str = n_value_to_string (rule->value);
        old_value_str = n_value_to_string (old_value);
        new_value_str = n_value_to_string (new_value);

        N_DEBUG (LOG_CAT "cache " N_EVENT_RULE_CONTEXT_PREFIX "%s(%s): %s -> %s: %s",
                         key, rule_value_str,
                         old_value_str, new_value_str,
                         n_event_rule_cached_value (rule) ? "true" : "false");
        g_free (rule_value_str);
        g_free (old_value_str);
        g_free (new_value_str);
    }
}

static void
match_event_rule_cb (gpointer data, gpointer userdata)
{
    NEventRule        *rule        = data;
    NEventMatchResult *result      = userdata;
    NRequest          *request     = result->request;
    const NValue      *match_value = NULL;

    if (!result->has_match)
        return;

    if (n_event_rule_cached (rule)) {
        if (!n_event_rule_cached_value (rule))
            result->has_match = FALSE;
        N_DEBUG (LOG_CAT "-> (cached) " N_EVENT_RULE_CONTEXT_PREFIX "'%s'-> %s",
                         rule->key,
                         result->has_match ? "true" : "false");
        return;
    }

    switch (rule->target) {
        case N_EVENT_RULE_CONTEXT:  match_value = n_context_get_value (result->context, rule->key); break;
        case N_EVENT_RULE_REQUEST:  match_value = n_proplist_get (request->properties, rule->key);  break;
    };

    result->has_match = n_event_rule_match (rule, match_value);

    n_event_rule_cached_value_set (rule, result->has_match);

    if (n_log_get_level() <= N_LOG_LEVEL_DEBUG) {
        gchar      *value_str       = NULL;
        gchar      *match_value_str = NULL;
        const char *op_str          = "";

        if (rule->op != N_EVENT_RULE_ALWAYS)
            value_str = n_value_to_string (rule->value);
        op_str = n_event_rule_op_string (rule);
        match_value_str = n_value_to_string (match_value);

        N_DEBUG (LOG_CAT "-> %s'%s': '%s' %s '%s' -> %s",
                 rule->target == N_EVENT_RULE_CONTEXT ? N_EVENT_RULE_CONTEXT_PREFIX : "",
                 rule->key, match_value_str, op_str, value_str ? value_str : "*",
                 result->has_match ? "true" : "false");

        g_free (value_str);
        g_free (match_value_str);
    }
}

NEvent*
n_event_list_match_request (NEventList *eventlist, NRequest *request)
{
    NEvent *event      = NULL;
    NEvent *found      = NULL;
    GList  *event_list = NULL;
    GList  *iter       = NULL;

    NEventMatchResult result;

    g_assert (eventlist);
    g_assert (request);

    /* find the list of events that have the same name. */

    event_list = g_hash_table_lookup (eventlist->event_table, request->name);
    if (!event_list)
        return NULL;

    /* for each event, match the properties. */

    for (iter = g_list_first (event_list); iter; iter = g_list_next (iter)) {
        event = iter->data;

        /* default event with no properties, accept. */

        if (n_event_rules_size (event) == 0) {
            found = event;
            break;
        }

        result.request    = request;
        result.context    = n_core_get_context (eventlist->core);
        result.has_match  = TRUE;

        N_DEBUG (LOG_CAT "consider event '%s' (priority %d)", event->name, event->priority);
        g_slist_foreach (event->rules, match_event_rule_cb, &result);

        if (result.has_match) {
            found = event;
            break;
        }
    }

    return found;
}

static void
subscribe_event_rules_cb (gpointer data, gpointer userdata)
{
    NEventRule *rule    = data;
    NContext   *context = userdata;

    if (rule->target == N_EVENT_RULE_CONTEXT &&
        rule->cache == N_EVENT_RULE_CACHE_INACTIVE) {
        n_context_subscribe_value_change (context, rule->key, cache_rule_context_cb, rule);
        rule->cache = N_EVENT_RULE_CACHE_UNSET;
    }
}

static void
unsubscribe_event_rules_cb (gpointer data, gpointer userdata)
{
    NEventRule *rule    = data;
    NContext   *context = userdata;

    if (rule->target == N_EVENT_RULE_CONTEXT &&
        rule->cache != N_EVENT_RULE_CACHE_INACTIVE) {
        n_context_unsubscribe_value_change (context, rule->key, cache_rule_context_cb);
        rule->cache = N_EVENT_RULE_CACHE_INACTIVE;
    }
}
