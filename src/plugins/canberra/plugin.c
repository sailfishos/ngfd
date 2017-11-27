/*
 * ngfd - Non-graphic feedback daemon
 *
 * Copyright (C) 2013 Jolla Ltd <robin.burchell@jollamobile.com>
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

#include <ngf/plugin.h>
#include <canberra.h>

#include <string.h>

#define STREAM_PREFIX_KEY "sound.stream."
#define CANBERRA_KEY "plugin.canberra.data"
#define LOG_CAT "canberra: "
#define SOUND_FILENAME_KEY "canberra.filename"
#define SOUND_VOLUME_KEY "sound.volume"

typedef struct _CanberraData
{
    NRequest       *request;
    NSinkInterface *iface;
    const gchar    *filename;
    gboolean        sound_enabled;
    guint           complete_cb_id;
} CanberraData;

N_PLUGIN_NAME        ("canberra")
N_PLUGIN_VERSION     ("0.1")
N_PLUGIN_DESCRIPTION ("libcanberra plugin")

typedef struct sink_userdata {
    ca_context *c_context;
    GHashTable *cached_samples;
    gboolean    support_cached_samples;
} sink_userdata;


static void canberra_disconnect (sink_userdata *u)
{
    if (u->c_context) {
        ca_context_destroy (u->c_context);
        u->c_context = NULL;
    }
}

static int canberra_connect (sink_userdata *u)
{
    int error;

    if (u->c_context)
        return TRUE;

    g_hash_table_remove_all (u->cached_samples);
    ca_context_create (&u->c_context);
    error = ca_context_open (u->c_context);
    if (error) {
        N_WARNING (LOG_CAT "can't connect to canberra! %s", ca_strerror (error));
        ca_context_destroy (u->c_context);
        u->c_context = NULL;
        return FALSE;
    }

    return TRUE;
}

static int
canberra_sink_initialize (NSinkInterface *iface)
{
    sink_userdata *u;

    N_DEBUG (LOG_CAT "sink initialize");

    u = g_new0 (sink_userdata, 1);

    /* Only provide free function for key, since both key and value are same
     * string. */
    u->cached_samples = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    u->support_cached_samples = TRUE;
    canberra_connect (u);
    n_sink_interface_set_userdata (iface, u);
    return TRUE;
}

static void
canberra_sink_shutdown (NSinkInterface *iface)
{
    sink_userdata *u = n_sink_interface_get_userdata (iface);

    N_DEBUG (LOG_CAT "sink shutdown");

    if (u) {
        canberra_disconnect (u);
        if (u->cached_samples)
            g_hash_table_destroy (u->cached_samples);
        g_free (u);
    }
}

static int
canberra_sink_can_handle (NSinkInterface *iface, NRequest *request)
{
    (void) iface;

    NProplist *props = NULL;

    props = (NProplist*) n_request_get_properties (request);
    if (n_proplist_has_key (props, SOUND_FILENAME_KEY)) {
        N_DEBUG (LOG_CAT "request has a sound.filename, we can handle this.");
        return TRUE;
    }

    return FALSE;
}

static int
canberra_sink_prepare (NSinkInterface *iface, NRequest *request)
{
    N_DEBUG (LOG_CAT "sink prepare");

    CanberraData *data = g_slice_new0 (CanberraData);
    NProplist *props = props = (NProplist*) n_request_get_properties (request);

    data->request    = request;
    data->iface      = iface;
    data->filename = n_proplist_get_string (props, SOUND_FILENAME_KEY);
    data->sound_enabled = TRUE;
    data->complete_cb_id = 0;

    n_request_store_data (request, CANBERRA_KEY, data);
    n_sink_interface_synchronize (iface, request);

    if (n_proplist_has_key (props, SOUND_VOLUME_KEY))
        data->sound_enabled = n_proplist_get_int (props, SOUND_VOLUME_KEY) > 0;

    return TRUE;
}

static void
proplist_to_structure_cb (const char *key, const NValue *value, gpointer userdata)
{
    ca_proplist *target     = (ca_proplist*) userdata;
    const char   *prop_key   = NULL;
    const char   *prop_value = NULL;

    if (!g_str_has_prefix (key, STREAM_PREFIX_KEY))
        return;

    prop_key = key + strlen (STREAM_PREFIX_KEY);
    if (*prop_key == '\0')
        return;

    prop_value = n_value_get_string (value);
    ca_proplist_sets (target, prop_key, prop_value);
}

static gboolean
canberra_complete_cb (gpointer userdata) {
    CanberraData *data = userdata;

    data->complete_cb_id = 0;
    n_sink_interface_complete (data->iface, data->request);

    return FALSE;
}

static int
canberra_sink_play (NSinkInterface *iface, NRequest *request)
{
    sink_userdata   *u;
    CanberraData    *data;
    const NProplist *props;
    ca_proplist     *ca_props = 0;
    int              error;

    N_DEBUG (LOG_CAT "sink play");

    u = n_sink_interface_get_userdata (iface);
    data = n_request_get_data (request, CANBERRA_KEY);

    g_assert (u);
    g_assert (data);

    if (!data->sound_enabled)
        goto complete;

    if (canberra_connect (u) == FALSE)
        return FALSE;

    props = n_request_get_properties (request);
    ca_proplist_create (&ca_props);

    /* TODO: don't hardcode */
    ca_proplist_sets (ca_props, CA_PROP_CANBERRA_XDG_THEME_NAME, "jolla-ambient");
    ca_proplist_sets (ca_props, CA_PROP_EVENT_ID, data->filename);
    if (u->support_cached_samples)
        ca_proplist_sets (ca_props, CA_PROP_CANBERRA_CACHE_CONTROL, "permanent");

    /* convert all properties within the request that begin with
       "sound.stream." prefix. */
    n_proplist_foreach (props, proplist_to_structure_cb, ca_props);

    if (u->support_cached_samples && !g_hash_table_contains(u->cached_samples, data->filename)) {
        N_DEBUG (LOG_CAT "caching sample %s", data->filename);
        error = ca_context_cache_full (u->c_context, ca_props);
        if (error == CA_ERROR_NOTSUPPORTED) {
            N_WARNING (LOG_CAT "sample caching not supported by backend. disabling for the duration of plugin.");
            u->support_cached_samples = FALSE;
        } else if (error != CA_SUCCESS) {
            N_WARNING (LOG_CAT "canberra couldn't cache sample %s (%d: %s)", data->filename, -error, ca_strerror(error));
            ca_proplist_destroy (ca_props);
            canberra_disconnect (u);
            return FALSE;
        } else
            g_hash_table_add (u->cached_samples, g_strdup(data->filename));
    }

    error = ca_context_play_full (u->c_context, 0, ca_props, NULL, NULL);
    ca_proplist_destroy (ca_props);

    if (error != CA_SUCCESS) {
        N_WARNING (LOG_CAT "sink play had a warning: %s", ca_strerror (error));
        canberra_disconnect (u);
        return FALSE;
    }

complete:
    /* We do not know how long our samples play, but let's guess we
     * are done in 200ms. */
    data->complete_cb_id = g_timeout_add (200, canberra_complete_cb, data);

    return TRUE;
}

static void
canberra_sink_stop (NSinkInterface *iface, NRequest *request)
{
    N_DEBUG (LOG_CAT "sink stop");

    (void) iface;

    CanberraData *data = (CanberraData*) n_request_get_data (request, CANBERRA_KEY);
    g_assert (data != NULL);

    if (data->complete_cb_id > 0)
        g_source_remove (data->complete_cb_id);

    g_slice_free (CanberraData, data);
}

N_PLUGIN_LOAD (plugin)
{
    N_DEBUG (LOG_CAT "plugin load");

    static const NSinkInterfaceDecl decl = {
        .name       = "canberra",
        .initialize = canberra_sink_initialize,
        .shutdown   = canberra_sink_shutdown,
        .can_handle = canberra_sink_can_handle,
        .prepare    = canberra_sink_prepare,
        .play       = canberra_sink_play,
        .pause      = NULL,
        .stop       = canberra_sink_stop
    };

    n_plugin_register_sink (plugin, &decl);

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    (void) plugin;

    N_DEBUG (LOG_CAT "plugin unload");
}
