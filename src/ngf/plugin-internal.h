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

#ifndef N_PLUGIN_INTERNAL_H
#define N_PLUGIN_INTERNAL_H

#include <glib.h>
#include <gmodule.h>

#include <ngf/plugin.h>
#include <ngf/proplist.h>
#include "core-internal.h"

/* typedef struct _NPlugin NPlugin; */

struct _NPlugin
{
    NCore       *core;              /* core functionality */
    GModule     *module;            /* plugin module handle */
    gpointer     userdata;          /* plugin implementor internal data */
    NProplist   *params;            /* plugin parameters */

    const char* (*get_name)    ();
    const char* (*get_desc)    ();
    const char* (*get_version) ();
    int         (*load)        (NPlugin *plugin);
    void        (*unload)      (NPlugin *plugin);
};

NPlugin* n_plugin_open   (const char *plugin_name);
int      n_plugin_init   (NPlugin *plugin);
void     n_plugin_unload (NPlugin *plugin);

#endif /* N_PLUGIN_INTERNAL_H */
