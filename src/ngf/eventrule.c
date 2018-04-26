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
#include <ngf/value.h>
#include "eventrule-internal.h"

#define LOG_CAT "event-rule: "

gboolean
n_parse_number (const char *str, gint64 *value)
{
    gchar *endptr = NULL;

    g_assert (str);
    g_assert (value);

    if (strlen (str) == 0)
        return FALSE;

    *value = g_ascii_strtoll (str, &endptr, 10);

    if (str == endptr)
        return FALSE;

    return TRUE;
}

static gboolean
parse_boolean (const char *str, gboolean *value)
{
    g_assert (str);
    g_assert (value);

    if (g_strcmp0 (str, "true") == 0 ||
        g_strcmp0 (str, "True") == 0 ||
        g_strcmp0 (str, "TRUE") == 0 ||
        g_strcmp0 (str, "1")    == 0) {
        *value = TRUE;
        return TRUE;
    } else if (g_strcmp0 (str, "false") == 0 ||
               g_strcmp0 (str, "False") == 0 ||
               g_strcmp0 (str, "FALSE") == 0 ||
               g_strcmp0 (str, "0")     == 0) {
        *value = FALSE;
        return TRUE;
    }

    return FALSE;
}

NEventRule*
n_event_rule_parse (const char *rule_str)
{
    NEventRule         *rule;
    char               *key;
    char               *value_str;
    gint                value_int;
    guint               value_uint;
    gboolean            value_bool;
    gint64              value_gint64;
    NValue             *value = NULL;
    NEventRuleOp        op;
    NEventRuleTarget    target;
    gchar             **items = NULL;

    g_assert (rule_str);

    if (strstr (rule_str, "==")) {
        op = N_EVENT_RULE_EQUALS;
        items = g_strsplit (rule_str, "==", 2);
    } else if (strstr (rule_str, "!=")) {
        op = N_EVENT_RULE_NEQUALS;
        items = g_strsplit (rule_str, "!=", 2);
    } else if (strstr (rule_str, ">=")) {
        op = N_EVENT_RULE_GREATER_OR_EQUAL;
        items = g_strsplit (rule_str, ">=", 2);
    } else if (strstr (rule_str, "<=")) {
        op = N_EVENT_RULE_LESS_OR_EQUAL;
        items = g_strsplit (rule_str, "<=", 2);
    } else if (strstr (rule_str, ">")) {
        op = N_EVENT_RULE_GREATER;
        items = g_strsplit (rule_str, ">", 2);
    } else if (strstr (rule_str, "<")) {
        op = N_EVENT_RULE_LESS;
        items = g_strsplit (rule_str, "<", 2);
    } else if (strstr (rule_str, "=")) {
        /* support for legacy rule formatting */
        op = N_EVENT_RULE_EQUALS;
        items = g_strsplit (rule_str, "=", 2);
    } else
        goto bad_rule;

    if (items[1] == NULL)
        goto bad_rule;

    g_strstrip (items[0]);
    g_strstrip (items[1]);

    key       = items[0];
    value_str = items[1];

    if (g_str_has_prefix (key, N_EVENT_RULE_CONTEXT_PREFIX)) {
        target = N_EVENT_RULE_CONTEXT;
        key = key + strlen (N_EVENT_RULE_CONTEXT_PREFIX);
        if (strlen (key) == 0)
            goto bad_rule;
    } else
        target = N_EVENT_RULE_REQUEST;

    value = n_value_new ();

    if (g_str_has_prefix (value_str, N_VALUE_STR_INT)) {
        value_str = value_str + strlen (N_VALUE_STR_INT);
        g_strstrip (value_str);
        if (n_parse_number (value_str, &value_gint64)) {
            if (value_gint64 > G_MAXINT) value_int = G_MAXINT;
            else if (value_gint64 < G_MININT) value_int = G_MININT;
            else value_int = (gint) value_gint64;
        } else
            goto bad_rule;
        n_value_set_int (value, value_int);
    } else if (g_str_has_prefix (value_str, N_VALUE_STR_UINT)) {
        value_str = value_str + strlen (N_VALUE_STR_UINT);
        g_strstrip (value_str);
        if (n_parse_number (value_str, &value_gint64)) {
            if (value_gint64 > G_MAXUINT) value_uint = G_MAXUINT;
            else if (value_gint64 < 0) value_uint = 0;
            else value_uint = (guint) value_gint64;
        } else
            goto bad_rule;
        n_value_set_uint (value, value_uint);
    } else if (g_str_has_prefix (value_str, N_VALUE_STR_BOOL)) {
        value_str = value_str + strlen (N_VALUE_STR_BOOL);
        g_strstrip (value_str);
        if (!parse_boolean (value_str, &value_bool))
            goto bad_rule;
        n_value_set_bool (value, value_bool);
    } else {
        n_value_set_string (value, value_str);
        if (g_strcmp0 (n_value_get_string (value), "*") == 0)
            op = N_EVENT_RULE_ALWAYS;
    }


    rule            = g_new0 (NEventRule, 1);
    rule->ref       = 1;
    rule->key       = g_strdup (key);
    rule->value     = value;
    rule->op        = op;
    rule->target    = target;
    rule->cache     = N_EVENT_RULE_CACHE_INACTIVE;

    g_strfreev (items);

    return rule;

bad_rule:
    if (value)
        n_value_free (value);
    if (items)
        g_strfreev (items);
    N_WARNING (LOG_CAT "bad event rule '%s', ignoring.", rule_str);

    return NULL;
}

NEventRule*
n_event_rule_ref (NEventRule *rule)
{
    g_assert (rule);
    g_assert (rule->ref > 0);

    rule->ref ++;
    return rule;
}

static void
event_rule_free (NEventRule *rule)
{
    g_assert (rule);
    n_value_free (rule->value);
    g_free (rule->key);
    g_free (rule);
}

void
n_event_rule_unref (NEventRule *rule)
{
    g_assert (rule);
    g_assert (rule->ref > 0);

    rule->ref --;
    if (rule->ref == 0)
        event_rule_free (rule);
}

gboolean
n_event_rule_equal (const NEventRule *a, const NEventRule *b)
{
    g_assert (a);
    g_assert (b);

    return (g_strcmp0 (a->key, b->key) == 0 &&
            a->op == b->op                  &&
            n_value_equals (a->value, b->value));
}

void
n_event_rule_dump (const NEventRule *rule, const char *debug_prefix)
{
    char *value_str = NULL;

    g_assert (rule);

    if (n_log_get_level() <= N_LOG_LEVEL_DEBUG) {
        if (rule->op != N_EVENT_RULE_ALWAYS)
            value_str = n_value_to_string (rule->value);
        N_DEBUG ("%s+ %s'%s' %s '%s'", debug_prefix ? debug_prefix : LOG_CAT,
                 rule->target == N_EVENT_RULE_CONTEXT ? N_EVENT_RULE_CONTEXT_PREFIX : "",
                 rule->key, n_event_rule_op_string (rule),
                 value_str ? value_str : "*");
        g_free (value_str);
    }
}

#define MATCH_VALUES(match, value1, op, value2) \
    switch (n_value_type (value1)) {                                                                    \
        case N_VALUE_TYPE_INT:  match = n_value_get_int (value1) op n_value_get_int (value2);   break;  \
        case N_VALUE_TYPE_UINT: match = n_value_get_uint (value1) op n_value_get_uint (value2); break;  \
        default:                match = FALSE;                                                          \
    };

gboolean
n_event_rule_match (const NEventRule *rule, const NValue *match_value)
{
    g_assert (rule);

    gboolean match  = TRUE;

    if (match_value && rule->op == N_EVENT_RULE_ALWAYS)
        goto done;

    if (!match_value) {
        match = FALSE;
        goto done;
    }

    if (g_strcmp0 (n_value_get_string (match_value), "*") == 0)
        goto done;

    if (n_value_type (match_value) != n_value_type (rule->value)) {
        match = FALSE;
        goto done;
    }

    switch (rule->op) {
        case N_EVENT_RULE_ALWAYS:
            break;

        case N_EVENT_RULE_NEQUALS:
            match = !n_value_equals (match_value, rule->value); break;

        case N_EVENT_RULE_EQUALS:
            match = n_value_equals (match_value, rule->value);  break;

        case N_EVENT_RULE_LESS:
            MATCH_VALUES (match, match_value, <, rule->value);  break;

        case N_EVENT_RULE_GREATER:
            MATCH_VALUES (match, match_value, >, rule->value);  break;

        case N_EVENT_RULE_LESS_OR_EQUAL:
            MATCH_VALUES (match, match_value, <=, rule->value); break;

        case N_EVENT_RULE_GREATER_OR_EQUAL:
            MATCH_VALUES (match, match_value, >=, rule->value); break;

        default:
            match = FALSE;
            break;
    };

done:
    return match;
}
#undef MATCH_VALUES

gboolean
n_event_rule_cached (const NEventRule *rule)
{
    g_assert (rule);

    return rule->target == N_EVENT_RULE_CONTEXT &&
           rule->cache > N_EVENT_RULE_CACHE_UNSET;
}

gboolean
n_event_rule_cached_value (const NEventRule *rule)
{
    g_assert (rule);

    return rule->cache == N_EVENT_RULE_CACHE_TRUE;
}

gboolean
n_event_rule_cached_value_set (NEventRule *rule, gboolean value)
{
    gboolean changed = FALSE;

    g_assert (rule);

    if (rule->target == N_EVENT_RULE_CONTEXT) {
        changed = (rule->cache == N_EVENT_RULE_CACHE_TRUE) != value;
        rule->cache = value ? N_EVENT_RULE_CACHE_TRUE : N_EVENT_RULE_CACHE_FALSE;
    }

    return changed;
}

const char*
n_event_rule_op_string (const NEventRule *rule)
{
    g_assert (rule);

    switch (rule->op) {
        case N_EVENT_RULE_ALWAYS:           /* fall through */
        case N_EVENT_RULE_EQUALS:           return "==";
        case N_EVENT_RULE_NEQUALS:          return "!=";
        case N_EVENT_RULE_LESS:             return "<";
        case N_EVENT_RULE_GREATER:          return ">";
        case N_EVENT_RULE_LESS_OR_EQUAL:    return "<=";
        case N_EVENT_RULE_GREATER_OR_EQUAL: return ">=";
    };

    return "<unknown>";
}
