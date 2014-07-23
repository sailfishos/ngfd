
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


int
n_haptic_can_handle (NSinkInterface *iface, NRequest *request)
{
    NCore           *core = n_sink_interface_get_core (iface);
    NContext        *context = n_core_get_context (core);
    const NProplist *props = n_request_get_properties (request);
    NValue          *enabled = NULL;
    NValue          *touch_level = NULL;
    NValue          *call_state = NULL;
    const char      *haptic_type = NULL;
    const char      *event_name = n_request_get_name (request);

    N_DEBUG (LOG_CAT "can handle %s?", event_name);

    enabled = (NValue*) n_context_get_value (context,
            "profile.current.vibrating.alert.enabled");
    touch_level = (NValue*) n_context_get_value (context,
            "profile.current.touchscreen.vibration.level");

    haptic_type =  n_proplist_get_string (props,
            HAPTIC_TYPE_KEY);

    call_state = (NValue*) n_context_get_value (context,
            "call_state.mode");

    if (touch_level == NULL) {
        N_WARNING (LOG_CAT "No value for touchscreen.vibration.level!");
    }

    if (call_state == NULL) {
        N_WARNING (LOG_CAT "Call state not available!");
    }

    if (haptic_type == NULL) {
        N_DEBUG (LOG_CAT "Haptic type not defined for %s", event_name);
    }

    if (!enabled || !n_value_get_bool (enabled)) {
        N_DEBUG (LOG_CAT "no, vibration disabled in profile");
        return FALSE;
    }

    if (call_state && !strcmp (n_value_get_string (call_state), "active")) {
        N_DEBUG (LOG_CAT "no, should not vibrate during call");
        return FALSE;
    }

    if (haptic_type &&
                !strcmp (haptic_type, HAPTIC_TYPE_TOUCH) &&
                n_value_get_int (touch_level) == 0) {
        N_DEBUG (LOG_CAT "No, touch vibra level at 0, skipping vibra");
        return FALSE;
    }

    return TRUE;
}
