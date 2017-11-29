/*
 * ngfd - Non-graphic feedback daemon
 * Haptic feedback support functions
 *
 * Copyright (C) 2014,2017 Jolla Ltd.
 * Contact: Juho Hämäläinen <juho.hamalainen@jolla.com>
 *
 * Based on code from the ffmemless plugin:
 * Copyright (C) 2013 Jolla Oy.
 * Contact: Kalle Jokiniemi <kalle.jokiniemi@jollamobile.com>
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


#include <string.h>
#include <ngf/haptic.h>
#include "core-internal.h"
#include "haptic-internal.h"

#define LOG_CAT "haptic: "

#define CONTEXT_ALERT_ENABLED   "profile.current.vibrating.alert.enabled"
#define CONTEXT_VIBRA_LEVEL     "profile.current.touchscreen.vibration.level"
#define CONTEXT_CALL_STATE      "call_state.mode"

struct NHaptic {
    NCore      *core;
    gboolean    call_active;
    int         vibra_level;
    gboolean    alert_enabled;
};

static void
call_state_changed_cb (NContext *context,
                       const char *key,
                       const NValue *old_value,
                       const NValue *new_value,
                       void *userdata)
{
    NHaptic    *haptic = userdata;
    const char *state;

    (void) context;
    (void) key;
    (void) old_value;

    if ((state = n_value_get_string (new_value)) &&
        !strcmp (state, "active"))
        haptic->call_active = TRUE;
    else
        haptic->call_active = FALSE;
}

static void
vibra_level_changed_cb (NContext *context,
                        const char *key,
                        const NValue *old_value,
                        const NValue *new_value,
                        void *userdata)
{
    NHaptic *haptic = userdata;

    (void) context;
    (void) key;
    (void) old_value;

    haptic->vibra_level = n_value_get_int (new_value);
}

static void
alert_enabled_changed_cb (NContext *context,
                          const char *key,
                          const NValue *old_value,
                          const NValue *new_value,
                          void *userdata)
{
    NHaptic *haptic = userdata;

    (void) context;
    (void) key;
    (void) old_value;

    haptic->alert_enabled = n_value_get_bool (new_value);
}

NHaptic*
n_haptic_new (NCore *core)
{
    NHaptic      *haptic;
    NContext     *context;
    const NValue *alert_enabled;
    const NValue *vibra_level;
    const NValue *call_state;
    const char   *call_state_str;

    haptic = g_new0 (NHaptic, 1);
    haptic->core = core;
    context = n_core_get_context (core);

    n_context_subscribe_value_change (context, CONTEXT_CALL_STATE, call_state_changed_cb, haptic);
    n_context_subscribe_value_change (context, CONTEXT_VIBRA_LEVEL, vibra_level_changed_cb, haptic);
    n_context_subscribe_value_change (context, CONTEXT_ALERT_ENABLED, alert_enabled_changed_cb, haptic);

    if ((call_state = n_context_get_value (context, CONTEXT_CALL_STATE))) {
        if ((call_state_str = n_value_get_string (call_state)) &&
            !strcmp (call_state_str, "active"))
            haptic->call_active = TRUE;
    }

    if ((vibra_level = n_context_get_value (context, CONTEXT_VIBRA_LEVEL)))
        haptic->vibra_level = n_value_get_int (vibra_level);

    if ((alert_enabled = n_context_get_value (context, CONTEXT_ALERT_ENABLED)))
        haptic->alert_enabled = n_value_get_bool (alert_enabled);

    return haptic;
}

void
n_haptic_free (NHaptic *haptic)
{
    NContext *context;

    context = n_core_get_context (haptic->core);

    n_context_unsubscribe_value_change (context, CONTEXT_CALL_STATE, call_state_changed_cb);
    n_context_unsubscribe_value_change (context, CONTEXT_VIBRA_LEVEL, vibra_level_changed_cb);
    n_context_unsubscribe_value_change (context, CONTEXT_ALERT_ENABLED, alert_enabled_changed_cb);

    g_free (haptic);
}

int
n_haptic_can_handle (NSinkInterface *iface, NRequest *request)
{
    NCore           *core = n_sink_interface_get_core (iface);
    NHaptic         *haptic = core->haptic;
    const NEvent    *event = n_request_get_event (request);
    const NProplist *props = n_request_get_properties (request);
    const char      *haptic_type = NULL;
    const char      *event_name = n_request_get_name (request);
    int              haptic_class;

    N_DEBUG (LOG_CAT "can handle %s?", event_name);

    if (!event) {
        N_ERROR (LOG_CAT "Invalid request!");
        return FALSE;
    }

    haptic_type = n_proplist_get_string (props, N_HAPTIC_TYPE_KEY);

    if (haptic_type == NULL) {
        N_DEBUG (LOG_CAT "No, haptic type not defined.");
        return FALSE;
    }

    if (haptic->call_active) {
        N_DEBUG (LOG_CAT "No, should not vibrate during call.");
        return FALSE;
    }

    haptic_class = n_haptic_class_for_type (haptic_type);

    switch (haptic_class) {
        case N_HAPTIC_CLASS_TOUCH:
            if (haptic->vibra_level == 0) {
                N_DEBUG (LOG_CAT "No, touch vibra level at 0.");
                return FALSE;
            }
            break;

        case N_HAPTIC_CLASS_EVENT:
            if (!haptic->alert_enabled) {
                N_DEBUG (LOG_CAT "No, vibration disabled in profile.");
                return FALSE;
            }
            break;

        case N_HAPTIC_CLASS_UNDEFINED:
            /* fall through */
        default:
            N_DEBUG (LOG_CAT "No, unknown haptic type.");
            return FALSE;
    }

    return TRUE;
}

/* Hard-coded here for now, if more flexible setup is needed
 * implement something to ini files. */
int
n_haptic_class_for_type (const char *haptic_type)
{
    if (!haptic_type)
        return N_HAPTIC_CLASS_UNDEFINED;
    else if (!strcmp (haptic_type, N_HAPTIC_TYPE_TOUCH))
        return N_HAPTIC_CLASS_TOUCH;
    else if (!strcmp (haptic_type, N_HAPTIC_TYPE_EVENT))
        return N_HAPTIC_CLASS_EVENT;

    return N_HAPTIC_CLASS_UNDEFINED;
}

const char*
n_haptic_effect_for_request (NRequest *request)
{
    const NProplist *props;

    g_assert (request);
    props = n_request_get_properties (request);

    return n_proplist_get_string (props, N_HAPTIC_EFFECT_KEY);
}
