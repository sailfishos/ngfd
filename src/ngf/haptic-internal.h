/*
 * ngfd - Non-graphic feedback daemon
 *
 * Copyright (C) 2017 Jolla Ltd
 * Contact: Juho Hämäläinen <juho.hamalainen@jolla.com>
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

#ifndef N_CORE_HAPTIC_INTERNAL_H_
#define N_CORE_HAPTIC_INTERNAL_H_

typedef struct NHaptic NHaptic;

NHaptic*    n_haptic_new   (NCore *core);
void        n_haptic_free  (NHaptic *haptic);

#endif
