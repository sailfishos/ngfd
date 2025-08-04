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

#include <ngf/plugin.h>
#include <ImmVibe.h>
#include <ImmVibeCore.h>
#include <stdio.h>

#define IMMVIBE_KEY                 "plugin.immvibe.data"
#define SOUND_REPEAT_KEY            "sound.repeat"
#define SOUND_FILENAME_KEY          "sound.filename"
#define SOUND_FILENAME_ORIGINAL_KEY "sound.filename.original"
#define IMMVIBE_FILENAME_KEY        "immvibe.filename"
#define IMMVIBE_LOOKUP_KEY          "immvibe.lookup"
#define IMMVIBE_LOOKUP_FROM_KEY     "immvibe.lookup_from_key"
#define ALLOW_CUSTOM_KEY            "transform.allow_custom"
#define SYSTEM_SOUND_PATH           "/usr/share/sounds/"
#define LOG_CAT                     "immvibe: "
#define POLL_TIMEOUT                500

typedef struct _ImmvibeData
{
    NRequest       *request;
    NSinkInterface *iface;
    guint           id;
    gpointer        pattern;
    gboolean        paused;
    guint           poll_id;
    gboolean        repeat_pattern;
    guint           idle_complete_id;
} ImmvibeData;

static VibeInt32    device      = VIBE_INVALID_DEVICE_HANDLE_VALUE;
static const gchar *search_path = NULL;
NContext* context = NULL;

guint vibrator_start (gpointer pattern_data, gpointer userdata);

N_PLUGIN_NAME        ("immvibe")
N_PLUGIN_VERSION     ("0.1")
N_PLUGIN_DESCRIPTION ("Immersion vibra plugin")

static gboolean
pattern_is_completed (gint id)
{
    VibeStatus status;
    VibeInt32 effect_state = 0;

    status = ImmVibeGetEffectState (device, id, &effect_state);
    if (VIBE_SUCCEEDED (status)) {
        if (effect_state == VIBE_EFFECT_STATE_PLAYING)
            return FALSE;
    }

    return TRUE;
}

static gboolean
pattern_poll_cb (gpointer userdata)
{
    ImmvibeData *data = (ImmvibeData*) n_request_get_data ((NRequest *)userdata, IMMVIBE_KEY);

    if (!data->paused && pattern_is_completed (data->id)) {
        N_DEBUG (LOG_CAT "vibration has been completed.");

        data->poll_id = 0;
        if (data->repeat_pattern) {
            N_DEBUG (LOG_CAT "pattern needs to be repeated");

            if (data->id > 0)
                ImmVibeStopPlayingEffect (device, data->id);

            if (data->pattern)
                data->id = vibrator_start (data->pattern, data->request);

            if (!data->pattern || data->id == 0)
                n_sink_interface_complete (data->iface, data->request);
        }
        else {
            n_sink_interface_complete (data->iface, data->request);
        }

        return FALSE;
    }

    return TRUE;
}

static gboolean
vibrator_reconnect ()
{
    if (device != VIBE_INVALID_DEVICE_HANDLE_VALUE) {
        ImmVibeCloseDevice (device);
        ImmVibeTerminate   ();

        device = VIBE_INVALID_DEVICE_HANDLE_VALUE;
    }

    if (VIBE_FAILED (ImmVibeInitialize (VIBE_CURRENT_VERSION_NUMBER)))
        return FALSE;

    if (VIBE_FAILED (ImmVibeOpenDevice (0, &device)))
        return FALSE;

    return TRUE;
}

static gpointer
vibrator_load (const char *filename)
{
    FILE *fp = NULL;
    unsigned long pattern_size = 0;
    size_t bytes_read = 0;
    VibeUInt8 *data = NULL;

    if (filename == NULL)
        goto failed;

    if ((fp = fopen (filename, "rb")) == NULL)
        goto failed;

    fseek (fp, 0L, SEEK_END);
    pattern_size = ftell (fp);
    fseek (fp, 0L, SEEK_SET);

    if (pattern_size > 0 && ((data = g_new (VibeUInt8, pattern_size)) != NULL)) {
        bytes_read = fread (data, sizeof (VibeUInt8), pattern_size, fp);
        if (bytes_read != pattern_size)
            goto failed;

        fclose (fp);

        return (gpointer)data;
    }

failed:
    if (data) {
        g_free (data);
        data = NULL;
    }

    if (fp) {
        fclose (fp);
        fp = NULL;
    }

    return NULL;
}

static gchar*
build_vibration_filename (const char *path, const char *source)
{
    gchar *separator = NULL;
    gchar *output    = NULL;
    gchar *basename  = NULL;
    gchar *result    = NULL;

    if (!source)
        return NULL;

    if (!path) {
        basename = g_strdup (source);
        if ((separator = g_strrstr (basename, ".")) == NULL) {
            g_free (basename);
            return NULL;
        }

        *separator = '\0';
        result = g_strdup_printf ("%s.ivt", basename);

        g_free (output);
        g_free (basename);
    }
    else {
        basename = g_path_get_basename (source);
        if ((separator = g_strrstr (basename, ".")) == NULL) {
            g_free (basename);
            return NULL;
        }

        *separator = '\0';
        output = g_strdup_printf ("%s.ivt", basename);
        result = g_build_filename (path, output, NULL);

        g_free (output);
        g_free (basename);
    }

    return result;
}

guint
vibrator_start (gpointer pattern_data, gpointer userdata)
{
    ImmvibeData *data    = (ImmvibeData*) n_request_get_data ((NRequest *)userdata, IMMVIBE_KEY);
    VibeUInt8   *effects = pattern_data ? (VibeUInt8*) pattern_data : g_pVibeIVTBuiltInEffects;
    gint         id      = 0;
    VibeInt32    ret     = 0;
    gboolean     retry   = FALSE;

    do {
        ret = ImmVibePlayIVTEffect (device, effects, 0, &id);

        if (VIBE_SUCCEEDED (ret)) {
            n_sink_interface_set_resync_on_master (data->iface, data->request);

            N_DEBUG ("%s >> started pattern with id %d", __FUNCTION__, id);
            data->poll_id = g_timeout_add (POLL_TIMEOUT, pattern_poll_cb, userdata);
            return id;
        }
        else if (ret == VIBE_E_NOT_INITIALIZED) {
            if (retry)
                return 0;

            N_DEBUG ("%s >> vibrator is not initialized.", __FUNCTION__);
            if (!vibrator_reconnect ()) {
                N_WARNING ("%s >> failed to reconnect to vibrator.", __FUNCTION__);
                return 0;
            }
            else
                N_DEBUG ("%s >> reconnected to vibrator.", __FUNCTION__);

            retry = TRUE;
        }

    } while (retry);

    return 0;
}

static int
immvibe_sink_initialize (NSinkInterface *iface)
{
    (void) iface;
    N_DEBUG (LOG_CAT "sink initialize");
    if (!vibrator_reconnect ())
        N_WARNING ("%s >> failed to connect to vibrator daemon.", __FUNCTION__);

    context = n_core_get_context (n_sink_interface_get_core (iface));

    return TRUE;
}

static void
immvibe_sink_shutdown (NSinkInterface *iface)
{
    (void) iface;

    N_DEBUG (LOG_CAT "sink shutdown");

    ImmVibeStopAllPlayingEffects (device);
    ImmVibeCloseDevice (device);
    device = VIBE_INVALID_DEVICE_HANDLE_VALUE;
    ImmVibeTerminate ();
}

static int
immvibe_sink_can_handle (NSinkInterface *iface, NRequest *request)
{
    (void) iface;

    const NProplist *props = n_request_get_properties (request);

    NCore    *core    = n_sink_interface_get_core     (iface);
    NContext *context = n_core_get_context            (core);
    NValue   *enabled = NULL;

    enabled = (NValue*) n_context_get_value (context,
        "profile.current.vibrating.alert.enabled");

    if (!enabled || !n_value_get_bool (enabled)) {
        N_DEBUG (LOG_CAT "vibration is not enabled, no action from immvibe.");
        return FALSE;
    }

    if (n_proplist_has_key (props, "immvibe.filename") || 
		n_proplist_has_key (props, "immvibe.filename_original")) {
        N_DEBUG (LOG_CAT "can handle request");
        return TRUE;
    }

    return FALSE;
}

static gboolean
factory_sound_filename (const char *filename)
{
    if (filename && g_str_has_prefix (filename, SYSTEM_SOUND_PATH))
        return TRUE;

    return FALSE;
}

static const char*
lookup_sound_from_context (NContext *context, const char *key)
{
    NValue *v;

    if (!context || !key)
        return NULL;

    v = (NValue*) n_context_get_value (context, key);
    return n_value_get_string (v);
}

static int
immvibe_sink_prepare (NSinkInterface *iface, NRequest *request)
{
    const NProplist *props = n_request_get_properties (request);
    ImmvibeData *data = g_slice_new0 (ImmvibeData);

    char *filename;
    const char *sound_filename, *immvibe_filename, *lookup_key,
        *custom_file, *factory_sound = NULL;
    gboolean lookup, allow_custom, sound_repeat;

    N_DEBUG (LOG_CAT "sink prepare");

    data->request    = request;
    data->iface      = iface;
    
    sound_filename = n_proplist_get_string (props, SOUND_FILENAME_KEY);
    immvibe_filename = n_proplist_get_string (props, IMMVIBE_FILENAME_KEY);
    lookup = n_proplist_get_bool (props, IMMVIBE_LOOKUP_KEY);
    custom_file = n_proplist_get_string (props, SOUND_FILENAME_ORIGINAL_KEY);
    allow_custom = n_proplist_get_bool (props, ALLOW_CUSTOM_KEY);
    sound_repeat = n_proplist_get_bool (props, SOUND_REPEAT_KEY);

    if (custom_file && !allow_custom &&
        !g_file_test (custom_file, G_FILE_TEST_EXISTS) && !n_request_is_fallback (request))
    {
        g_slice_free (ImmvibeData, data);
        return FALSE;
    }

    if (n_request_is_fallback (request))
        custom_file = NULL;

    /* case 1 (factory sounds): custom sound file through audio parameter */

    if (custom_file)
        factory_sound = custom_file;

    /* case 2 (factory sounds): not a custom file, but we have lookup parameter set to
       find the vibration pattern based on filename. */

    if (!custom_file && lookup)
        factory_sound = sound_filename;

    /* case 3 (factory sounds): and if we have a lookup_from_key set, then we don't use the sound
       filename, but a sound queried from the profile using the given key. */

    if (!custom_file && lookup && (lookup_key = n_proplist_get_string (props, IMMVIBE_LOOKUP_FROM_KEY)))
        factory_sound = lookup_sound_from_context (context, lookup_key);

    /* all the cases apply to "factory sounds", which are files with custom
       vibration patterns. */

    if (factory_sound_filename (factory_sound)) {
        filename = build_vibration_filename (search_path, factory_sound);
        N_DEBUG (LOG_CAT "sound is factory sound, loading pattern from: %s", filename);
        data->pattern = vibrator_load (filename);
        g_free (filename);
    }

    /* default case: if no pattern yet, then use immvibe.filename to load either
       absolute path or a filename that is to be searched from the vibration path. */

    if (!data->pattern && immvibe_filename) {

        /* if repeat is set, then we need to repeat the pattern too */
        data->repeat_pattern = sound_repeat;

        if (!(data->pattern = vibrator_load (immvibe_filename))) {
            filename = build_vibration_filename (search_path, immvibe_filename);
            data->pattern = vibrator_load (filename);
            g_free (filename);
        }
    }

    /* succeed even if no data. */

    n_request_store_data (request, IMMVIBE_KEY, data);
    n_sink_interface_synchronize (iface, request);

    return TRUE;
}

static gboolean
immvibe_idle_complete_cb (gpointer userdata)
{
    ImmvibeData *data = (ImmvibeData*) userdata;

    data->idle_complete_id = 0;

    /* nothing played, we just complete this event. */
    N_DEBUG (LOG_CAT "idle complete");
    n_sink_interface_complete (data->iface, data->request);

    return FALSE;
}

static int
immvibe_sink_play (NSinkInterface *iface, NRequest *request)
{
    N_DEBUG (LOG_CAT "sink play");

    (void) iface;

    ImmvibeData *data = (ImmvibeData*) n_request_get_data (request, IMMVIBE_KEY);
    g_assert (data != NULL);

    if (data->paused) {
        if (data->id > 0) {
            (void) ImmVibeResumePausedEffect (device, data->id);
        }

        data->paused = FALSE;
        return TRUE;
    }

    if (data->pattern) {
        data->id = vibrator_start (data->pattern, request);
    }

    if (!data->pattern || data->id == 0) {
        data->idle_complete_id = g_idle_add (immvibe_idle_complete_cb, data);
    }

    return TRUE;
}

static int
immvibe_sink_pause (NSinkInterface *iface, NRequest *request)
{
    N_DEBUG (LOG_CAT "sink pause");

    (void) iface;

    ImmvibeData *data = (ImmvibeData*) n_request_get_data (request, IMMVIBE_KEY);
    g_assert (data != NULL);

    if (!data->pattern) {
        return TRUE;
    }

    if (data->id > 0) {
        (void) ImmVibePausePlayingEffect (device, data->id);
    }

    data->paused = TRUE;

    return TRUE;
}

static void
immvibe_sink_stop (NSinkInterface *iface, NRequest *request)
{
    N_DEBUG (LOG_CAT "sink stop");

    (void) iface;

    ImmvibeData *data = (ImmvibeData*) n_request_get_data (request, IMMVIBE_KEY);
    if (!data)
        return;

    n_request_store_data (request, IMMVIBE_KEY, NULL);

    if (data->id > 0) {
        ImmVibeStopPlayingEffect (device, data->id);
    }

    if (data->poll_id > 0) {
        g_source_remove (data->poll_id);
        data->poll_id = 0;
    }

    if (data->idle_complete_id > 0) {
        g_source_remove (data->idle_complete_id);
        data->idle_complete_id = 0;
    }

    g_free       (data->pattern);
    g_slice_free (ImmvibeData, data);
}

N_PLUGIN_LOAD (plugin)
{
    N_DEBUG (LOG_CAT "plugin load");

    static const NSinkInterfaceDecl decl = {
        .name       = "immvibe",
        .type       = N_SINK_INTERFACE_TYPE_VIBRATOR,
        .initialize = immvibe_sink_initialize,
        .shutdown   = immvibe_sink_shutdown,
        .can_handle = immvibe_sink_can_handle,
        .prepare    = immvibe_sink_prepare,
        .play       = immvibe_sink_play,
        .pause      = immvibe_sink_pause,
        .stop       = immvibe_sink_stop
    };

    n_plugin_register_sink (plugin, &decl);

    search_path = n_proplist_get_string (n_plugin_get_params (plugin), "vibration_search_path");

    if (search_path == NULL) {
        N_WARNING (LOG_CAT "Vibration pattern search path is missing from the configuration file");

        return FALSE;
    }

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    (void) plugin;

    N_DEBUG (LOG_CAT "plugin unload");
}
