/*
 * ngfd - Non-graphic feedback daemon
 *
 * Copyright (C) 2010 Nokia Corporation.
 *               2018 Jolla Ltd
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

#ifndef N_EVENT_INTERNAL_H
#define N_EVENT_INTERNAL_H

#include <ngf/event.h>

#include <ngf/request.h>
#include <ngf/proplist.h>
#include "core-internal.h"

#define N_EVENT_GROUP_ENTRY_DEFINE  "%define "
#define N_EVENT_GROUP_ENTRY_INCLUDE "%include"

struct _NEvent
{
    gchar      *name;               /* event name */
    NProplist  *properties;         /* properties */
    GSList     *rules;
    int         priority;           /* higher value higher priority */
};

NEvent*     n_event_new              ();
void        n_event_free             (NEvent *event);

NEvent*     n_event_new_from_group   (GSList **rule_list, GKeyFile *keyfile,
                                      const char *group, GHashTable *keytypes, GHashTable *defines);
NProplist*  n_event_parse_properties (GKeyFile *keyfile, const char *group,
                                      GHashTable *key_types, GHashTable *defines);

void        n_event_rules_dump       (NEvent *event, const char *debug_prefix);
guint       n_event_rules_size       (const NEvent *event);
int         n_event_rules_equal      (NEvent *a, NEvent *b);

#endif
