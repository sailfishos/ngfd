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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * USAGE:
 *
 * Resource plugin looks for media.* properties in request proplist.
 * 1) if no media.* is found in proplist, default for ENABLED for all
 * sinks.
 * 2) if media.foo is found in proplist, default for DISABLED for all
 * sinks, and enable only those specifically enabled in proplist.
 *
 * *) media.audio, media.vibra, media.leds, media.backlight
 *
 * After enabled/disabled classification is done, drop all disabled
 * sinks from request.
 *
 * resource.ini plugin configuration can be used to define which sinks
 * correspond which resource_key, ie. to define gst sink as media.audio
 * resource:
 *
 * [resource]
 * media.audio = gst
 *
 */

#include <string.h>
#include <ngf/plugin.h>
#include <ngf/sinkinterface.h>

#define LOG_CAT "resource: "

N_PLUGIN_NAME        ("resource")
N_PLUGIN_VERSION     ("0.3")
N_PLUGIN_DESCRIPTION ("Resource rules")

#define RESOURCE_KEY_PREFIX "media."

struct resource_def {
    char       *key;
    char       *type;
    gboolean    enabled_default;
    gboolean    enabled;
};

static GSList *def_list;

static void
lookup_types_from_keys (const char *key, const NValue *value, gpointer userdata)
{
    struct resource_def *resdef;
    const char          *value_str;

    (void) userdata;

    if (!key || !value)
        return;

    if (!g_str_has_prefix (key, RESOURCE_KEY_PREFIX) ||
        strlen (key) <= strlen (RESOURCE_KEY_PREFIX))
        return;

    resdef          = g_new0 (struct resource_def, 1);
    resdef->key     = g_strdup (key);
    resdef->type    = g_strdup (key + strlen (RESOURCE_KEY_PREFIX));

    value_str = n_value_get_string (value);
    if (g_strcmp0 (value_str, "0")     == 0 ||
        g_strcmp0 (value_str, "false") == 0 ||
        g_strcmp0 (value_str, "False") == 0 ||
        g_strcmp0 (value_str, "FALSE") == 0)
        resdef->enabled_default = FALSE;
    else
        resdef->enabled_default = TRUE;

    def_list = g_slist_append (def_list, resdef);
}

static void
resource_def_free_cb (gpointer data)
{
    struct resource_def *resdef = data;

    g_free (resdef->key);
    g_free (resdef->type);
    g_free (resdef);
}

static void
filter_sinks_cb (NHook *hook, void *data, void *userdata)
{
    const NProplist          *props;
    NSinkInterface          **sink;
    GSList                   *i;
    struct resource_def      *resdef;
    gboolean                  apply_filter = FALSE;
    NCore                    *core         = userdata;
    NCoreHookFilterSinksData *filter       = data;

    (void) hook;

    if (!def_list)
        return;

    N_DEBUG (LOG_CAT "filter sinks for request '%s'",
                     n_request_get_name (filter->request));

    props = n_request_get_properties (filter->request);

    for (i = def_list; i; i = g_slist_next (i)) {
        resdef = i->data;
        resdef->enabled = resdef->enabled_default;

        if (n_proplist_has_key (props, resdef->key))
            resdef->enabled = n_proplist_get_bool (props, resdef->key);

        if (!resdef->enabled)
            apply_filter = TRUE;

        N_DEBUG (LOG_CAT "resource type '%s' %s",
                         resdef->type,
                         resdef->enabled ? "enabled" : "disabled");
    }

    if (apply_filter) {
        for (i = def_list; i; i = g_slist_next (i)) {
            resdef = i->data;

            if (resdef->enabled)
                continue;

            for (sink = n_core_get_sinks (core); *sink; ++sink) {
                if (g_str_equal (n_sink_interface_get_type (*sink), resdef->type)) {
                    N_DEBUG (LOG_CAT "filter sink '%s' (%s = false)",
                                     n_sink_interface_get_name (*sink),
                                     resdef->key);
                    filter->sinks = g_list_remove (filter->sinks, *sink);
                }
            }
        }
    }
}

N_PLUGIN_LOAD (plugin)
{
    NCore           *core;
    const NProplist *params;
    def_list = NULL;

    core    = n_plugin_get_core (plugin);
    params  = n_plugin_get_params (plugin);

    n_proplist_foreach (params, lookup_types_from_keys, NULL);

    if (!def_list) {
        N_WARNING (LOG_CAT "filtering sinks by resources disabled, no mapping "
                           "defined from flag to sink type.");
        return FALSE;
    }

    /* connect to filter sinks hook. */

    (void) n_core_connect (core, N_CORE_HOOK_FILTER_SINKS, 0,
        filter_sinks_cb, core);

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    NCore *core = n_plugin_get_core (plugin);

    n_core_disconnect (core, N_CORE_HOOK_FILTER_SINKS, filter_sinks_cb, core);

    if (def_list)
        g_slist_free_full (def_list, resource_def_free_cb);
}
