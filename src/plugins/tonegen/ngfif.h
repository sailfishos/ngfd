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

#ifndef __TONEGEND_NGFIF_H__
#define __TONEGEND_NGFIF_H__

#include <glib.h>

#include <ngf/request.h>

struct tonegend;

struct ngfif {
    struct tonegend *tonegend;
    GHashTable *hash;
};

typedef int (*event_handler_method)(NRequest *request, struct tonegend *tonegend);

struct ngfif *ngfif_create(struct tonegend *t);
void ngfif_destroy(struct ngfif *t);
int ngfif_register_input_method(struct tonegend *t, const char *name,
                                 event_handler_method method_start_cb,
                                 event_handler_method method_stop_cb);

int ngfif_can_handle_request(struct tonegend *t, NRequest *request);
int ngfif_handle_start_request(struct tonegend *t, NRequest *request);
int ngfif_handle_stop_request(struct tonegend *t, NRequest *request);



#endif /* __TONEGEND_NGFIF_H__ */
