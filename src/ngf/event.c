/*
 * ngfd - Non-graphic feedback daemon
 *
 * Copyright (C) 2010 Nokia Corporation.
 *               2018 Jolla Ltd.
 * Contact: Xun Chen <xun.chen@nokia.com>
 *          Juho Hämäläinen <juho.hamalainen@jolla.com>
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
#include "eventrule-internal.h"
#include "event-internal.h"

#define LOG_CAT "event: "



static int      event_parse_group_title (const char *value, gchar **out_title,
                                         int *out_priority, GSList **out_rules);
static void     rule_match_cb           (gpointer data, gpointer userdata);
static void     dump_event_rules_cb     (gpointer data, gpointer userdata);
static void     merge_rules             (GSList **to, GSList *from);


NEvent*
n_event_new ()
{
    return g_new0 (NEvent, 1);
}

void
n_event_free (NEvent *event)
{
    if (event->properties) {
        n_proplist_free (event->properties);
        event->properties = NULL;
    }

    g_free (event->name);
    g_free (event);
}

guint
n_event_rules_size (const NEvent *event)
{
    if (!event)
        return 0;

    return g_slist_length (event->rules);
}

struct rule_match_data {
    NEvent  *event_b;
    gboolean equals;
};

static void
rule_match_cb (gpointer data, gpointer userdata)
{
    NEventRule             *rule_a = data;
    struct rule_match_data *match_data = userdata;
    NEventRule             *rule_b;
    GSList                 *i_b;
    gboolean                match = FALSE;

    if (!match_data->equals)
        return;

    for (i_b = match_data->event_b->rules; i_b; i_b = g_slist_next (i_b)) {
        rule_b = i_b->data;

        if (n_event_rule_equal (rule_a, rule_b)) {
            match = TRUE;
            break;
        }
    }

    match_data->equals = match;
}

int
n_event_rules_equal (NEvent *a, NEvent *b)
{
    struct rule_match_data match_data;

    if (!a->rules && !b->rules)
        return TRUE;

    if (n_event_rules_size (a) != n_event_rules_size (b))
        return FALSE;

    match_data.event_b = b;
    match_data.equals  = TRUE;

    g_slist_foreach (a->rules, rule_match_cb, &match_data);

    return match_data.equals;
}

static void
dump_event_rules_cb (gpointer data, gpointer userdata)
{
    const NEventRule *rule         = data;
    const char       *debug_prefix = userdata;

    n_event_rule_dump (rule, debug_prefix);
}

void
n_event_rules_dump (NEvent *event, const char *debug_prefix)
{
    if (event && n_log_get_level() <= N_LOG_LEVEL_DEBUG)
        g_slist_foreach (event->rules, dump_event_rules_cb, (gpointer) debug_prefix);
}

static void
merge_rules (GSList **to, GSList *from)
{
    GSList     *i_to;
    GSList     *i_from;
    NEventRule *rule;
    gboolean    new_entry;

    for (i_from = from; i_from; i_from = g_slist_next (i_from)) {
        NEventRule *new_rule = i_from->data;
        new_entry = TRUE;

        for (i_to = *to; i_to; i_to = g_slist_next (i_to)) {
            rule = i_to->data;
            if (n_event_rule_equal (new_rule, rule)) {
                new_entry = FALSE;
                break;
            }
        }

        if (new_entry) {
            *to = g_slist_append (*to, new_rule);
            N_DEBUG (LOG_CAT "new rule:");
            n_event_rule_dump (new_rule, LOG_CAT);
        } else {
            n_event_rule_free (new_rule);
            i_from->data = rule;
            N_DEBUG (LOG_CAT "cached rule:");
            n_event_rule_dump (rule, LOG_CAT);
        }
    }
}

static void
parse_priority (const char *str, int *out_priority)
{
    gint64 value;

    g_assert (str);
    g_assert (out_priority);

    /* default to lowest priority */
    *out_priority = 0;

    if (n_parse_number (str, &value)) {
        if (value < 0)
            *out_priority = 0;
        else if (value > G_MAXINT)
            *out_priority = G_MAXINT;
        else
            *out_priority = (int) value;
    }
}

static int
event_parse_group_title (const char *value, gchar **out_title,
                         int *out_priority, GSList **out_rules)
{
    g_assert (value != NULL);
    g_assert (*out_title == NULL);
    g_assert (*out_rules == NULL);
    g_assert (out_priority != NULL);

    NEventRule *rule      = NULL;
    GSList     *rule_list = NULL;
    gchar     **split     = NULL;
    gchar     **rules     = NULL;
    gchar     **iter      = NULL;
    gchar      *priority  = NULL;

    /* split the value by =>, which acts as a title and rule separator. first
       item contains the title (and optionally priority), second item contains
       unparsed rule string. */

    split = g_strsplit (value, "=>", 2);

    if ((priority = strstr (split[0], "@priority"))) {
        *priority = '\0';
        priority += strlen ("@priority");
        g_strstrip (priority);
        parse_priority (priority, out_priority);
    }

    g_strstrip (split[0]);

    /* if there are no rules, then we are done. */

    if (split[1] == NULL)
        goto done;

    /* split the rules by ",", strip each rule and make a new entry
       to the rule property list. */

    rules = g_strsplit (split[1], ",", -1);
    for (iter = rules; *iter; ++iter) {
        g_strstrip (*iter);
        if ((rule = n_event_rule_parse (*iter)))
            rule_list = g_slist_append (rule_list, rule);
    }

    g_strfreev (rules);

done:
    *out_title = g_strdup (split[0]);
    *out_rules = rule_list;
    g_strfreev (split);

    return TRUE;
}

NProplist*
n_event_parse_properties (GKeyFile *keyfile, const char *group,
                          GHashTable *keytypes, GHashTable *defines)
{
    g_assert (keyfile != NULL);
    g_assert (group != NULL);
    g_assert (keytypes != NULL);

    NProplist  *proplist = NULL;
    NProplist  *includes = NULL;
    gchar     **key_list = NULL;
    gchar     **key      = NULL;
    gchar      *value    = NULL;
    gboolean    bvalue   = FALSE;
    gint        ivalue   = 0;
    int         key_type = 0;

    proplist = n_proplist_new ();

    key_list = g_key_file_get_keys (keyfile, group, NULL, NULL);

    /* first pass for getting includes in */
    for (key = key_list; *key; ++key) {
        if (g_str_has_prefix (*key, N_EVENT_GROUP_ENTRY_INCLUDE)) {
            value = g_key_file_get_string (keyfile, group, *key, NULL);
            if (defines && (includes = g_hash_table_lookup (defines, value)))
                n_proplist_merge (proplist, includes);
            else
                N_WARNING (LOG_CAT "tried to include unknown define '%s'", value);
            g_free (value);
            /* mark the key as empty to prevent leaking include keys
             * to event proplist. */
            **key = '\0';
        }
    }

    /* second pass for getting event values, also override possible
     * values coming from includes. */
    for (key = key_list; *key; ++key) {
        if (!**key)
            continue;

        key_type = GPOINTER_TO_INT(g_hash_table_lookup (keytypes, *key));

        switch (key_type) {
            case N_VALUE_TYPE_INT:
                ivalue = g_key_file_get_integer (keyfile, group, *key, NULL);
                n_proplist_set_int (proplist, *key, ivalue);
                break;
            case N_VALUE_TYPE_BOOL:
                bvalue = g_key_file_get_boolean (keyfile, group, *key, NULL);
                n_proplist_set_bool (proplist, *key, bvalue);
                break;
            default:
                value = g_key_file_get_string (keyfile, group, *key, NULL);
                n_proplist_set_string (proplist, *key, value);
                g_free (value);
                break;
        }
    }
    g_strfreev (key_list);

    return proplist;
}

/* Sort rules so that context rules are evaluated first,
 * as the context rule values are most likely already cached. */
static gint
sort_rules_cb (gconstpointer a, gconstpointer b)
{
    const NEventRule *rule_a = a;
    const NEventRule *rule_b = b;

    if (rule_a->target == rule_b->target)
        return 0;
    if (rule_a->target == N_EVENT_RULE_CONTEXT)
        return -1;

    return 1;
}

NEvent*
n_event_new_from_group (GSList **rule_list, GKeyFile *keyfile, const char *group,
                        GHashTable *keytypes, GHashTable *defines)
{
    g_assert (keyfile);
    g_assert (group);
    g_assert (keytypes);
    g_assert (rule_list);

    NEvent    *event = NULL;
    NProplist *props = NULL;
    GSList    *rules = NULL;
    gchar     *title = NULL;
    int        priority = 0;

    if (g_str_has_prefix (group, N_EVENT_GROUP_ENTRY_DEFINE))
        return NULL;

    /* parse the group title and related rules. */

    if (!event_parse_group_title (group, &title, &priority, &rules))
        return NULL;

    merge_rules (rule_list, rules);

    /* convert the group content entries to property list. */

    props = n_event_parse_properties (keyfile, group, keytypes, defines);

    /* create a new event based on the parsed data. */

    event             = n_event_new ();
    event->name       = title;
    event->rules      = g_slist_sort (rules, sort_rules_cb);
    event->properties = props;
    event->priority   = priority;

    return event;
}

const char*
n_event_get_name (NEvent *event)
{
    return event->name;
}

const NProplist*
n_event_get_properties (NEvent *event)
{
    return (event != NULL) ? event->properties : NULL;
}
