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

#ifndef __TONEGEND_TONE_H__
#define __TONEGEND_TONE_H__

#include <stdint.h>
#include <stdbool.h>

/*
 * predefined tone types
 */
typedef enum _tone_type {
    TONE_UNDEFINED    = 0,
    TONE_DIAL         = 1,
    TONE_BUSY         = 2,
    TONE_CONGEST      = 3,
    TONE_RADIO_ACK    = 4,
    TONE_RADIO_NA     = 5,
    TONE_ERROR        = 6,
    TONE_WAIT         = 7,
    TONE_RING         = 8,
    TONE_DTMF_IND_L   = 9,
    TONE_DTMF_IND_H   = 10,
    TONE_DTMF_L       = 11,
    TONE_DTMF_H       = 12,
    TONE_NOTE_0       = 13,
    TONE_SINGEN_END   = 14,
    TONE_MAX          = 15
} tone_type;

#define BACKEND_UNKNOWN      0
#define BACKEND_SINGEN       1


struct stream;
union  envelop;

struct singen {
    int64_t        m;
    int64_t        n0;
    int64_t        n1;
    int64_t        offs;
};


struct tone {
    struct tone       *next;
    struct stream     *stream;
    struct tone       *chain;
    tone_type          type;
    uint32_t           period;   /* period (ie. play+pause) length */
    uint32_t           play;     /* how long to play the sine */
    uint64_t           start;
    uint64_t           end;
    int                backend;
    union {
        struct singen  singen;
    };
    bool               reltime; /* relative time to be passed to env. func's */
    union envelop     *envelop;
};


int tone_init(void);
struct tone *tone_create(struct stream *stream, tone_type type, uint32_t freq, uint32_t volume,
                         uint32_t period,uint32_t play, uint32_t start, uint32_t duration);
void tone_destroy(struct tone *tone, bool kill_chain);
bool tone_chainable(tone_type type);
uint32_t tone_write_callback(struct stream *stream, int16_t *buf, int length);
void tone_destroy_callback(void *data);


#endif /* __TONEGEND_TONE_H__ */
