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

#ifndef N_VALUE_H
#define N_VALUE_H

#include <glib.h>

#define N_VALUE_STR_STRING  "(string)"
#define N_VALUE_STR_INT     "(int)"
#define N_VALUE_STR_UINT    "(uint)"
#define N_VALUE_STR_BOOL    "(bool)"
#define N_VALUE_STR_POINTER "(pointer)"

/** NValue type enum. Used in n_value_type */
typedef enum
{
    N_VALUE_TYPE_STRING = 1,
    N_VALUE_TYPE_INT,
    N_VALUE_TYPE_UINT,
    N_VALUE_TYPE_BOOL,
    N_VALUE_TYPE_POINTER
} NValueType;

/** Internal NValue structure. */
typedef struct _NValue NValue;

/** Return new NValue
 * @return New NValue
 */
NValue*      n_value_new         ();

/** Free NValue
 * @param value NValue
 */
void         n_value_free        (NValue *value);

/** Re-init NValue. Current contents are emptied (but not freed).
 * @param value NValue
 */
void         n_value_init        (NValue *value);

/** Clean NValue. Current contents are freed.
 * @param value NValue
 */
void         n_value_clean       (NValue *value);

/** Copy NValue
 * @param value NValue
 * @return New NValue
 */
NValue*      n_value_copy        (const NValue *value);

/** Return type of contents of NValue
 * @param value NValue
 * @return Type as NValueType
 * @see NValueType
 */
int          n_value_type        (const NValue *value);

/** Compare two NValues
 * @param a NValue A
 * @param b NValue B
 * @return TRUE if NValues are identical
 */
gboolean     n_value_equals      (const NValue *a, const NValue *b);

/** Set string value to NValue
 * @param value NValue
 * @param in_value value
 */
void         n_value_set_string  (NValue *value, const char *in_value);

/** Get string value from NValue
 * @param value NValue
 * @return Value
 */
const gchar* n_value_get_string  (const NValue *value);

/** Return copy of string from NValue
 * @param value NValue
 * @return Newly allocated string. Must be freed after use.
 */
gchar*       n_value_dup_string  (const NValue *value);

/** Set int value to NValue
 * @param value NValue
 * @param in_value value
 */
void         n_value_set_int     (NValue *value, const gint in_value);

/** Get int value from NValue
 * @param value NValue
 * @return Value
 */
gint         n_value_get_int     (const NValue *value);

/** Set uint value to NValue
 * @param value NValue
 * @param in_value value
 */
void         n_value_set_uint    (NValue *value, const guint in_value);

/** Get uint value from NValue
 * @param value NValue
 * @return Value
 */
guint        n_value_get_uint    (const NValue *value);

/** Set boolean value to NValue
 * @param value NValue
 * @param in_value value
 */
void         n_value_set_bool    (NValue *value, const gboolean in_value);

/** Get boolean value from NValue
 * @param value NValue
 * @return Value
 */
gboolean     n_value_get_bool    (const NValue *value);

/** Set pointer to NValue
 * @param value NValue
 * @param in_value value
 */
void         n_value_set_pointer (NValue *value, const gpointer in_value);

/** Get pointer from NValue
 * @param value NValue
 * @return Value
 */
gpointer     n_value_get_pointer (const NValue *value);

/** Return string representation of contents
 * @param value NValue
 * @return Contents as string
 */
gchar*       n_value_to_string   (const NValue *value);

#endif /* N_VALUE_H */
