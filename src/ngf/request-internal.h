/*
 * ngfd - Non-graphic feedback daemon
 *
 * Copyright (C) 2010 Nokia Corporation.
 * Contact: Xun Chen <xun.chen@nokia.com>
 * Copyright (c) 2025 Jolla Mobile Ltd
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

#ifndef N_REQUEST_INTERNAL_H
#define N_REQUEST_INTERNAL_H

#include <ngf/request.h>
#include <ngf/proplist.h>

#include "core-internal.h"
#include "event-internal.h"
#include "inputinterface-internal.h"

/* typedef struct _NRequest NRequest; */

struct _NRequest
{
    gchar           *name;          /* request name */
    NProplist       *properties;    /* request custom properties */
    NProplist       *original_properties;

    guint            id;                    /* unique request identifier */
    NEvent          *event;                 /* borrowed reference */
    NCore           *core;                  /* borrowed reference */
    NInputInterface *input_iface;           /* borrowed reference */

    gboolean         is_paused;
    gboolean         is_fallback;
    gboolean         has_failed;
    gboolean         no_event;

    guint            play_source_id;        /* source id for play */
    guint            stop_source_id;        /* source id for stop */

    GList           *all_sinks;             /* all sinks available for the request */
    GList           *sinks_preparing;       /* sinks not yet synchronized and still preparing */
    GList           *sinks_prepared;
    GList           *sinks_playing;         /* sinks currently playing */
    GList           *sinks_resync;
    GList           *stop_list;
    NSinkInterface  *master_sink;           /* borrowed reference */

    guint            max_timeout_id;
    guint            timeout_ms;
};

NRequest* n_request_new          ();
void      n_request_free         (NRequest *request);
int       n_request_has_fallback (NRequest *request);

#endif /* N_REQUEST_INTERNAL_H */
