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

#include "inputinterface-internal.h"

NCore*
n_input_interface_get_core (NInputInterface *iface)
{
    if (!iface)
        return NULL;

    return iface->core;
}

int
n_input_interface_play_request (NInputInterface *iface, NRequest *request)
{
    if (!iface || !request)
        return FALSE;

    if (n_request_is_paused (request))
        return n_core_resume_request (iface->core, request);

    request->input_iface = iface;
    return n_core_play_request (iface->core, request);
}

int
n_input_interface_pause_request (NInputInterface *iface, NRequest *request)
{
    if (!iface || !request)
        return FALSE;

    return n_core_pause_request (iface->core, request);
}

void
n_input_interface_stop_request  (NInputInterface *iface, NRequest *request, guint timeout)
{
    if (!iface || !request)
        return;

    n_core_stop_request (iface->core, request, timeout);
}

void
n_input_interface_set_userdata (NInputInterface *iface, void *userdata)
{
    if (!iface)
        return;

    iface->userdata = userdata;
}

void*
n_input_interface_get_userdata (NInputInterface *iface)
{
    if (!iface)
        return NULL;

    return iface->userdata;
}
