
/*
 * ngfd - Non-graphic feedback daemon
 * Haptic feedback support functions
 *
 * Copyright (C) 2014 Jolla Ltd.
 * Contact: Thomas Perl <thomas.perl@jolla.com>
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

#ifndef N_NGF_HAPTIC_H
#define N_NGF_HAPTIC_H

#include <ngf/log.h>
#include <ngf/proplist.h>
#include <ngf/core.h>
#include <ngf/sinkinterface.h>
#include <ngf/inputinterface.h>

/**
 * The haptic.type key tells what kind of haptic event this is
 *
 * Each bracketed event definition in events.d config files that needs
 * haptic/vibra playback has to contain "haptic.type" line to define what kind
 * of haptic event it is.
 *
 * Supported type strings are currently:
 *   "event" - for most vibration effects
 *   "touch" - for events that only play when user touches touchscreen.
 *
 * The haptic event type is used to filter out playback in case user has
 * disabled the setting for certain type of haptic feedback.
 */
#define N_HAPTIC_TYPE_KEY         "haptic.type"

#define N_HAPTIC_TYPE_TOUCH       "touch"
#define N_HAPTIC_TYPE_EVENT       "event"

#define N_HAPTIC_EFFECT_KEY       "haptic.effect"

#define N_HAPTIC_EFFECT_DEFAULT   "default"

/* System-defined haptic effects. Preferably all plugins implementing
 * haptic functionality should be able to handle all the effects
 * listed here. Strictly speaking only mandatory one is "default".
 *
 * Effects are roughly listed by their invasiveness, top one on the
 * list is least invasive and last most invasive.
 *
 * alarm and ringtone effects should repeat indefinitely.
 */
#define N_HAPTIC_EFFECT_DRAG_START     "drag_start"
#define N_HAPTIC_EFFECT_RELEASE_WEAK   "release_weak"
#define N_HAPTIC_EFFECT_DRAG_FAIL      "drag_fail"
#define N_HAPTIC_EFFECT_DRAG_BOUNDARY  "drag_boundary"
#define N_HAPTIC_EFFECT_TOUCH_WEAK     "touch_weak"
#define N_HAPTIC_EFFECT_DRAG_END       "drag_end"
#define N_HAPTIC_EFFECT_RELEASE        "release"
#define N_HAPTIC_EFFECT_TOUCH          "touch"
#define N_HAPTIC_EFFECT_RELEASE_STRONG "release_strong"
#define N_HAPTIC_EFFECT_TOUCH_STRONG   "touch_strong"
#define N_HAPTIC_EFFECT_SHORT          "short"
#define N_HAPTIC_EFFECT_STRONG         "strong"
#define N_HAPTIC_EFFECT_LONG           "long"
#define N_HAPTIC_EFFECT_NOTICE         "notice"
#define N_HAPTIC_EFFECT_MESSAGE        "message"
#define N_HAPTIC_EFFECT_ATTENTION      "attention"
#define N_HAPTIC_EFFECT_ALARM          "alarm"
#define N_HAPTIC_EFFECT_RINGTONE       "ringtone"

/* Supported haptic classes */
#define N_HAPTIC_CLASS_UNDEFINED  (0)
#define N_HAPTIC_CLASS_TOUCH      (1)
#define N_HAPTIC_CLASS_EVENT      (2)

/**
 * Convenience function to filter haptic depending on settings and call state
 *
 * This function should be used by all haptic feedback plugins in their
 * _sink_can_handle() function to determine whether the event should be
 * played or not. It will take care of returning FALSE in case vibration
 * feedback is diabled or if phone calls are active. If it returns TRUE,
 * the plugin can do additional checks (e.g. whether or not it has an
 * effect for the request). The interface is kept in line with the
 * definition in NSinkInterfaceDecl, so in case the plugin does not have
 * to do any special casing, it can be used there directly.
 *
 * @param iface Pointer to a NSinkInterface
 * @param request Pointer to a NRequest
 * @return FALSE if the plugin should not handle this event, TRUE otherwise
 */
int n_haptic_can_handle (NSinkInterface *iface, NRequest *request);

/* Each haptic type belongs to a haptic class.
 *
 * Based on the haptic class the haptic event may be filtered away
 * depending on the currently active settings.
 *
 * @param haptic_type Haptic type string
 * @return Haptic class, or HAPTIC_CLASS_UNDEFINED if type doesn't have a class
 */
int n_haptic_class_for_type (const char *haptic_type);

/* Get value for haptic.effect from request.
 *
 * @param request NRequest struct
 * @return Haptic effect string or NULL if haptic.effect is not defined
 */
const char* n_haptic_effect_for_request (NRequest *request);

#endif /* N_NGF_HAPTIC_H */
