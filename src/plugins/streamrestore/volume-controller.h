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

#ifndef VOLUME_CONTROLLER_H
#define VOLUME_CONTROLLER_H

// Called when volume entry with stream name changes.
// stream_name      name of the stream
// volume           new volume in range 0..100
// data             data passed to volume_controller_subscribe ()
// userdata         userdata passed to volume_controller_set_subscribe_cb ()
typedef void (*volume_controller_subscribe_cb) (const char *stream_name, int volume, void *data, void *userdata);

int  volume_controller_initialize ();
void volume_controller_shutdown   ();
int  volume_controller_update     (const char *role, int volume);
void volume_controller_subscribe  (const char *stream_name, void *data);
void volume_controller_unsubscribe(const char *stream_name);
void volume_controller_set_subscribe_cb (volume_controller_subscribe_cb cb, void *userdata);

#endif /* VOLUME_CONTROLLER_H */
