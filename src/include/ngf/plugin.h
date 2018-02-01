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

#ifndef N_PLUGIN_H
#define N_PLUGIN_H

/** Internal plugin structure. */
typedef struct _NPlugin NPlugin;

#include <ngf/log.h>
#include <ngf/proplist.h>
#include <ngf/core.h>
#include <ngf/sinkinterface.h>
#include <ngf/inputinterface.h>

/** Get core to which plugin is associated to
 * @param plugin NPlugin structure
 * @return NCore structure
 */
NCore*           n_plugin_get_core       (NPlugin *plugin);

/** Get parameters stored in plugin settings file
 * @param plugin NPlugin structure
 * @return Parameters as NProplist structure
 */
const NProplist* n_plugin_get_params     (NPlugin *plugin);

/** Set userdata for plugin. This should be called only once per plugin init.
 * @param plugin NPlugin structure
 * @param userdata Plugin userdata
 */
void             n_plugin_set_userdata   (NPlugin *plugin, gpointer userdata);

/** Get userdata for plugin.
 * @param plugin NPlugin structure
 * @return Userdata pointer stored for the plugin
 */
gpointer         n_plugin_get_userdata   (NPlugin *plugin);

/** Register sink type plugin
 * @param plugin NPlugin structure
 * @param decl Plugin declaration in NSinkInterfaceDecl structure
 */
void             n_plugin_register_sink  (NPlugin *plugin, const NSinkInterfaceDecl *decl);

/** Register input type plugin
 * @param plugin NPlugin structure
 * @param decl Plugin declaration in NInputInterfaceDecl structure
 */
void             n_plugin_register_input (NPlugin *plugin, const NInputInterfaceDecl *decl);

/** Macro to define plugin name */
#define N_PLUGIN_NAME(p_name)                   \
    const char* n_plugin__get_name () {         \
        return p_name;                          \
    }

/** Macro to define plugin description */
#define N_PLUGIN_DESCRIPTION(p_desc)            \
    const char* n_plugin__get_desc () {         \
        return p_desc;                          \
    }

/** Macro to define plugin version */
#define N_PLUGIN_VERSION(p_version)             \
    const char* n_plugin__get_version () {      \
        return p_version;                       \
    }

/** Plugin loading function. Plugin declaration structure should be initialized here. */
#define N_PLUGIN_LOAD(p_plugin)                 \
    int n_plugin__load (NPlugin* p_plugin)

/** Plugin unload function. Plugin memory releasing should be done here. */
#define N_PLUGIN_UNLOAD(p_plugin)               \
    void n_plugin__unload (NPlugin* p_plugin)

#endif /* N_PLUGIN_H */
