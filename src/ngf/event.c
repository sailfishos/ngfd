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
#include <glib.h>
#include <ngf/log.h>
#include "event-internal.h"

#define LOG_CAT "event: "

#define GROUP_ENTRY_DEFINE  "%define "
#define GROUP_ENTRY_INCLUDE "%include"

static int        n_event_parse_rule        (const char *rule, NProplist *proplist);
static int        n_event_parse_group_title (const char *value, gchar **out_title,
                                             NProplist **out_rules);
static NProplist* n_event_parse_properties  (GKeyFile *keyfile, const char *group,
                                             GHashTable *key_types, GHashTable *defines);
static NEvent*    n_event_parse_group       (GKeyFile *keyfile, const char *group,
                                             GHashTable *key_types, GHashTable *defines);

static const char* strip_prefix             (const char *group, const char *prefix);


NEvent*
n_event_new ()
{
    return g_new0 (NEvent, 1);
}

NEvent*
n_event_new_from_group (NCore *core, GKeyFile *keyfile,
                        const char *group, GHashTable *defines)
{
    g_assert (keyfile != NULL);
    g_assert (group != NULL);

    return n_event_parse_group (keyfile, group, core->key_types, defines);
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

void
n_event_parse_defines (NCore *core, GKeyFile *keyfile,
                       const char *group, GHashTable **defines)
{
    NProplist  *proplist = NULL;
    const char *name;
    char *key;

    g_assert (core);
    g_assert (keyfile);
    g_assert (group);
    g_assert (defines);

    if ((name = strip_prefix (group, GROUP_ENTRY_DEFINE))) {
        if (!*defines)
            *defines = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free, (GDestroyNotify) n_proplist_free);

        proplist = n_event_parse_properties (keyfile, group, core->key_types, NULL);
        key = g_strdup (name);
        g_strstrip (key);
        g_hash_table_insert (*defines, key, proplist);
    }
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

static int
n_event_parse_rule (const char *rule, NProplist *proplist)
{
    g_assert (rule != NULL);
    g_assert (proplist != NULL);

    gchar **items = NULL;

    items = g_strsplit (rule, "=", 2);
    if (items[1] == NULL) {
        g_strfreev (items);
        return FALSE;
    }

    g_strstrip (items[0]);
    g_strstrip (items[1]);
    n_proplist_set_string (proplist, items[0], items[1]);
    g_strfreev (items);

    return TRUE;
}

static int
n_event_parse_group_title (const char *value, gchar **out_title,
                           NProplist **out_rules)
{
    g_assert (value != NULL);
    g_assert (*out_title == NULL);
    g_assert (*out_rules == NULL);

    NProplist  *rule_list = NULL;
    gchar     **split     = NULL;
    gchar     **rules     = NULL;
    gchar     **iter      = NULL;

    rule_list = n_proplist_new ();

    /* split the value by =>, which as a title and rule separator. first
       item contains the title, second the unparsed rule string. */

    split = g_strsplit (value, "=>", 2);
    g_strstrip (split[0]);

    /* if there are no rules, then we are done. */

    if (split[1] == NULL)
        goto done;

    /* split the rules by ",", strip each rule and make a new entry
       to the rule property list. */

    rules = g_strsplit (split[1], ",", -1);
    for (iter = rules; *iter; ++iter) {
        g_strstrip (*iter);
        n_event_parse_rule (*iter, rule_list);
    }

    g_strfreev (rules);

done:
    *out_title = g_strdup (split[0]);
    *out_rules = rule_list;
    g_strfreev (split);

    return TRUE;
}

static NProplist*
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
        if (g_str_has_prefix (*key, GROUP_ENTRY_INCLUDE)) {
            value = g_key_file_get_string (keyfile, group, *key, NULL);
            if (defines && (includes = g_hash_table_lookup (defines, value)))
                n_proplist_merge (proplist, includes);
            else
                N_WARNING (LOG_CAT "tried to include unknown define '%s'", value);
            g_free (value);
        }
    }

    /* second pass for getting event values, also override possible
     * values coming from includes. */
    for (key = key_list; *key; ++key) {
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

static NEvent*
n_event_parse_group (GKeyFile *keyfile, const char *group,
                     GHashTable *keytypes, GHashTable *defines)
{
    g_assert (keyfile != NULL);
    g_assert (group != NULL);
    g_assert (keytypes != NULL);

    NEvent    *event = NULL;
    NProplist *props = NULL;
    NProplist *rules = NULL;
    gchar     *title = NULL;

    /* parse the group title and related rules. */
    if (g_str_has_prefix (group, GROUP_ENTRY_DEFINE))
        return NULL;

    if (!n_event_parse_group_title (group, &title, &rules))
        return NULL;

    /* convert the group content entries to property list. */

    props = n_event_parse_properties (keyfile, group, keytypes, defines);

    /* create a new event based on the parsed data. */

    event = n_event_new ();
    event->name       = title;
    event->rules      = rules;
    event->properties = props;

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


