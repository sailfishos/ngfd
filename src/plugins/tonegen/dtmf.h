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

#ifndef __TONEGEND_DTMF_H__
#define __TONEGEND_DTMF_H__

#include <stdint.h>

typedef enum _dtmf_tone {
    DTMF_0        = 0,
    DTMF_1        = 1,
    DTMF_2        = 2,
    DTMF_3        = 3,
    DTMF_4        = 4,
    DTMF_5        = 5,
    DTMF_6        = 6,
    DTMF_7        = 7,
    DTMF_8        = 8,
    DTMF_9        = 9,
    DTMF_ASTERISK = 10,
    DTMF_HASHMARK = 11,
    DTMF_A        = 12,
    DTMF_B        = 13,
    DTMF_C        = 14,
    DTMF_D        = 15,
    DTMF_MAX      = 16
} dtmf_tone;

int  dtmf_init(void);
void dtmf_play(struct ausrv *ausrv, dtmf_tone tone,
               uint32_t volume, int duration, const char *extra_properties);
void dtmf_stop(struct ausrv *ausrv);
void dtmf_set_properties(char *propstring);
void dtmf_set_volume(uint32_t volume);
void dtmf_enable_mute_signal(gboolean enable);

#endif /* __TONEGEND_DTMF_H__ */
