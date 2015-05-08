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

#ifndef __TONEGEND_INDICATOR_H__
#define __TONEGEND_INDICATOR_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum _indicator_standard {
    STD_CEPT          = 0,
    STD_ANSI          = 1,
    STD_JAPAN         = 2,
    STD_ATNT          = 3
} indicator_standard;


int  indicator_init(void);
void indicator_play(struct ausrv *ausrv, int type, uint32_t volume, int duration);
void indicator_stop(struct ausrv *ausrv, bool kill_stream);
void indicator_set_standard(indicator_standard std);
void indicator_set_properties(char *propstring);
void indicator_set_volume(uint32_t volume);

#endif /* __TONEGEND_INDICATOR_H__ */
