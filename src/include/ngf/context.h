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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef N_CONTEXT_H
#define N_CONTEXT_H

/** Internal context structure. */
typedef struct _NContext NContext;

#include <ngf/value.h>

/** Context value change callback function */
typedef void (*NContextValueChangeFunc) (NContext *context,
                                         const char *key,
                                         const NValue *old_value,
                                         const NValue *new_value,
                                         void *userdata);

/**
 * Change or add key/value pair to context.
 *
 * @param context NContext structure.
 * @param key Key.
 * @param value Values as NValue type. Context takes over the ownership.
 */
void          n_context_set_value                (NContext *context, const char *key,
                                                  NValue *value);

/**
 * Get value by key from context.
 *
 * @param context NContext structure.
 * @param key Key.
 * @return Value as NValue or NULL if no value associated with key is found.
 */
const NValue* n_context_get_value                (NContext *context, const char *key);

/**
 * Subscribe callback function to key in context structure
 *
 * @param context NContext structure.
 * @param key Key.
 * @param callback Callback function.
 * @param userdata Userdata.
 * @return TRUE is successful.
 * @see NContextValueChangeFunc
 */
int           n_context_subscribe_value_change   (NContext *context, const char *key,
                                                  NContextValueChangeFunc callback,
                                                  void *userdata);

/**
 * Unsubscribe value change callback
 *
 * @param context NContext structure.
 * @param key Key.
 * @param callback Callback function, @see NContextValueChangeFunc
 */
void          n_context_unsubscribe_value_change (NContext *context, const char *key,
                                                  NContextValueChangeFunc callback);

#endif /* N_CONTEXT_H */
