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

#include "sinkinterface-internal.h"
#include "core-internal.h"

NCore*
n_sink_interface_get_core (NSinkInterface *iface)
{
    return (iface != NULL) ? iface->core : NULL;
}

const char*
n_sink_interface_get_name (NSinkInterface *iface)
{
    return (iface != NULL) ? (const char*) iface->name : NULL;
}

void
n_sink_interface_set_resync_on_master (NSinkInterface *iface, NRequest *request)
{
    if (!iface || !request)
        return;

    n_core_set_resync_on_master (iface->core, iface, request);
}

void
n_sink_interface_resynchronize (NSinkInterface *iface, NRequest *request)
{
    if (!iface || !request)
        return;

    n_core_resynchronize_sinks (iface->core, iface, request);
}

void
n_sink_interface_synchronize (NSinkInterface *iface, NRequest *request)
{
    if (!iface || !request)
        return;

    n_core_synchronize_sink (iface->core, iface, request);
}

void
n_sink_interface_complete (NSinkInterface *iface, NRequest *request)
{
    if (!iface || !request)
        return;

    n_core_complete_sink (iface->core, iface, request);
}

void
n_sink_interface_fail (NSinkInterface *iface, NRequest *request)
{
    if (!iface || !request)
        return;

    n_core_fail_sink (iface->core, iface, request);
}

void
n_sink_interface_set_userdata (NSinkInterface *iface, void *userdata)
{
    g_assert (iface);

    iface->userdata = userdata;
}

void*
n_sink_interface_get_userdata (NSinkInterface *iface)
{
    g_assert (iface);

    return iface->userdata;
}
