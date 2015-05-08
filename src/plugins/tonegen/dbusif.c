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

static DBusHandlerResult handle_message(DBusConnection *,DBusMessage *,void *);
static gchar *create_key(const gchar *memb, const gchar *sign, const gchar *intf);
static void destroy_key(gpointer);


int dbusif_init(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    return 0;
}

void dbsuif_exit(void)
{

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

struct dbusif *dbusif_create_full(struct tonegend *tonegend)
{
    static struct DBusObjectPathVTable method = {
        .message_function = handle_message
    };

    struct dbusif   *dbusif = NULL;
    DBusError        err;
    int              ret;

    if (!(dbusif = dbusif_create(tonegend)))
        goto failed;

    if (!dbus_connection_register_object_path(dbusif->conn, TELEPHONY_TONES_PATH, &method, dbusif)) {
        N_ERROR(LOG_CAT "%s(): failed to register object path", __FUNCTION__);
        goto failed;
    }

    dbus_error_init(&err);
    ret = dbus_bus_request_name(dbusif->conn, TELEPHONY_TONES_SERVICE, DBUS_NAME_FLAG_REPLACE_EXISTING,
                                &err);
    if (ret < 0) {
        N_ERROR(LOG_CAT "%s(): request name failed: %s", __FUNCTION__, err.message);
        dbus_error_free(&err);
        goto failed;
    }
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        N_ERROR(LOG_CAT "%s(): not a primary owner", __FUNCTION__);
        goto failed;
    }

    dbusif->hash   = g_hash_table_new_full(g_str_hash, g_str_equal,
                                           destroy_key, NULL);
    N_DEBUG(LOG_CAT "D-Bus legacy API setup OK");

    return dbusif;

 failed:
    if (dbusif != NULL) {
        if (dbusif->conn)
            dbus_connection_unref(dbusif->conn);
        free(dbusif);
    }

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


int dbusif_register_input_method(struct tonegend *tonegend,
                                 const char *intf, const char *memb, const char *sig,
                                 int (*method)(DBusMessage*, struct tonegend*))
{
    struct dbusif *dbusif = tonegend->dbus_ctx;
    gchar         *key;

    if (!memb || !sig || !method) {
        N_ERROR(LOG_CAT "%s(): Called with invalid argument(s)", __FUNCTION__);
        errno = EINVAL;
        return -1;
    }

    if (intf == NULL)
        intf = TELEPHONY_TONES_SERVICE;

    key = create_key((const gchar *)memb, (const gchar *)sig, (const gchar *)intf);
    g_hash_table_insert(dbusif->hash, key, (gpointer)method);

    return 0;
}


int dbusif_unregister_input_method(struct tonegend *tonegend, const char *intf,
                                   const char *memb, const char *sign)
{
    (void)tonegend;
    (void)intf;
    (void)memb;
    (void)sign;

    return 0;
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


static DBusHandlerResult handle_message(DBusConnection *conn,
                                        DBusMessage    *msg,
                                        void           *user_data)
{
    struct dbusif   *dbusif = (struct dbusif *)user_data;
    struct tonegend *tonegend = dbusif->tonegend;
    DBusMessage     *reply  = NULL;
    uint32_t         ser;
    int            (*method)(DBusMessage *, struct tonegend *);
    const char      *intf;
    const char      *memb;
    const char      *sig;
    gchar           *key;
    const char      *errname;
    char             errdesc[256];
    int              success;

    (void)conn;

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
        TRACE("%s(): ignoring non method_call's", __FUNCTION__);
    else {
        intf = dbus_message_get_interface(msg);
        memb = dbus_message_get_member(msg);
        sig  = dbus_message_get_signature(msg);
        ser  = dbus_message_get_serial(msg);

        TRACE("%s(): message no #%u received: '%s', '%s', '%s'",
              __FUNCTION__, ser, intf, memb, sig);

        key = create_key((const gchar *)memb, (const gchar *)sig, (const gchar *)intf);
        method = g_hash_table_lookup(dbusif->hash, key);
        g_free(key);

        success = method ? method(msg, tonegend) : FALSE;

        if (success)
            reply = dbus_message_new_method_return(msg);
        else {
            if (method) {
                errname = DBUS_ERROR_FAILED;
                snprintf(errdesc, sizeof(errdesc), "Internal error");
            }
            else {
                errname = DBUS_ERROR_NOT_SUPPORTED;
                snprintf(errdesc, sizeof(errdesc), "Method '%s(%s)' "
                         "not supported", memb, sig);
            }
            reply = dbus_message_new_error(msg, errname, errdesc);
        }

        dbus_message_set_reply_serial(msg, ser);

        if (!dbus_connection_send(dbusif->conn, reply, NULL))
            N_ERROR(LOG_CAT "%s(): D-Bus message reply failure", __FUNCTION__);
        else
            TRACE("%s(): message no #%u replied", __FUNCTION__, ser);

        dbus_message_unref(reply);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static gchar *create_key(const gchar *memb, const gchar *sign, const gchar *intf)
{
    gchar *key = NULL;

    if (memb == NULL) memb = "";
    if (sign == NULL) sign = "";
    if (intf == NULL) intf = "";

    key = g_strjoin("__", memb, sign, intf, NULL);

    return key;
}


static void destroy_key(gpointer key)
{
    g_free(key);
}
