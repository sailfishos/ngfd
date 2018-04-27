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

#ifndef N_EVENT_RULE_INTERNAL_H
#define N_EVENT_RULE_INTERNAL_H

#include <ngf/value.h>

#define N_EVENT_RULE_CONTEXT_PREFIX "context@"

typedef enum _NEventRuleTarget
{
    N_EVENT_RULE_REQUEST,
    N_EVENT_RULE_CONTEXT
} NEventRuleTarget;

typedef enum _NEventRuleOp
{
    N_EVENT_RULE_ALWAYS,            /* for all types */
    N_EVENT_RULE_EQUALS,            /* for all types */
    N_EVENT_RULE_NEQUALS,           /* for all types */
    N_EVENT_RULE_GREATER,           /* only for numerical types */
    N_EVENT_RULE_LESS,              /* only for numerical types */
    N_EVENT_RULE_GREATER_OR_EQUAL,  /* only for numerical types */
    N_EVENT_RULE_LESS_OR_EQUAL      /* only for numerical types */
} NEventRuleOp;

typedef enum _NEventRuleCache
{
    N_EVENT_RULE_CACHE_INACTIVE,
    N_EVENT_RULE_CACHE_UNSET,
    N_EVENT_RULE_CACHE_TRUE,
    N_EVENT_RULE_CACHE_FALSE
} NEventRuleCache;

typedef struct _NEventRule
{
    int                 ref;
    NEventRuleTarget    target;
    char               *key;
    NValue             *value;
    NEventRuleOp        op;
    NEventRuleCache     cache;
} NEventRule;

NEventRule* n_event_rule_parse            (const char *rule_str);
NEventRule* n_event_rule_ref              (NEventRule *rule);
void        n_event_rule_unref            (NEventRule *rule);
gboolean    n_event_rule_equal            (const NEventRule *a, const NEventRule *b);
void        n_event_rule_dump             (const NEventRule *rule, const char *debug_prefix);
gboolean    n_event_rule_match            (const NEventRule *rule, const NValue *match_value);
gboolean    n_event_rule_cached           (const NEventRule *rule);
gboolean    n_event_rule_cached_value     (const NEventRule *rule);
gboolean    n_event_rule_cached_value_set (NEventRule *rule, gboolean value);
const char* n_event_rule_op_string        (const NEventRule *rule);

gboolean    n_parse_number                (const char *str, gint64 *value);

#endif
