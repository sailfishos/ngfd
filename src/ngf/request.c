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

#include "request-internal.h"

static guint id_counter = 0;

NRequest*
n_request_new ()
{
    NRequest *request = NULL;

    request = g_slice_new0 (NRequest);
    /* skip 0 */
    request->id = ++id_counter ? id_counter : ++id_counter;
    return request;
}

NRequest*
n_request_copy (const NRequest *request)
{
    NRequest *copy;

    copy                = g_slice_new0 (NRequest);
    copy->id            = request->id;
    copy->name          = request->name ? g_strdup (request->name) : NULL;
    copy->input_iface   = request->input_iface;
    if (request->original_properties)
        copy->properties = n_proplist_copy (request->original_properties);
    else if (request->properties)
        copy->properties = n_proplist_copy (request->properties);

    return copy;
}

NRequest*
n_request_new_with_event (const char *event)
{
    if (!event)
        return NULL;

    NRequest *request = n_request_new ();
    request->name = g_strdup (event);
    return request;
}

NRequest*
n_request_new_with_event_and_properties (const char *event, const NProplist *properties)
{
    if (!event)
        return NULL;

    NRequest *request   = n_request_new ();
    request->name       = g_strdup (event);
    request->properties = n_proplist_copy ((NProplist*) properties);

    return request;
}

void
n_request_free (NRequest *request)
{
    if (request->properties) {
        n_proplist_free (request->properties);
        request->properties = NULL;
    }

    if (request->original_properties) {
        n_proplist_free (request->original_properties);
        request->original_properties = NULL;
    }

    g_free (request->name);
    request->name = NULL;

    g_slice_free (NRequest, request);
}

unsigned int
n_request_get_id (NRequest *request)
{
    return (request != NULL) ? request->id : 0;
}

const char*
n_request_get_name (NRequest *request)
{
    return (request != NULL) ? (const char*) request->name : NULL;
}

void
n_request_set_properties (NRequest *request, NProplist *properties)
{
    if (!request || !properties)
        return;

    n_proplist_free (request->properties);
    request->properties = n_proplist_copy (properties);
}

const NProplist*
n_request_get_properties (NRequest *request)
{
    return (request != NULL) ? (const NProplist*) request->properties : NULL;
}

void
n_request_store_data (NRequest *request, const char *key, void *data)
{
    if (!request || !key)
        return;

    n_proplist_set_pointer (request->properties, key, data);
}

void*
n_request_get_data (NRequest *request, const char *key)
{
    if (!request || !key)
        return NULL;

    return n_proplist_get_pointer (request->properties, key);
}

int
n_request_is_paused (NRequest *request)
{
    if (!request)
        return FALSE;

    return request->is_paused;
}

int
n_request_is_fallback (NRequest *request)
{
    if (!request)
        return FALSE;

    return request->is_fallback;
}

const NEvent*
n_request_get_event (NRequest *request)
{
    return (request != NULL) ? (const NEvent*) request->event : NULL;
}

void
n_request_set_timeout (NRequest *request, guint timeout)
{
    if (!request)
        return;

    request->timeout_ms = timeout;
}

guint
n_request_get_timeout (NRequest *request)
{
    return (request != NULL) ? request->timeout_ms : 0;
}


