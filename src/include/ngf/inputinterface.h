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

#ifndef N_INPUT_INTERFACE_H
#define N_INPUT_INTERFACE_H

/** Internal inputinterface structure. */
typedef struct _NInputInterface NInputInterface;

#include <ngf/core.h>
#include <ngf/request.h>

/** Interface declaration structure. */
typedef struct _NInputInterfaceDecl
{
    /** Name of the interface. */
    const char *name;

    /** Initialization function. Called when input interface is loaded.
     * @param iface NInputInterface structure
     * @return TRUE if success
     */
    int  (*initialize) (NInputInterface *iface);
    
    /** Shutdown function. Called when input interface is removed.
     * @param iface NInputInterface structure
     */
    void (*shutdown)   (NInputInterface *iface);
    
    /** Error sending function. Called when error message needs to be sent to client
     * @param iface NInputInterface structure
     * @param request Request which error is related to
     * @param err_msg Error message
     */
    void (*send_error) (NInputInterface *iface, NRequest *request, const char *err_msg);
    
    /** Reply sending function. Called when reply to request needs to be sent to client
     * @param iface NInputInterface structure
     * @param request Request which reply is related to
     * @param ret_code Return code to request
     */
    void (*send_reply) (NInputInterface *iface, NRequest *request, int ret_code);
} NInputInterfaceDecl;

/** Get core to which interface is associated to
 * @param iface NInputInterface structure
 * @return NCore structure
 */
NCore* n_input_interface_get_core      (NInputInterface *iface);

/** Start playback of the request. Request can be paused or not yet started.
 * @param iface NInputInterface structure
 * @param request NRequest structure
 * @return TRUE if success
 */
int    n_input_interface_play_request  (NInputInterface *iface, NRequest *request);

/** Pauses playback of the request
 * @param iface NInputInterface structure
 * @param request NRequest structure
 * @return TRUE if success
 */
int    n_input_interface_pause_request (NInputInterface *iface, NRequest *request);

/** Stops playback of the request
 * @param iface NInputInterface structure
 * @param request NRequest structure
 * @param timeout Timeout in ms
 * @return TRUE if success
 */
void   n_input_interface_stop_request  (NInputInterface *iface, NRequest *request, guint timeout);

/** Stores userdata for the interface
 * @param iface NInputInterface structure
 * @param userdata Interface userdata to store
 */
void   n_input_interface_set_userdata (NInputInterface *iface, void *userdata);

/** Returns userdata stored to the interface
 * @param iface NInputInterface structure
 * @return Userdata for the interface
 */
void*  n_input_interface_get_userdata (NInputInterface *iface);

#endif /* N_INPUT_INTERFACE_H */
