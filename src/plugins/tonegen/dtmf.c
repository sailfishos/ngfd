/*************************************************************************
This file is part of ngfd / tone-generator

Copyright (C) 2010 Nokia Corporation.
              2015 Jolla Ltd.

This library is free software; you can redistribute
it and/or modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation
version 2.1 of the License.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
USA.
*************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib.h>

#include <ngf/log.h>
#include <trace/trace.h>

#include "ausrv.h"
#include "stream.h"
#include "tone.h"
#include "indicator.h"
#include "dtmf.h"
#include "dbusif.h"

#define LOG_CAT "tonegen-dtmf: "

struct dtmf {
    char           symbol;
    uint32_t       low_freq;
    uint32_t       high_freq;
};


static struct dtmf dtmf_defs[DTMF_MAX] = {
    {'0', 941, 1336},
    {'1', 697, 1209},
    {'2', 697, 1336},
    {'3', 697, 1477},
    {'4', 770, 1209},
    {'5', 770, 1336},
    {'6', 770, 1477},
    {'7', 852, 1209},
    {'8', 852, 1336},
    {'9', 852, 1477},
    {'*', 941, 1209},
    {'#', 941, 1477},
    {'A', 697, 1633},
    {'B', 770, 1633},
    {'C', 852, 1633},
    {'D', 941, 1633}
};

static char   *dtmf_stream = STREAM_DTMF;
static void   *dtmf_props  = NULL;
static int     vol_scale   = 100;
static bool    mute        = false;
static guint   tmute_id;


static void destroy_callback(void *);
static void set_mute_timeout(struct ausrv *, guint);
static gboolean mute_timeout_callback(gpointer);
static void request_muting(struct ausrv *ausrv, bool new_mute);



int dtmf_init(void)
{
    return 0;
}

void dtmf_play(struct ausrv *ausrv, dtmf_tone tone, uint32_t volume, int duration, const char *extra_properties)
{
    struct stream *stream = stream_find(ausrv, dtmf_stream);
    struct dtmf   *dtmf   = dtmf_defs + tone;
    uint32_t       per    = duration;
    uint32_t       play   = duration > 60000 ? duration - 20000 : duration;
    int            type_l = TONE_DTMF_L;
    int            type_h = TONE_DTMF_H;
    uint32_t       timeout;
    void          *properties = dtmf_props;
        
    if (tone >= DTMF_MAX || (duration != 0 && duration < 10000))
        return;

    if (!duration) {
        /*
         * These types will make the DTMF tone as 'indicator'
         * i.e. the indicator_stop() will work on them
         */
        type_l = TONE_DTMF_IND_L;
        type_h = TONE_DTMF_IND_H;

        per = play = 1000000;
    }

    if (stream != NULL) {
        if (!duration) {
            indicator_stop(ausrv, true);
            dtmf_stop(ausrv);
        }
    }
    else {
        if (extra_properties)
            properties = stream_merge_properties(dtmf_props, extra_properties);

        stream = stream_create(ausrv, dtmf_stream, NULL, 0,
                               tone_write_callback,
                               destroy_callback,
                               properties,
                               NULL);

        if (extra_properties)
            stream_free_properties(properties);

        if (stream == NULL) {
            N_ERROR(LOG_CAT "%s(): Can't create stream", __FUNCTION__);
            return;
        }
    }

    volume = (vol_scale * volume) / 100;

    tone_create(stream, type_l, dtmf->low_freq , volume/2, per, play, 0, duration);
    tone_create(stream, type_h, dtmf->high_freq, volume/2, per, play, 0, duration);

    timeout = duration ? duration + (30 * 1000000) : (1 * 60 * 1000000);

    stream_set_timeout(stream, timeout);

    request_muting(ausrv, true);
    set_mute_timeout(ausrv, 0);
}

void dtmf_stop(struct ausrv *ausrv)
{
    struct stream *stream = stream_find(ausrv, dtmf_stream);
    struct tone   *tone;
    struct tone   *next;

    TRACE("%s() stream=%s", __FUNCTION__, stream ? stream->name:"<no-stream>");

    if (stream != NULL) {
        for (tone = (struct tone *)stream->data;  tone;  tone = next) {

            next = tone->next;

            switch (tone->type) {
                
            case TONE_DTMF_IND_L:
            case TONE_DTMF_IND_H:
                /* in the future a linear ramp-down enevlop can be set */
                tone_destroy(tone, true);
                break;

            default:
                if (!tone_chainable(tone->type))
                    tone_destroy(tone, true);
                break;
            }
        }

        if (stream->data == NULL)
            stream_clean_buffer(stream);

        stream_set_timeout(stream, 10 * 1000000);
        set_mute_timeout(ausrv, 2 * 1000000);        
    }
}


void dtmf_set_properties(char *propstring)
{
    dtmf_props = stream_parse_properties(propstring);
}


void dtmf_set_volume(uint32_t volume)
{
    vol_scale = volume;
}

static void destroy_callback(void *data)
{
    struct tone   *tone = (struct tone *)data;
    struct stream *stream;
    struct ausrv  *ausrv;

    set_mute_timeout(NULL, 0);

    if (mute && tone != NULL) {
        stream = tone->stream;
        ausrv  = stream->ausrv;

        request_muting(ausrv, false);

        mute = false;
    }

    tone_destroy_callback(data);
}

static void set_mute_timeout(struct ausrv *ausrv, guint interval)
{
    if (interval)
        TRACE("add %d usec mute timeout", interval);
    else if (tmute_id)
        TRACE("remove mute timeout");

    if (tmute_id != 0) {
        g_source_remove(tmute_id);
        tmute_id = 0;
    }

    if (interval > 0 && ausrv != NULL) {
        tmute_id = g_timeout_add(interval/1000, mute_timeout_callback, ausrv);
    }
}

static gboolean mute_timeout_callback(gpointer data)
{
    struct ausrv *ausrv = (struct ausrv *)data;

    TRACE("mute timeout fired");

    request_muting(ausrv, false);

    tmute_id = 0;

    return FALSE;
}

static void request_muting(struct ausrv *ausrv, bool new_mute)
{
    int             sts;
    dbus_bool_t     set_mute = new_mute ? TRUE : FALSE;

    if (ausrv != NULL && mute != new_mute) {
        sts = dbusif_send_signal(ausrv->tonegend, NULL, "Mute",
                                 DBUS_TYPE_BOOLEAN, &set_mute,
                                 DBUS_TYPE_INVALID);

        if (sts != 0)
            N_ERROR(LOG_CAT "failed to send mute signal");
        else {
            TRACE("sent signal to turn mute %s", new_mute ? "on" : "off");
            mute = new_mute;
        }
    }
}
