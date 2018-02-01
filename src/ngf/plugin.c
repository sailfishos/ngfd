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

#include <ngf/log.h>
#include <ngf/proplist.h>
#include "plugin-internal.h"

#define LOG_CAT "plugin: "

NPlugin*
n_plugin_open (const char *filename)
{
    g_assert (filename != NULL);

    NPlugin *plugin = NULL;
    plugin = g_new0 (NPlugin, 1);
    plugin->module = g_module_open (filename,
        G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

    if (!plugin->module)
        goto fail_load;

#define LOAD_SYMBOL(p_mod,p_name,p_store)                           \
{                                                                   \
    if (!g_module_symbol (p_mod, p_name, (gpointer*) &p_store)) {   \
        N_WARNING (LOG_CAT "missing symbol in %s: %s",              \
            g_module_name (p_mod), p_name);                         \
        goto fail_load;                                             \
    }                                                               \
}

    LOAD_SYMBOL (plugin->module, "n_plugin__get_name"   , plugin->get_name);
    LOAD_SYMBOL (plugin->module, "n_plugin__get_desc"   , plugin->get_desc);
    LOAD_SYMBOL (plugin->module, "n_plugin__get_version", plugin->get_version);
    LOAD_SYMBOL (plugin->module, "n_plugin__load"       , plugin->load);
    LOAD_SYMBOL (plugin->module, "n_plugin__unload"     , plugin->unload);

#undef LOAD_SYMBOL

    return plugin;

fail_load:
    n_plugin_unload (plugin);
    return NULL;
}

int
n_plugin_init (NPlugin *plugin)
{
    g_assert (plugin != NULL);

    return plugin->load (plugin);
}

void
n_plugin_unload (NPlugin *plugin)
{
    g_assert (plugin != NULL);

    if (plugin->module) {
        g_module_close (plugin->module);
        plugin->module = NULL;
    }

    if (plugin->params) {
        n_proplist_free (plugin->params);
        plugin->params = NULL;
    }

    g_free (plugin);
}

NCore*
n_plugin_get_core (NPlugin *plugin)
{
    if (!plugin)
        return NULL;

    return plugin->core;
}

const NProplist*
n_plugin_get_params (NPlugin *plugin)
{
    if (!plugin)
        return NULL;

    return plugin->params;
}

void
n_plugin_set_userdata (NPlugin *plugin, gpointer userdata)
{
    g_assert (plugin);

    plugin->userdata = userdata;
}

gpointer
n_plugin_get_userdata (NPlugin *plugin)
{
    g_assert (plugin);

    return plugin->userdata;
}

void
n_plugin_register_sink (NPlugin *plugin, const NSinkInterfaceDecl *decl)
{
    if (!plugin || !decl)
        return;

    n_core_register_sink (plugin->core, decl);
}

void
n_plugin_register_input (NPlugin *plugin, const NInputInterfaceDecl *decl)
{
    if (!plugin || !decl)
        return;

    n_core_register_input (plugin->core, decl);
}

