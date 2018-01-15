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

#ifndef N_SINK_INTERFACE_INTERNAL_H
#define N_SINK_INTERFACE_INTERNAL_H

#include <ngf/sinkinterface.h>

#include "core-internal.h"

/* typedef struct _NSinkInterface NSinkInterface; */

struct _NSinkInterface
{
    const char         *name;           /* sink interface name */
    const char         *type;           /* sink interface type */
    NSinkInterfaceDecl funcs;           /* functions for the interface */
    NCore              *core;
    void               *userdata;
    int                 priority;       /* priority */
};

#endif /* N_SINK_INTERFACE_INTERNAL_H */
