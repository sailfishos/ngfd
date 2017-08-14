
/*
 * ngfd - Non-graphic feedback daemon
 * Haptic feedback support functions
 *
 * Copyright (C) 2014 Jolla Ltd.
 * Contact: Thomas Perl <thomas.perl@jolla.com>
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


#include <ngf/haptic.h>
#include <string.h>

#define LOG_CAT "haptic: "

#define CONTEXT_ALERT_ENABLED   "profile.current.vibrating.alert.enabled"
#define CONTEXT_VIBRA_LEVEL     "profile.current.touchscreen.vibration.level"
#define CONTEXT_CALL_STATE      "call_state.mode"

int
n_haptic_can_handle (NSinkInterface *iface, NRequest *request)
{
    NCore           *core = n_sink_interface_get_core (iface);
    NContext        *context = n_core_get_context (core);
    const NEvent    *event = n_request_get_event (request);
    const NProplist *props = n_request_get_properties (request);
    const NValue    *alert_enabled = NULL;
    const NValue    *touch_level = NULL;
    const NValue    *call_state = NULL;
    const char      *haptic_type = NULL;
    const char      *event_name = n_request_get_name (request);
    int              haptic_class;

    N_DEBUG (LOG_CAT "can handle %s?", event_name);

    if (!event) {
        N_ERROR (LOG_CAT "Invalid request!");
        return FALSE;
    }

    haptic_type = n_proplist_get_string (props, HAPTIC_TYPE_KEY);

    if (haptic_type == NULL) {
        N_DEBUG (LOG_CAT "No, haptic type not defined.");
        return FALSE;
    }

    call_state = n_context_get_value (context, CONTEXT_CALL_STATE);

    if (call_state == NULL)
        N_WARNING (LOG_CAT "No value for " CONTEXT_CALL_STATE "!");

    if (call_state && !strcmp (n_value_get_string (call_state), "active")) {
        N_DEBUG (LOG_CAT "No, should not vibrate during call.");
        return FALSE;
    }

    haptic_class = n_haptic_class_for_type (haptic_type);

    switch (haptic_class) {
        case HAPTIC_CLASS_TOUCH:
            touch_level = n_context_get_value (context, CONTEXT_VIBRA_LEVEL);

            if (touch_level == NULL)
                N_WARNING (LOG_CAT "No value for " CONTEXT_VIBRA_LEVEL "!");

            if (n_value_get_int (touch_level) == 0) {
                N_DEBUG (LOG_CAT "No, touch vibra level at 0.");
                return FALSE;
            }
            break;

        case HAPTIC_CLASS_ALARM:
            alert_enabled = n_context_get_value (context, CONTEXT_ALERT_ENABLED);

            if (alert_enabled == NULL)
                N_WARNING (LOG_CAT "No value for " CONTEXT_ALERT_ENABLED "!");

            if (!n_value_get_bool (alert_enabled)) {
                N_DEBUG (LOG_CAT "No, vibration disabled in profile.");
                return FALSE;
            }
            break;

        case HAPTIC_CLASS_UNDEFINED:
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
        return HAPTIC_CLASS_UNDEFINED;
    else if (!strcmp (haptic_type, HAPTIC_TYPE_TOUCH))
        return HAPTIC_CLASS_TOUCH;
    else if (!strcmp (haptic_type, HAPTIC_TYPE_SHORT))
        return HAPTIC_CLASS_TOUCH;
    else if (!strcmp (haptic_type, HAPTIC_TYPE_STRONG))
        return HAPTIC_CLASS_TOUCH;
    else if (!strcmp (haptic_type, HAPTIC_TYPE_ALARM))
        return HAPTIC_CLASS_ALARM;
    else if (!strcmp (haptic_type, HAPTIC_TYPE_NOTICE))
        return HAPTIC_CLASS_ALARM;
    else if (!strcmp (haptic_type, HAPTIC_TYPE_MESSAGE))
        return HAPTIC_CLASS_ALARM;
    else if (!strcmp (haptic_type, HAPTIC_TYPE_RINGTONE))
        return HAPTIC_CLASS_ALARM;

    return HAPTIC_CLASS_UNDEFINED;
}
