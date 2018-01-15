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

#ifndef N_SINK_INTERFACE_H
#define N_SINK_INTERFACE_H

/** Internal sinkinterface structure. */
typedef struct _NSinkInterface NSinkInterface;

#include <ngf/request.h>
#include <ngf/core.h>

#define N_SINK_INTERFACE_TYPE_AUDIO     "audio"
#define N_SINK_INTERFACE_TYPE_VIBRATOR  "vibra"
#define N_SINK_INTERFACE_TYPE_LEDS      "leds"

/** Interface declaration structure. */
typedef struct _NSinkInterfaceDecl
{
    /** Name of the interface. */
    const char *name;

    /** Type of the interface.
     * Common types are defined as macros. */
    const char *type;

    /** Initialization function. Called when interface is loaded.
     * @param iface NSinkInterface structure
     * @return TRUE if success
     */
    int  (*initialize) (NSinkInterface *iface);
    
    /** Shutdown function. Called when interface is removed.
     * @param iface NSinkInterface structure
     */
    void (*shutdown)   (NSinkInterface *iface);
    
    /** Can_handle function. This function is called to determinate can this interface handle requested event.
     * @param iface NSinkInterface structure
     * @param request Request to be handled
     * @return TRUE if interface can handle the request
     */
    int  (*can_handle) (NSinkInterface *iface, NRequest *request);
    
    /** Prepare function. This function is when interface is requested to ready itself to play the request.
     * @param iface NSinkInterface structure
     * @param request Request
     * @return TRUE if prepare succeeds
     */
    int  (*prepare)    (NSinkInterface *iface, NRequest *request);
    
    /** Play function. This function is when interface is requested to start playback of the request.
     * @param iface NSinkInterface structure
     * @param request Request
     * @return TRUE if playback is started
     */
    int  (*play)       (NSinkInterface *iface, NRequest *request);
    
    /** Pause function. This function is when interface is requested to pause playback of the request.
     * @param iface NSinkInterface structure
     * @param request Request
     * @return TRUE if playback is paused successfully
     */
    int  (*pause)      (NSinkInterface *iface, NRequest *request);
    
    /** Stop function. This function is when interface is requested to stop playback of the request.
     * @param iface NSinkInterface structure
     * @param request Request
     * @return TRUE if playback is stopped
     */
    void (*stop)       (NSinkInterface *iface, NRequest *request);
} NSinkInterfaceDecl;

/** Stores userdata for the sink interface
 * @param iface NSinkInterface structure
 * @param userdata Interface userdata to store
 */
void    n_sink_interface_set_userdata (NSinkInterface *iface, void *userdata);

/** Returns userdata stored to the sink interface
 * @param iface NSinkInterface structure
 * @return Userdata for the interface
 */
void*   n_sink_interface_get_userdata (NSinkInterface *iface);

/** Get core to which interface is associated to
 * @param iface NSinkInterface structure
 * @return NCore structure
 */
NCore*      n_sink_interface_get_core (NSinkInterface *iface);

/** Get interface name
 * @param iface NSinkInterface structure
 * @return Name of the interface
 */
const char* n_sink_interface_get_name (NSinkInterface *iface);

/** Get interface type
 * @param iface NSinkInterface structure
 * @return Type of the interface
 */
const char* n_sink_interface_get_type (NSinkInterface *iface);

/** Report that sink will resync to other sinks resynchronize requests.
 * @param iface NSinkInterface structure
 * @param request Request
 */
void n_sink_interface_set_resync_on_master (NSinkInterface *iface, NRequest *request);

/** Request resynchronization of other sinks. Ie. Event playback will restart.
 * @param iface NSinkInterface structure
 * @param request Request
 */
void n_sink_interface_resynchronize        (NSinkInterface *iface, NRequest *request);

/** Report sink is synchronized and ready to start the playback
 * @param iface NSinkInterface structure
 * @param request Request
 */
void n_sink_interface_synchronize          (NSinkInterface *iface, NRequest *request);

/**
 * Report sink has completed playback of request
 * @param iface NSinkInterface structure
 * @param request Request
 */
void n_sink_interface_complete             (NSinkInterface *iface, NRequest *request);

/**
 * Report sink has failed the request
 * @param iface NSinkInterface structure
 * @param request Request
 */
void n_sink_interface_fail                 (NSinkInterface *iface, NRequest *request);

#endif /* N_SINK_INTERFACE_H */
