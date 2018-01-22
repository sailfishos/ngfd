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

#ifndef N_EVENT_LIST_INTERNAL_H
#define N_EVENT_LIST_INTERNAL_H

#include <ngf/event.h>

#include <ngf/proplist.h>
#include "core-internal.h"

typedef struct _NEventList
{
    NCore      *core;
    GHashTable *event_table;
    GList      *event_list;
    GSList     *rule_list;
} NEventList;

NEventList* n_event_list_new            (NCore *core);
void        n_event_list_free           (NEventList *eventlist);
gboolean    n_event_list_parse_keyfile  (NEventList *eventlist, GKeyFile *keyfile);
GList*      n_event_list_get_events     (NEventList *eventlist);
guint       n_event_list_size           (const NEventList *eventlist);

NEvent*     n_event_list_match_request  (NEventList *eventlist, NRequest *request);

#endif
