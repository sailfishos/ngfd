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

#ifndef N_PROPLIST_H
#define N_PROPLIST_H

/** Internal proplist structure. */
typedef struct _NProplist NProplist;

#include <ngf/value.h>

/** Proplist manipulation function definition. Used in n_proplist_foreach
 * @param key Proplist key
 * @param value Value associated with key
 * @param userdata Userdata
 */
typedef void (*NProplistFunc) (const char *key, const NValue *value, gpointer userdata);

/** Initializes new proplist
 * @return Empty proplist
 */
NProplist*  n_proplist_new         ();

/** Create copy of existing proplist
 * @param source Source proplist
 * @return Copy of source proplist
 */
NProplist*  n_proplist_copy        (const NProplist *source);

/** Create copy of existing proplist, copying only selected keys.
 * @param source Source proplist
 * @param keys Keys to be copied as GList
 * @return Copy of source proplist containing only selected keys
 */
NProplist*  n_proplist_copy_keys   (const NProplist *source, GList *keys);

/** Merge two proplists
 * @param target Target proplist
 * @param source Source to be merged to target
 */
void        n_proplist_merge       (NProplist *target, const NProplist *source);

/** Merge only selected keys on proplists
 * @param target Target proplist
 * @param source Source to be merged to target
 * @param keys List of keys to be merged as GList
 */
void        n_proplist_merge_keys  (NProplist *target, const NProplist *source, GList *keys);

/** Free proplist
 * @param proplist Proplist
 */
void        n_proplist_free        (NProplist *proplist);

/** Return number of keys in the proplist
 * @param proplist Proplist
 * @return Number of keys
 */
int         n_proplist_size        (const NProplist *proplist);

/** Run function on each key in the proplist
 * @param proplist Target proplist
 * @param func Function to be run
 * @param userdata Userdata
 */
void        n_proplist_foreach     (const NProplist *proplist, NProplistFunc func, gpointer userdata);

/** Check if the proplist is empty
 * @param proplist Proplist
 * @return TRUE if proplist is empty
 */
gboolean    n_proplist_is_empty    (const NProplist *proplist);

/** Check if the proplist has key
 * @param proplist Proplist
 * @param key Key
 * @return TRUE if proplist has key
 */
gboolean    n_proplist_has_key     (const NProplist *proplist, const char *key);

/** Check if two proplists are identical
 * @param a Proplist A
 * @param b Proplist B
 * @return TRUE if proplists are identical
 */
gboolean    n_proplist_match_exact (const NProplist *a, const NProplist *b);

/** Insert or update key/value pair in proplist
 * @param proplist Proplist
 * @param key Key
 * @param value Value. Proplist takes ownership of the value.
 */
void        n_proplist_set         (NProplist *proplist, const char *key, NValue *value);

/** Get value from proplist
 * @param proplist Proplist
 * @param key Key
 * @return Value of the key as NValue or NULL if empty
 */
NValue*     n_proplist_get         (const NProplist *proplist, const char *key);

/* helpers */

/** Remove key from proplist
 * @param proplist Proplist
 * @param key Key
 */
void        n_proplist_unset       (NProplist *proplist, const char *key);

/** Set or update string value in proplist
 * @param proplist Proplist
 * @param key Key
 * @param value Value
 */
void        n_proplist_set_string  (NProplist *proplist, const char *key, const char *value);

/** Get string value from proplist
 * @param proplist Proplist
 * @param key Key
 * @return Value or NULL if key is not found
 */
const char* n_proplist_get_string  (const NProplist *proplist, const char *key);

/** Get duplicate of string value from proplist
 * @param proplist Proplist
 * @param key Key
 * @return Newly allocated string of value or NULL if key is not found. Value must be freed afterwards.
 */
gchar*      n_proplist_dup_string  (const NProplist *proplist, const char *key);

/** Set or update int value in proplist
 * @param proplist Proplist
 * @param key Key
 * @param value Value
 */
void        n_proplist_set_int     (NProplist *proplist, const char *key, gint value);

/** Get int value from proplist
 * @param proplist Proplist
 * @param key Key
 * @return Value or 0 if key is not found
 */
gint        n_proplist_get_int     (const NProplist *proplist, const char *key);

/** Set or update uint value in proplist
 * @param proplist Proplist
 * @param key Key
 * @param value Value
 */
void        n_proplist_set_uint    (NProplist *proplist, const char *key, guint value);

/** Get uint value from proplist
 * @param proplist Proplist
 * @param key Key
 * @return Value or 0 if key is not found
 */
guint       n_proplist_get_uint    (const NProplist *proplist, const char *key);

/** Set or update boolean value in proplist
 * @param proplist Proplist
 * @param key Key
 * @param value Value
 */
void        n_proplist_set_bool    (NProplist *proplist, const char *key, gboolean value);

/** Get boolean value from proplist
 * @param proplist Proplist
 * @param key Key
 * @return Value or FALSE if key is not found
 */
gboolean    n_proplist_get_bool    (const NProplist *proplist, const char *key);

/** Set or update pointer value in proplist
 * @param proplist Proplist
 * @param key Key
 * @param value Value
 */
void        n_proplist_set_pointer (NProplist *proplist, const char *key, gpointer value);

/** Get pointer value from proplist
 * @param proplist Proplist
 * @param key Key
 * @return Value or FALSE if key is not found
 */
gpointer    n_proplist_get_pointer (const NProplist *proplist, const char *key);

/** Dump contents of proplist to debug log
 * @param proplist Proplist
 * @see log.h
 */
void        n_proplist_dump        (const NProplist *proplist);

#endif /* N_PROPLIST_H */
