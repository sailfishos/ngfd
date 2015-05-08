/*************************************************************************
This file is part of ngfd / tone-generator

Copyright (C) 2010 Nokia Corporation.
              2015 Jolla Ltd.

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

#ifndef __TONEGEND_RFC4733_H__
#define __TONEGEND_RFC4733_H__

struct tonegend;

/* Event values defined in
 * http://www.rfc-editor.org/rfc/rfc4733.txt
 * and
 * http://www.rfc-editor.org/rfc/rfc2833.txt
 */
typedef enum tone_event_ {
    EVENT_DIAL        = 66,
    EVENT_RING        = 70,
    EVENT_BUSY        = 72,
    EVENT_CONGEST     = 73,
    EVENT_ERROR       = 74,
    EVENT_WAIT        = 79,
    EVENT_RADIO_ACK   = 256,
    EVENT_RADIO_NA    = 257
} tone_event;

int rfc4733_init(void);
int rfc4733_create(struct tonegend *tonegend);
void rfc4733_destroy();

#endif /* __NOTFIFD_RFC4733_H__ */
