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

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdbool.h>

#include <ngf/request.h>
#include <ngf/proplist.h>
#include <ngf/log.h>
#include <trace/trace.h>

#include "tonegend.h"
#include "dbusif.h"

#define LOG_CAT "tonegen-dbusif: "

int dbusif_init(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    return 0;
}

struct dbusif *dbusif_create(struct tonegend *tonegend)
{
    struct dbusif   *dbusif = NULL;
    DBusConnection  *conn   = NULL;
    DBusError        err;

    if ((dbusif = (struct dbusif *) calloc(1, sizeof(struct dbusif))) == NULL) {
        N_ERROR(LOG_CAT "%s(): Can't allocate memory", __FUNCTION__);
        goto failed;
    }

    dbus_error_init(&err);

    if ((conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) == NULL) {
        N_ERROR(LOG_CAT "%s(): Can't connect to D-Bus daemon: %s",
                  __FUNCTION__, err.message);
        dbus_error_free(&err);
        goto failed;
    }

    /*
     * The following will make us zombie if the system bus goes down.
     * However, for 'clean' shutdown operation it is useful, ie. the
     * shutdown sequence should not assure that we go before D-Bus go
     */
    dbus_connection_set_exit_on_disconnect(conn, FALSE);

    dbus_connection_setup_with_g_main(conn, NULL);

    dbusif->tonegend = tonegend;
    dbusif->conn   = conn;

    N_DEBUG(LOG_CAT "D-Bus setup OK");

    return dbusif;

 failed:
    if (dbusif != NULL)
        free(dbusif);

    return NULL;
}

void dbusif_destroy(struct dbusif *dbusif)
{
    if (dbusif) {

        if (dbusif->hash != NULL)
            g_hash_table_destroy(dbusif->hash);

        free(dbusif);
    }
}

int dbusif_send_signal(struct tonegend *tonegend, const char *intf, const char *name,
                       int first_arg_type, ...)
{
    struct dbusif *dbusif = tonegend->dbus_ctx;
    DBusMessage   *msg;
    va_list        ap;
    dbus_bool_t    success = FALSE;

    if (name == NULL) {
        N_ERROR(LOG_CAT "%s(): Called with invalid argument", __FUNCTION__);
        errno   = EINVAL;
        return -1;
    }

    if (intf == NULL)
        intf = TELEPHONY_TONES_SERVICE;

    if ((msg = dbus_message_new_signal(TELEPHONY_TONES_PATH, intf, name)) == NULL) {
        errno = ENOMEM;
        return -1;
    }

    va_start(ap, first_arg_type);

    if (dbus_message_append_args_valist(msg, first_arg_type, ap)) {
        success = dbus_connection_send(dbusif->conn, msg, NULL);
    }

    va_end(ap);

    dbus_message_unref(msg);

    return success ? 0 : -1;
}
