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

#ifndef N_EVENT_INTERNAL_H
#define N_EVENT_INTERNAL_H

/* typedef struct _NEvent NEvent; */

#include <ngf/event.h>

#include <ngf/proplist.h>
#include "core-internal.h"

struct _NEvent
{
    gchar      *name;               /* event name */
    NProplist  *properties;         /* properties */
    NProplist  *rules;
};

NEvent* n_event_new            ();
void    n_event_parse_defines  (NCore *core, GKeyFile *keyfile,
                                const char *group, GHashTable **defines);
NEvent* n_event_new_from_group (NCore *core, GKeyFile *keyfile,
                                const char *group, GHashTable *defines);
void    n_event_free           (NEvent *event);

#endif /* N_EVENT_INTERNAL_H */
