/*
 * ngfd - Non-graphic feedback daemon, tonegen plugin
 *
 * Copyright (C) 2010 Nokia Corporation.
 *               2015 Jolla Ltd.
 *
 * Contact: Juho Hämäläinen <juho.hamalainen@tieto.com>
 *          Xun Chen <xun.chen@nokia.com>
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
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <stdbool.h>
#include <glib.h>
#include <limits.h>
#include <string.h>

#include "tonegend.h"
#include "dbusif.h"
#include "ausrv.h"
#include "stream.h"
#include "tone.h"
#include "envelop.h"
#include "indicator.h"
#include "dtmf.h"
#include "rfc4733.h"
#include "ngfif.h"

#define LOG_CAT        "tonegen: "

N_PLUGIN_NAME        ("tonegen")
N_PLUGIN_VERSION     ("0.2")
N_PLUGIN_DESCRIPTION ("Tone generator plugin")

struct properties {
    indicator_standard  standard;
    int                 sample_rate;
    bool                statistics;
    int                 buflen;
    int                 minreq;
    char               *dtmf_tags;
    char               *ind_tags;
    int                 dtmf_volume;
    int                 ind_volume;
};

struct userdata {
    NPlugin *plugin;
    struct properties properties;
    struct tonegend tonegend;
};

struct options_parse;
typedef bool (*prop_value_parser)(const NValue *val, struct options_parse *opt);

struct options_parse {
    const char *arg_name;
    prop_value_parser arg_parser;
    void *arg_value;
    char **arg_str;
};

static struct userdata u;

static bool prop_8khz_parser(const NValue *val, struct options_parse *opt);
static bool prop_standard_parser(const NValue *val, struct options_parse *opt);
static bool prop_string_parser(const NValue *val, struct options_parse *opt);
static bool prop_int_parser(const NValue *val, struct options_parse *opt);
static bool prop_bool_parser(const NValue *val, struct options_parse *opt);

static struct options_parse options[] = {
    { "8kHz"            , prop_8khz_parser      , &u.properties.sample_rate, NULL },
    { "standard"        , prop_standard_parser  , &u.properties.standard, NULL    },
    { "buflen"          , prop_int_parser       , &u.properties.buflen, NULL      },
    { "minreq"          , prop_int_parser       , &u.properties.minreq, NULL      },
    { "statistics"      , prop_bool_parser      , &u.properties.statistics, NULL  },
    { "tag-dtmf"        , prop_string_parser    , NULL, &u.properties.dtmf_tags   },
    { "tag-indicator"   , prop_string_parser    , NULL, &u.properties.ind_tags    },
    { "volume-dtmf"     , prop_int_parser       , &u.properties.dtmf_volume, NULL },
    { "volume-indicator", prop_int_parser       , &u.properties.ind_volume, NULL  },
    { NULL              , NULL                  , NULL, NULL                      }
};

/* OPTION PARSING */

static bool
prop_8khz_parser (const NValue *val, struct options_parse *opt)
{
    struct options_parse b;
    bool use_8khz;

    b.arg_value = (void *) &use_8khz;

    if (prop_bool_parser (val, &b)) {
        if (*(bool *) opt->arg_value)
            *(int *) opt->arg_value = 8000;
        return true;
    } else
        return false;
}

static bool
prop_bool_parser (const NValue *val, struct options_parse *opt)
{
    const char *str;

    str = n_value_get_string (val);
    if (g_ascii_strncasecmp (str ?: "", "true", 4) == 0 ||
        g_strcmp0 (str, "1") == 0)
        *(bool *) opt->arg_value = true;
    else if (g_ascii_strncasecmp (str ?: "", "false", 5) == 0 ||
             g_strcmp0 (str, "0") == 0)
        *(bool *) opt->arg_value = false;
    else {
        N_ERROR (LOG_CAT "Invalid value for boolean (%s)", str);
        return false;
    }

    return true;
}

static bool
prop_standard_parser (const NValue *val, struct options_parse *opt)
{
    const gchar *std;

    std = n_value_get_string(val);
    if (g_ascii_strncasecmp (std ?: "", "cept", 4) == 0)
        *(indicator_standard *) opt->arg_value = STD_CEPT;
    else if (g_ascii_strncasecmp (std ?: "", "ansi", 4) == 0)
        *(indicator_standard *) opt->arg_value = STD_ANSI;
    else if (g_ascii_strncasecmp (std ?: "", "japan", 5) == 0)
        *(indicator_standard *) opt->arg_value = STD_JAPAN;
    else if (g_ascii_strncasecmp (std ?: "", "atnt", 4) == 0)
        *(indicator_standard *) opt->arg_value = STD_ATNT;
    else {
        N_ERROR (LOG_CAT "Invalid standard '%s'", std);
        return false;
    }

    return true;
}

static bool
prop_string_parser (const NValue *val, struct options_parse *opt)
{
    *opt->arg_str = n_value_dup_string (val);

    return true;
}

static bool
prop_int_parser (const NValue *val, struct options_parse *opt)
{
    const char *str = n_value_get_string (val);
    char *endptr;
    long int value;

    if (!str || strlen(str) == 0)
        return false;

    value = strtol (str, &endptr, 10);

    if (value > INT_MAX || value < INT_MIN)
        return false;

    *(int *) opt->arg_value = (int) value;

    return *endptr == '\0';
}

static void
parse_opt (const char *key, const NValue *value, gpointer userdata)
{
    int i;

    (void) userdata;

    for (i = 0; options[i].arg_name; i++) {
        if (g_strcmp0 (options[i].arg_name, key) == 0) {
            if (!options[i].arg_parser (value, &options[i]))
                N_ERROR (LOG_CAT "Failed to parse plugin property with key '%s'", key);
        }
    }
}

static void
parse_options (NProplist *params)
{
    n_proplist_foreach (params, parse_opt, NULL);
}

static int
tonegen_sink_initialize (NSinkInterface *iface)
{
    (void) iface;

    /* Set default properties */
    u.properties.standard = STD_CEPT;
    u.properties.sample_rate = 48000;
    u.properties.statistics = false;
    u.properties.buflen = 0;
    u.properties.minreq = 0;
    u.properties.dtmf_tags = NULL;
    u.properties.ind_tags = NULL;
    u.properties.dtmf_volume = 100;
    u.properties.ind_volume = 100;

    NProplist *params = (NProplist*) n_plugin_get_params (u.plugin);
    N_DEBUG (LOG_CAT "starting sink");
    parse_options (params);

    ausrv_init ();
    stream_init ();
    tone_init ();
    envelop_init ();
    indicator_init ();
    dtmf_init ();
    rfc4733_init ();

    stream_set_default_samplerate (u.properties.sample_rate);
    stream_print_statistics (u.properties.statistics);
    stream_buffering_parameters (u.properties.buflen, u.properties.minreq);

    dtmf_set_properties (u.properties.dtmf_tags);
    indicator_set_properties (u.properties.ind_tags);

    dtmf_set_volume (u.properties.dtmf_volume);
    indicator_set_volume (u.properties.ind_volume);

    u.tonegend.ngfd_ctx = ngfif_create (&u.tonegend);

    if ((u.tonegend.dbus_ctx = dbusif_create (&u.tonegend)) == NULL) {
        N_ERROR (LOG_CAT "D-Bus setup failed");
        return FALSE;
    }

    if ((u.tonegend.ausrv_ctx = ausrv_create (&u.tonegend, NULL)) == NULL) {
        N_ERROR (LOG_CAT "PulseAudio setup failed.");
        return FALSE;
    }

    if (rfc4733_create (&u.tonegend) < 0) {
        N_ERROR (LOG_CAT "Can't setup rfc4733 interface on NGFD");
        return FALSE;
    }

    indicator_set_standard (u.properties.standard);

    return TRUE;
}

static void
tonegen_sink_shutdown (NSinkInterface *iface)
{
    (void) iface;
}

static int
tonegen_sink_can_handle (NSinkInterface *iface, NRequest *request)
{
    (void) iface;

    return ngfif_can_handle_request (&u.tonegend, request);
}

static int
tonegen_sink_prepare (NSinkInterface *iface, NRequest *request)
{
    n_sink_interface_synchronize (iface, request);
    return TRUE;
}

static int
tonegen_sink_play (NSinkInterface *iface, NRequest *request)
{
    (void) iface;

    return ngfif_handle_start_request(&u.tonegend, request);
}

static void
tonegen_sink_stop (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    (void) request;

    ngfif_handle_stop_request (&u.tonegend, request);
}

N_PLUGIN_LOAD (plugin)
{
    static const NSinkInterfaceDecl decl = {
        .name       = "tonegen",
        .type       = N_SINK_INTERFACE_TYPE_AUDIO,
        .initialize = tonegen_sink_initialize,
        .shutdown   = tonegen_sink_shutdown,
        .can_handle = tonegen_sink_can_handle,
        .prepare    = tonegen_sink_prepare,
        .play       = tonegen_sink_play,
        .pause      = NULL,
        .stop       = tonegen_sink_stop
    };

    u.plugin = plugin;
    n_plugin_register_sink (plugin, &decl);

    return TRUE;
}

N_PLUGIN_UNLOAD (plugin)
{
    (void) plugin;

    ausrv_destroy (u.tonegend.ausrv_ctx);
    ngfif_destroy (u.tonegend.ngfd_ctx);
    dbusif_destroy (u.tonegend.dbus_ctx);
    rfc4733_destroy ();
}
