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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ngf/request.h>
#include <ngf/proplist.h>
#include <ngf/value.h>
#include <ngf/log.h>

#include <trace/trace.h>

#include "tonegend.h"
#include "dbusif.h"
#include "ngfif.h"
#include "tone.h"
#include "indicator.h"
#include "dtmf.h"
#include "rfc4733.h"

#define LOG_CAT "tonegen-rfc4733: "

struct method {
    char  *intf;                                    /* interface name */
    char  *memb;                                    /* method name */
    char  *sig;                                     /* signature */
    int  (*func)(DBusMessage *, struct tonegend *); /* implementing function */
};

struct method_ngfd {
    char  *name;                                        /* tone type */
    int  (*func_start)(NRequest *, struct tonegend *);  /* implementing function */
    int  (*func_stop)(NRequest *, struct tonegend *);   /* implementing function */
};

static int start_event_tone(DBusMessage *, struct tonegend *);
static int stop_tone(DBusMessage *, struct tonegend *);
static int stop_event_tone(DBusMessage *, struct tonegend *);
static int start_dtmf_tone_ngfd(NRequest *request, struct tonegend *);
static int start_indicator_tone_ngfd(NRequest *request, struct tonegend *);
static int stop_dtmf_tone_ngfd(NRequest *request, struct tonegend *);
static int stop_indicator_tone_ngfd(NRequest *request, struct tonegend *);
static uint32_t linear_volume(int);

#define TONE_INDICATOR      0
#define TONE_DTMF           1
#define DBUS_SENDER_MAXLEN  16
static char tone_sender[2][DBUS_SENDER_MAXLEN];

static struct method  method_defs[] = {
    {NULL, "StartEventTone", "uiu", start_event_tone},
    {NULL, "StartNotificationTone", "uiu", start_event_tone}, /* backward compatible */
    {NULL, "StopTone", "", stop_tone},
    {NULL, "StopEventTone", "u", stop_event_tone},
    {NULL, NULL, NULL, NULL}
};

static struct method_ngfd  method_ngfd_defs[] = {
    {"dtmf",        start_dtmf_tone_ngfd,       stop_dtmf_tone_ngfd         },
    {"indicator",   start_indicator_tone_ngfd,  stop_indicator_tone_ngfd    },
    {NULL,          NULL,                       NULL                        }
};

static GHashTable *indicator_hash = NULL;

int rfc4733_init(void)
{
    return 0;
}

int rfc4733_create(struct tonegend *tonegend)
{
    struct method *m;
    int err;
    int sts;

    for (m = method_defs, err = 0;    m->memb != NULL;    m++) {
        sts = dbusif_register_input_method(tonegend, m->intf, m->memb,
                                           m->sig, m->func);

        if (sts < 0) {
            N_ERROR(LOG_CAT "%s(): Can't register D-Bus method '%s'",
                      __FUNCTION__, m->memb);
            err = -1;
        }
    }

    return err;
}

int rfc4733_create_ngfd(struct tonegend *tonegend)
{
    struct method_ngfd *m;
    int sts;
    int err = 0;

    for (m = method_ngfd_defs, err = 0; m->name; m++) {
        sts = ngfif_register_input_method(tonegend, m->name, m->func_start, m->func_stop);
        if (sts < 0) {
            N_ERROR(LOG_CAT "Failed to register method %s", m->name);
            err = -1;
        }
    }

    indicator_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                           g_free, NULL);

    g_hash_table_insert(indicator_hash, g_strdup("dial"),       GUINT_TO_POINTER(TONE_DIAL));
    g_hash_table_insert(indicator_hash, g_strdup("busy"),       GUINT_TO_POINTER(TONE_BUSY));
    g_hash_table_insert(indicator_hash, g_strdup("congest"),    GUINT_TO_POINTER(TONE_CONGEST));
    g_hash_table_insert(indicator_hash, g_strdup("radio_ack"),  GUINT_TO_POINTER(TONE_RADIO_ACK));
    g_hash_table_insert(indicator_hash, g_strdup("radio_na"),   GUINT_TO_POINTER(TONE_RADIO_NA));
    g_hash_table_insert(indicator_hash, g_strdup("error"),      GUINT_TO_POINTER(TONE_ERROR));
    g_hash_table_insert(indicator_hash, g_strdup("wait"),       GUINT_TO_POINTER(TONE_WAIT));
    g_hash_table_insert(indicator_hash, g_strdup("ring"),       GUINT_TO_POINTER(TONE_RING));

    return err;
}

void rfc4733_destroy()
{
    if (indicator_hash) {
        g_hash_table_destroy(indicator_hash);
        indicator_hash = NULL;
    }
}

static int start_indicator_tone_ngfd(NRequest *request, struct tonegend *tonegend)
{
    struct ausrv *ausrv = tonegend->ausrv_ctx;
    gpointer      event_p;
    uint32_t      event;
    int32_t       dbm0 = 0;
    uint32_t      duration = 0;
    uint32_t      volume;
    const NProplist *proplist;
    const NValue *value;

    proplist = n_request_get_properties(request);
    N_DEBUG(LOG_CAT "request indicator event");

    if (!n_proplist_has_key(proplist, "tonegen.pattern")) {
        N_WARNING(LOG_CAT "request doesn't have pattern.");
        return FALSE;
    }

    value = n_proplist_get(proplist, "tonegen.pattern");
    if (n_value_type(value) == N_VALUE_TYPE_STRING) {

        if (!(event_p = g_hash_table_lookup(indicator_hash,
                                            n_value_get_string(value)))) {
            N_WARNING(LOG_CAT "request doesn't have valid pattern.");
            return FALSE;
        }
        event = GPOINTER_TO_UINT(event_p);

    } else if (n_value_type(value) == N_VALUE_TYPE_UINT) {

        switch (n_value_get_uint(value)) {
        case EVENT_DIAL:        event = TONE_DIAL;      break;
        case EVENT_RING:        event = TONE_RING;      break;
        case EVENT_BUSY:        event = TONE_BUSY;      break;
        case EVENT_CONGEST:     event = TONE_CONGEST;   break;
        case EVENT_ERROR:       event = TONE_ERROR;     break;
        case EVENT_WAIT:        event = TONE_WAIT;      break;
        case EVENT_RADIO_ACK:   event = TONE_RADIO_ACK; break;
        case EVENT_RADIO_NA:    event = TONE_RADIO_NA;  break;

        default:
            N_WARNING(LOG_CAT "invalid event %u", n_value_get_uint(value));
            return FALSE;
        }
    } else {
        N_WARNING(LOG_CAT "request doesn't have valid pattern.");
        return FALSE;
    }

    if (n_proplist_has_key(proplist, "tonegen.dbm0"))
        dbm0 = n_proplist_get_int(proplist, "tonegen.dbm0");
    if (n_proplist_has_key(proplist, "tonegen.duration"))
        duration = n_proplist_get_uint(proplist, "tonegen.duration");

    volume = linear_volume(dbm0);

    N_DEBUG(LOG_CAT "%s(): event %u  volume %d dbm0 (%u) duration %u msec",
          __FUNCTION__, event, dbm0, volume, duration);

    indicator_play(ausrv, event, volume, duration * 1000);

    return TRUE;
}

static int start_dtmf_tone_ngfd(NRequest *request, struct tonegend *tonegend)
{
    struct ausrv *ausrv = tonegend->ausrv_ctx;
    uint32_t      event;
    int32_t       dbm0 = 0;
    uint32_t      volume;
    const char   *extra_props = NULL;
    const NProplist *proplist;

    proplist = n_request_get_properties(request);
    N_DEBUG(LOG_CAT "request event");

    if (!n_proplist_has_key(proplist, "tonegen.value")) {
        N_WARNING(LOG_CAT "request doesn't have event.");
        return FALSE;
    }

    event = n_proplist_get_uint(proplist, "tonegen.value");

    if (event >= DTMF_MAX) {
        N_WARNING(LOG_CAT "Invalid DTMF value.");
        return FALSE;
    }

    if (n_proplist_has_key(proplist, "tonegen.dbm0"))
        dbm0 = n_proplist_get_int(proplist, "tonegen.dbm0");

    if (n_proplist_has_key(proplist, "tonegen.properties"))
        extra_props = n_proplist_get_string(proplist, "tonegen.properties");

    volume = linear_volume(dbm0);

    N_DEBUG(LOG_CAT "%s(): event %u volume %d dbm0 (%u) extra properties (%s)",
          __FUNCTION__, event, dbm0, volume, extra_props ? extra_props : "none");

    dtmf_play(ausrv, event, volume, 0, extra_props);

    return TRUE;
}

static int stop_dtmf_tone_ngfd(NRequest *request, struct tonegend *tonegend)
{
    struct ausrv *ausrv = tonegend->ausrv_ctx;
    (void) request;

    N_DEBUG(LOG_CAT "%s(): stop dtmf tone", __FUNCTION__);

    dtmf_stop(ausrv);

    return TRUE;
}

static int stop_indicator_tone_ngfd(NRequest *request, struct tonegend *tonegend)
{
    struct ausrv *ausrv = tonegend->ausrv_ctx;
    (void) request;

    N_DEBUG(LOG_CAT "%s(): stop indicator tone", __FUNCTION__);

    indicator_stop(ausrv, true);

    return TRUE;
}

static int start_event_tone(DBusMessage *msg, struct tonegend *tonegend)
{
    struct ausrv *ausrv = tonegend->ausrv_ctx;
    uint32_t      event;
    int32_t       dbm0;
    uint32_t      duration;
    uint32_t      volume;
    int           indtype;
    int           success;
    char         *sender;

    success = dbus_message_get_args(msg, NULL,
                                    DBUS_TYPE_UINT32, &event,
                                    DBUS_TYPE_INT32 , &dbm0,
                                    DBUS_TYPE_UINT32, &duration,
                                    DBUS_TYPE_INVALID);

    if (!success) {
        N_ERROR(LOG_CAT "%s(): Can't parse arguments", __FUNCTION__);
        return FALSE;
    }

    volume = linear_volume(dbm0);

    N_DEBUG(LOG_CAT "%s(): event %u  volume %d dbm0 (%u) duration %u msec",
          __FUNCTION__, event, dbm0, volume, duration);

    sender = (char *)dbus_message_get_sender(msg);

    if (event < DTMF_MAX) {
        if (tone_sender[TONE_DTMF][0])
            N_DEBUG(LOG_CAT "%s(): got request to play the second DTMF tone", __FUNCTION__);

        strncpy(tone_sender[TONE_DTMF], sender, DBUS_SENDER_MAXLEN);
        dtmf_play(ausrv, event, volume, 0, NULL);
    }
    else {
        switch (event) {
        case EVENT_DIAL:        indtype = TONE_DIAL;        break;
        case EVENT_BUSY:        indtype = TONE_BUSY;        break;
        case EVENT_CONGEST:     indtype = TONE_CONGEST;     break;
        case EVENT_RADIO_ACK:   indtype = TONE_RADIO_ACK;   break;
        case EVENT_RADIO_NA:    indtype = TONE_RADIO_NA;    break;
        case EVENT_ERROR:       indtype = TONE_ERROR;       break;
        case EVENT_WAIT:        indtype = TONE_WAIT;        break;
        case EVENT_RING:        indtype = TONE_RING;        break;

        default:
            N_ERROR(LOG_CAT "%s(): invalid event %d", __FUNCTION__, event);
            return FALSE;
        }

        if (tone_sender[TONE_INDICATOR][0])
            N_DEBUG(LOG_CAT "%s(): got request to play the second indicator tone", __FUNCTION__);

        strncpy(tone_sender[TONE_INDICATOR], sender, DBUS_SENDER_MAXLEN);
        indicator_play(ausrv, indtype, volume, duration * 1000);
    }

    return TRUE;
}

static int stop_event_tone(DBusMessage *msg, struct tonegend *tonegend)
{
    struct ausrv *ausrv = tonegend->ausrv_ctx;
    uint32_t      event;
    int           success;

    success = dbus_message_get_args(msg, NULL,
                                    DBUS_TYPE_UINT32, &event,
                                    DBUS_TYPE_INVALID);

    if (!success) {
        N_ERROR(LOG_CAT "%s(): Can't parse arguments", __FUNCTION__);
        return FALSE;
    }

    N_DEBUG(LOG_CAT "%s(): stop %d tone", __FUNCTION__, event);

    if (event < DTMF_MAX) {
        dtmf_stop(ausrv);
        tone_sender[TONE_DTMF][0] = 0;
    } else {
        indicator_stop(ausrv, true);
        tone_sender[TONE_INDICATOR][0] = 0;
    }

    return TRUE;
}

static int stop_tone(DBusMessage *msg, struct tonegend *tonegend)
{
    struct ausrv *ausrv = tonegend->ausrv_ctx;
    char         *sender;

    (void)msg;

    sender = (char *)dbus_message_get_sender(msg);
    if (!strncmp(sender, tone_sender[TONE_DTMF], DBUS_SENDER_MAXLEN)) {
        N_DEBUG(LOG_CAT "%s(): stop DTMF tone", __FUNCTION__);
        dtmf_stop(ausrv);
        tone_sender[TONE_DTMF][0] = 0;
    } else if (!strncmp(sender, tone_sender[TONE_INDICATOR], DBUS_SENDER_MAXLEN)) {
        N_DEBUG(LOG_CAT "%s(): stop indicator tone", __FUNCTION__);
        indicator_stop(ausrv, true);
        tone_sender[TONE_INDICATOR][0] = 0;
    } else {
        /* In fallback the safest variant is to stop both type of streams */
        N_DEBUG(LOG_CAT "%s(): stop DTMF and/or indicator tones", __FUNCTION__);
        dtmf_stop(ausrv);
        indicator_stop(ausrv, true);
        tone_sender[TONE_DTMF][0] = 0;
        tone_sender[TONE_INDICATOR][0] = 0;
    }

    return TRUE;
}

/*
 * This function maps the RFC4733 defined
 * power level of 0dbm0 - -63dbm0
 * to the linear range of 0 - 100
 */
static uint32_t linear_volume(int dbm0)
{
    double volume;              /* volume on the scale 0-100 */

    if (dbm0 > 0)   dbm0 = 0;
    if (dbm0 < -63) dbm0 = -63;

    volume = pow(10.0, (double)(dbm0 + 63) / 20.0) / 14.125375446;

    return (uint32_t)(volume + 0.5);
}
