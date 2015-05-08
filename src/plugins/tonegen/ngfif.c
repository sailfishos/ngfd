/*************************************************************************
This file is part of ngfd

Copyright (C) 2015 Jolla Ltd.

This library is free software; you can redistribute
it and/or modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation
version 2.1 of the License.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
USA.
*************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <ngf/request.h>
#include <ngf/proplist.h>
#include <ngf/log.h>

#include "tonegend.h"
#include "ngfif.h"

#define LOG_CAT "tonegen-ngfif: "

struct event_handler {
    char *name;
    event_handler_method method_start_cb;
    event_handler_method method_stop_cb;
};

static void destroy_handler(gpointer p)
{
    struct event_handler *handler = (struct event_handler *) p;

    g_free(handler->name);
    g_free(handler);
}

struct ngfif *ngfif_create(struct tonegend *t)
{
    struct ngfif *ngfif = t->ngfd_ctx;

    ngfif = g_new0(struct ngfif, 1);
    ngfif->tonegend = t;
    ngfif->hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                        NULL, destroy_handler);

    return ngfif;
}

void ngfif_destroy(struct ngfif *t)
{
    if (t) {
        if (t->hash)
            g_hash_table_destroy(t->hash);

        g_free(t);
    }
}

int ngfif_register_input_method(struct tonegend *t, const char *name,
                                 event_handler_method method_start_cb,
                                 event_handler_method method_stop_cb)
{
    struct event_handler *handler = g_new0(struct event_handler, 1);

    handler->name = g_strdup(name);
    handler->method_start_cb = method_start_cb;
    handler->method_stop_cb = method_stop_cb;

    g_hash_table_insert(t->ngfd_ctx->hash, handler->name, handler);

    return 0;
}

static const char *get_type(NRequest *request) {
    return n_proplist_get_string(n_request_get_properties(request), "tonegen.type");
}

static struct event_handler *get_handler(struct tonegend *t, const char *name)
{
    return g_hash_table_lookup(t->ngfd_ctx->hash, name);
}

int ngfif_handle_start_request(struct tonegend *t, NRequest *request)
{
    struct event_handler *handler;

    if ((handler = get_handler(t, get_type(request))))
        return handler->method_start_cb(request, t);
    else
        return FALSE;
}

int ngfif_handle_stop_request(struct tonegend *t, NRequest *request)
{
    struct event_handler *handler;

    if (!(handler = get_handler(t, get_type(request))))
        return FALSE;

    return handler->method_stop_cb ? handler->method_stop_cb(request, t) : 0;
}

int ngfif_can_handle_request(struct tonegend *t, NRequest *request)
{
    const char *event_type;

    if ((event_type = get_type(request))) {
        if (get_handler(t, event_type))
            return TRUE;
    }

    return FALSE;
}
