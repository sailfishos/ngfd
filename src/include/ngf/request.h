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

#ifndef N_REQUEST_H
#define N_REQUEST_H

/** Internal request structure. */
typedef struct _NRequest NRequest;

#include <ngf/proplist.h>
#include <ngf/event.h>

/** Create empty request
 * @return Allocated request structure
 */
NRequest*        n_request_new            ();

/** Copy request
 * @return Allocated request structure
 */
NRequest*        n_request_copy           (const NRequest *request);

/** Create request with event
 * @param event Event to be added to request
 * @return Allocated request structure
 */
NRequest*        n_request_new_with_event (const char *event);

/** Free request
 * @param request Request
 */
void             n_request_free           (NRequest *request);

/** Get request id
 * @param request Request
 * @return Id assigned to request
 */
unsigned int     n_request_get_id         (NRequest *request);

/** Get request name
 * @param request Request
 * @return Name of the request
 */
const char*      n_request_get_name       (NRequest *request);

/** Set properties to request
 * @param request Request
 * @param properties Properties as NProplist
 */
void             n_request_set_properties (NRequest *request, NProplist *properties);

/** Get properties from request
 * @param request Request
 * @return Properties as NProplist
 */
const NProplist* n_request_get_properties (NRequest *request);

/** Get associated event from request
 * @param request Request
 * @return Event as NEvent
 */
const NEvent*    n_request_get_event      (NRequest *request);

/** Set the timeout value for the request
 * @param request Request
 * @param timeout Timeout value in milliseconds (ms), 0 to disable
 */

void             n_request_set_timeout    (NRequest *request, guint timeout);

/** Get a timeout value for the request
 * @param request Request
 * @return Timeout value in milliseconds (ms), or 0 if timeout disabled
 */

guint            n_request_get_timeout    (NRequest *request);

/** Store key/value pair to request
 * @param request Request
 * @param key Key
 * @param data Pointer to data to be stored
 */
void             n_request_store_data     (NRequest *request, const char *key, void *data);

/** Get data stored to request by key
 * @param request Request
 * @param key Key
 * @return Pointer to data or NULL if key is not found
 */
void*            n_request_get_data       (NRequest *request, const char *key);

/** Check if the request is paused
 * @param request Request
 * @return TRUE if request is currently paused
 */
int              n_request_is_paused      (NRequest *request);

/** Create new request with event and properties
 * @param event Event
 * @param properties Properties as NProplist
 * @return Newly allocated request
 */
NRequest*        n_request_new_with_event_and_properties (const char *event, const NProplist *properties);

/** Request is a fallback request
 * @param request Request
 * @return TRUE if fallback, FALSE if normal.
 */
int              n_request_is_fallback    (NRequest *request);

#endif /* N_REQUEST_H */
