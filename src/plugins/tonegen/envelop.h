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

#ifndef __TONEGEND_ENVELOP_H__
#define __TONEGEND_ENVELOP_H__

#include <stdint.h>

#define ENVELOP_UNKNOWN      0
#define ENVELOP_RAMP_LINEAR  1

struct envelop_ramp_def {
    int32_t       k1;
    int32_t       k2;
    uint32_t      start;
    uint32_t      end;
};

struct envelop_ramp {
    int                     type; /* must be ENVELOP_RAMP_xxx */
    struct envelop_ramp_def up;   /* ramp-up */
    struct envelop_ramp_def down; /* ramp-down */
};

union envelop {
    int                  type;
    struct envelop_ramp  ramp;
};


int envelop_init(void);
union envelop *envelop_create(int type, uint32_t length, uint32_t start, uint32_t end);
void envelop_update(union envelop *envelop, uint32_t length, uint32_t end);
void envelop_destroy(union envelop *envelop);
int32_t envelop_apply(union envelop *envelop, int32_t in, uint32_t t);

#endif /* __TONEGEND_ENVELOP_H__ */
