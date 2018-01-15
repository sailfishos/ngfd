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

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <sys/types.h>
#include <dirent.h>

#include <ngf/log.h>
#include <ngf/core-dbus.h>
#include "core-internal.h"
#include "event-internal.h"
#include "request-internal.h"
#include "context-internal.h"
#include "core-dbus-internal.h"
#include "haptic-internal.h"

#define LOG_CAT  "core: "

#define DEFAULT_CONF_PATH     "/usr/share/ngfd"
#define DEFAULT_PLUGIN_PATH   "/usr/lib/ngf"
#define DEFAULT_CONF_FILENAME "ngfd.ini"
#define PLUGIN_CONF_PATH      "plugins.d"
#define EVENT_CONF_PATH       "events.d"

#define CORE_CONF_KEYTYPES    "keytypes"

typedef struct _NEventMatchResult
{
    NRequest *request;
    NContext *context;
    gboolean  has_match;
    gboolean  skip_rest;
} NEventMatchResult;

static gchar*     n_core_get_path               (const char *key, const char *default_path);
static NProplist* n_core_load_params            (NCore *core, const char *plugin_name);
static NPlugin*   n_core_open_plugin            (NCore *core, const char *plugin_name);
static int        n_core_init_plugin            (NPlugin *plugin, gboolean required);
static void       n_core_unload_plugin          (NCore *core, NPlugin *plugin);
static void       n_core_free_event_list_cb     (gpointer in_key, gpointer in_data, gpointer userdata);
static gint       n_core_sort_event_cb          (gconstpointer a, gconstpointer b);
static void       n_core_dump_value_cb          (const char *key, const NValue *value, gpointer userdata);
static void       n_core_parse_events_from_file (NCore *core, const char *filename);
static int        n_core_parse_events           (NCore *core);
static void       n_core_parse_keytypes         (NCore *core, GKeyFile *keyfile);
static void       n_core_parse_sink_order       (NCore *core, GKeyFile *keyfile);
static int        n_core_parse_configuration    (NCore *core);
static void       n_core_match_event_rule_cb    (const char *key, const NValue *value, gpointer userdata);

static GSList*    tmp_plugin_conf_files;


static gchar*
n_core_get_path (const char *key, const char *default_path)
{
    g_assert (default_path != NULL);

    const char *env_path = NULL;
    const char *source   = NULL;

    source = default_path;
    if (key && (env_path = getenv (key)) != NULL)
        source = env_path;

    return g_strdup (source);
}

static GSList*
n_core_conf_files_from_path (const char *base_path, const char *path)
{
    GSList         *conf_files  = NULL;
    gchar          *conf_path   = NULL;
    GDir           *conf_dir    = NULL;
    const gchar    *filename    = NULL;
    gchar          *full_name   = NULL;
    GError         *error       = NULL;

    conf_path = g_build_path (G_DIR_SEPARATOR_S, base_path, path, NULL);
    conf_dir  = g_dir_open (conf_path, 0, &error);

    if (!conf_dir) {
        N_WARNING (LOG_CAT "could not open configuration dir '%s': %s",
            conf_path, error->message);
        g_error_free (error);
        return NULL;
    }

    while ((filename = g_dir_read_name (conf_dir))) {
        if (!g_str_has_suffix (filename, ".ini"))
            continue;

        full_name = g_build_filename (conf_path, filename, NULL);

        if (g_file_test (full_name, G_FILE_TEST_IS_REGULAR))
            conf_files = g_slist_append (conf_files, full_name);
        else
            g_free (full_name);
    }

    if (conf_files)
        conf_files = g_slist_sort (conf_files, (GCompareFunc) g_strcmp0);

    g_free (conf_path);
    g_dir_close (conf_dir);

    return conf_files;
}

static GSList*
n_core_plugin_conf_files (NCore *core)
{
    if (tmp_plugin_conf_files)
        return tmp_plugin_conf_files;

    tmp_plugin_conf_files = n_core_conf_files_from_path (core->conf_path, PLUGIN_CONF_PATH);

    return tmp_plugin_conf_files;
}

static void
n_core_plugin_conf_files_done ()
{
    if (tmp_plugin_conf_files) {
        g_slist_free_full (tmp_plugin_conf_files, g_free);
        tmp_plugin_conf_files = NULL;
    }
}

/* Returns new GSList with filenames for plugin_name. The list should be freed
 * after use, but the list element data not. */
static GSList*
n_core_plugin_conf_files_for_plugin (NCore *core, const char *plugin_name)
{
    GSList *plugin = NULL;
    GSList *i      = NULL;
    gchar  *suffix = NULL;

    if (n_core_plugin_conf_files (core)) {
        suffix = g_strdup_printf ("%s.ini", plugin_name);

        for (i = n_core_plugin_conf_files (core); i; i = g_slist_next (i)) {
            if (g_str_has_suffix (i->data, suffix))
                plugin = g_slist_append (plugin, i->data);
        }

        g_free (suffix);
    }

    return plugin;
}

static NProplist*
n_core_load_params (NCore *core, const char *plugin_name)
{
    g_assert (core != NULL);
    g_assert (plugin_name != NULL);

    NProplist      *proplist    = NULL;
    GKeyFile       *keyfile     = NULL;
    gchar         **keys        = NULL;
    gchar         **iter        = NULL;
    GError         *error       = NULL;
    gchar          *value       = NULL;
    GSList         *plugin_conf = NULL;
    GSList         *i           = NULL;
    const gchar    *filename    = NULL;

    proplist = n_proplist_new ();
    keyfile = g_key_file_new ();
    plugin_conf = n_core_plugin_conf_files_for_plugin (core, plugin_name);

    for (i = plugin_conf; i; i = g_slist_next (i)) {
        filename = (const gchar*) i->data;

        if (!g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, &error)) {
            N_WARNING (LOG_CAT "problem with configuration file '%s': %s",
                filename, error->message);
            g_error_free (error);
            continue;
        }

        keys = g_key_file_get_keys (keyfile, plugin_name, NULL, NULL);
        if (!keys) {
            N_WARNING (LOG_CAT "no group '%s' within configuration file '%s'",
                plugin_name, filename);
            continue;
        }

        /* Extend known keytypes from plugin configuration. */
        n_core_parse_keytypes (core, keyfile);

        for (iter = keys; *iter; ++iter) {
            if ((value = g_key_file_get_string (keyfile, plugin_name, *iter, NULL)) == NULL)
                continue;

            N_DEBUG (LOG_CAT "+ plugin parameter (%s): %s = %s%s",
                plugin_name, *iter, value,
                n_proplist_has_key (proplist, *iter) ? " (override previous)" : "");
            n_proplist_set_string (proplist, *iter, value);
            g_free (value);
        }

        g_strfreev (keys);
        g_key_file_remove_group (keyfile, plugin_name, NULL);
        g_key_file_remove_group (keyfile, CORE_CONF_KEYTYPES, NULL);
    }

    g_key_file_free (keyfile);
    /* Only remove the list, not the element data. */
    g_slist_free (plugin_conf);

    return proplist;
}

static NPlugin*
n_core_open_plugin (NCore *core, const char *plugin_name)
{
    g_assert (core != NULL);
    g_assert (plugin_name != NULL);

    NPlugin *plugin    = NULL;
    gchar   *filename  = NULL;
    gchar   *full_path = NULL;

    filename  = g_strdup_printf ("libngfd_%s.so", plugin_name);
    full_path = g_build_filename (core->plugin_path, filename, NULL);

    if (!(plugin = n_plugin_open (full_path)))
        goto done;

    plugin->core   = core;
    plugin->params = n_core_load_params (core, plugin_name);

    N_DEBUG (LOG_CAT "opened plugin '%s' (%s)", plugin->get_name (), filename);

    g_free (full_path);
    g_free (filename);

    return plugin;

done:
    N_ERROR (LOG_CAT "unable to open plugin '%s'", plugin_name);

    if (plugin)
        n_plugin_unload (plugin);

    g_free (full_path);
    g_free (filename);

    return NULL;
}

static int
n_core_init_plugin (NPlugin *plugin, gboolean required)
{
    int ret = FALSE;

    g_assert (plugin != NULL);

    if (!(ret = n_plugin_init (plugin))) {
        if (required)
            N_ERROR (LOG_CAT "unable to init required plugin '%s'", plugin->get_name());
        else
            N_INFO (LOG_CAT "unable to init optional plugin '%s'", plugin->get_name());

        n_plugin_unload (plugin);
    }

    return ret;
}

static void
n_core_unload_plugin (NCore *core, NPlugin *plugin)
{
    g_assert (core != NULL);
    g_assert (plugin != NULL);

    N_DEBUG (LOG_CAT "unloading plugin '%s'", plugin->get_name ());
    plugin->unload (plugin);
    n_plugin_unload (plugin);
}

NCore*
n_core_new (int *argc, char **argv)
{
    NCore *core = NULL;

    (void) argc;
    (void) argv;

    core = g_new0 (NCore, 1);

    /* query the default paths */

    core->conf_path   = n_core_get_path ("NGF_CONF_PATH", DEFAULT_CONF_PATH);
    core->plugin_path = n_core_get_path ("NGF_PLUGIN_PATH", DEFAULT_PLUGIN_PATH);
    core->context     = n_context_new ();
    core->dbus        = n_dbus_helper_new (core);
    core->haptic      = n_haptic_new (core);

    core->event_table = g_hash_table_new_full (g_str_hash, g_str_equal,
        g_free, NULL);

    core->key_types = g_hash_table_new_full (g_str_hash, g_str_equal,
        g_free, NULL);

    return core;
}

static void
n_core_free_event_list_cb (gpointer in_key, gpointer in_data, gpointer userdata)
{
    (void) in_key;
    (void) userdata;

    GList      *event_list = (GList*) in_data;
    GList      *iter       = NULL;
    NEvent     *event      = NULL;

    for (iter = g_list_first (event_list); iter; iter = g_list_next (iter)) {
        event = (NEvent*) iter->data;
        n_event_free (event);
    }

    g_list_free (event_list);
}

void
n_core_free (NCore *core)
{
    g_assert (core != NULL);

    if (!core->shutdown_done)
        n_core_shutdown (core);

    if (core->sink_order) {
        g_list_foreach (core->sink_order, (GFunc) g_free, NULL);
        g_list_free (core->sink_order);
    }

    g_hash_table_destroy (core->key_types);

    g_list_free          (core->event_list);
    g_hash_table_foreach (core->event_table, n_core_free_event_list_cb, NULL);
    g_hash_table_destroy (core->event_table);

    n_haptic_free (core->haptic);
    n_dbus_helper_free (core->dbus);
    n_context_free (core->context);
    g_free (core->plugin_path);
    g_free (core->conf_path);
    g_free (core);
}

static void
n_core_set_sink_priorities (NSinkInterface **sink_list, GList *sink_order)
{
    NSinkInterface **sink = NULL;
    GList           *list = NULL;
    GList           *iter = NULL;
    const char      *name = NULL;
    int              prio = 0;

    list = g_list_copy (sink_order);
    list = g_list_reverse (list);

    for (iter = g_list_first (list); iter; iter = g_list_next (iter)) {
        name = (const char*) iter->data;

        for (sink = sink_list; *sink; ++sink) {
            if (g_str_equal ((*sink)->name, name)) {
                N_DEBUG (LOG_CAT "sink '%s' priority set to %d",
                    (*sink)->name, prio);

                (*sink)->priority = prio;
                ++prio;
                break;
            }
        }
    }

    g_list_free (list);
}

int
n_core_initialize (NCore *core)
{
    g_assert (core != NULL);
    g_assert (core->conf_path != NULL);
    g_assert (core->plugin_path != NULL);

    GList            *required_plugins = NULL;
    GList            *optional_plugins = NULL;
    NSinkInterface  **sink   = NULL;
    NInputInterface **input  = NULL;
    NPlugin          *plugin = NULL;
    GList            *p      = NULL;

    tmp_plugin_conf_files    = NULL;

    /* setup hooks */

    n_hook_init (&core->hooks[N_CORE_HOOK_INIT_DONE]);
    n_hook_init (&core->hooks[N_CORE_HOOK_TRANSFORM_PROPERTIES]);
    n_hook_init (&core->hooks[N_CORE_HOOK_FILTER_SINKS]);

    /* load the default configuration. */

    if (!n_core_parse_configuration (core))
        goto failed_init;

    /* check for required plugins. */

    if (!core->required_plugins && !core->optional_plugins) {
        N_ERROR (LOG_CAT "no plugins to load defined in configuration");
        goto failed_init;
    }

    /* load all plugins */

    /* first mandatory plugins */
    for (p = g_list_first (core->required_plugins); p; p = g_list_next (p)) {
        if (!(plugin = n_core_open_plugin (core, (const char*) p->data)))
            goto failed_init;

        required_plugins = g_list_append (required_plugins, plugin);
    }

    /* then optional plugins */
    for (p = g_list_first (core->optional_plugins); p; p = g_list_next (p)) {
        if ((plugin = n_core_open_plugin (core, (const char*) p->data)))
            optional_plugins = g_list_append (optional_plugins, plugin);

        if (!plugin)
            N_INFO (LOG_CAT "optional plugin %s not opened.", p->data);
    }

    /* Clear temporary conf file list. */
    n_core_plugin_conf_files_done ();

    /* load events from the given event path. */

    if (!n_core_parse_events (core))
        goto failed_init;

    /* initialize required plugins */
    for (p = required_plugins; p; p = g_list_next (p)) {
        if (!n_core_init_plugin ((NPlugin *) p->data, TRUE))
            goto failed_init;

        core->plugins = g_list_append (core->plugins, p->data);
    }

    g_list_free (required_plugins);

    /* initialize optional plugins */
    for (p = optional_plugins; p; p = g_list_next (p)) {
        if (n_core_init_plugin ((NPlugin *) p->data, FALSE))
            core->plugins = g_list_append (core->plugins, p->data);
    }

    g_list_free (optional_plugins);

    /* initialize all sinks. if no sinks, we're done. */

    if (!core->sinks) {
        N_ERROR (LOG_CAT "no plugin has registered sink interface");
        goto failed_init;
    }

    /* setup the sink priorities based on the sink-order */

    n_core_set_sink_priorities (core->sinks, core->sink_order);

    for (sink = core->sinks; *sink; ++sink) {
        if ((*sink)->funcs.initialize && !(*sink)->funcs.initialize (*sink)) {
            N_ERROR (LOG_CAT "sink '%s' failed to initialize", (*sink)->name);
            goto failed_init;
        }
    }

    /* initialize all inputs. */

    if (!core->inputs) {
        N_ERROR (LOG_CAT "no plugin has registered input interface");
        goto failed_init;
    }

    for (input = core->inputs; *input; ++input) {
        if ((*input)->funcs.initialize && !(*input)->funcs.initialize (*input)) {
            N_ERROR (LOG_CAT "input '%s' failed to initialize", (*input)->name);
            goto failed_init;
        }
    }

    /* fire the init done hook. */

    n_core_fire_hook (core, N_CORE_HOOK_INIT_DONE, NULL);

    return TRUE;

failed_init:
    return FALSE;
}

static void
unload_plugin_cb (gpointer data, gpointer userdata)
{
    NCore   *core   = (NCore*) userdata;
    NPlugin *plugin = (NPlugin*) data;
    n_core_unload_plugin (core, plugin);
}

void
n_core_shutdown (NCore *core)
{
    g_assert (core != NULL);

    NInputInterface **input = NULL;
    NSinkInterface  **sink  = NULL;

    /* shutdown all inputs */

    if (core->inputs) {
        for (input = core->inputs; *input; ++input) {
            if ((*input)->funcs.shutdown)
                (*input)->funcs.shutdown (*input);
        }
    }

    /* shutdown all sinks */

    if (core->sinks) {
        for (sink = core->sinks; *sink; ++sink) {
            if ((*sink)->funcs.shutdown)
                (*sink)->funcs.shutdown (*sink);
        }
        g_free (core->sinks);
        core->sinks = NULL;
    }

    if (core->plugins) {
        g_list_foreach (core->plugins, unload_plugin_cb, core);
        g_list_free (core->plugins);
        core->plugins = NULL;
    }

    if (core->required_plugins) {
        g_list_foreach (core->required_plugins, (GFunc) g_free, NULL);
        g_list_free (core->required_plugins);
        core->required_plugins = NULL;
    }

    if (core->optional_plugins) {
        g_list_foreach (core->optional_plugins, (GFunc) g_free, NULL);
        g_list_free (core->optional_plugins);
        core->optional_plugins = NULL;
    }

    core->shutdown_done = TRUE;
}

void
n_core_register_sink (NCore *core, const NSinkInterfaceDecl *iface)
{
    g_assert (core != NULL);
    g_assert (iface->name != NULL);
    g_assert (iface->play != NULL);
    g_assert (iface->stop != NULL);

    NSinkInterface *sink = NULL;
    sink = g_new0 (NSinkInterface, 1);
    sink->name  = iface->name;
    sink->type  = iface->type;
    sink->core  = core;
    sink->funcs = *iface;

    core->num_sinks++;
    core->sinks = (NSinkInterface**) g_realloc (core->sinks,
        sizeof (NSinkInterface*) * (core->num_sinks + 1));

    core->sinks[core->num_sinks-1] = sink;
    core->sinks[core->num_sinks]   = NULL;

    N_DEBUG (LOG_CAT "sink interface '%s' registered", sink->name);
}

void
n_core_register_input (NCore *core, const NInputInterfaceDecl *iface)
{
    NInputInterface *input = NULL;

    g_assert (core != NULL);
    g_assert (iface->name != NULL);

    input = g_new0 (NInputInterface, 1);
    input->name  = iface->name;
    input->core  = core;
    input->funcs = *iface;

    core->num_inputs++;
    core->inputs = (NInputInterface**) g_realloc (core->inputs,
        sizeof (NInputInterface*) * (core->num_inputs + 1));

    core->inputs[core->num_inputs-1] = input;
    core->inputs[core->num_inputs]   = NULL;

    N_DEBUG (LOG_CAT "input interface '%s' registered", input->name);
}

static gint
n_core_sort_event_cb (gconstpointer a, gconstpointer b)
{
    const NEvent *ea = (const NEvent*) a;
    const NEvent *eb = (const NEvent*) b;

    guint numa = ea->rules ? n_proplist_size (ea->rules) : 0;
    guint numb = eb->rules ? n_proplist_size (eb->rules) : 0;

    return (numa > numb) ? -1 : ((numa < numb) ? 1 : 0);
}

static void
n_core_dump_value_cb (const char *key, const NValue *value, gpointer userdata)
{
    (void) userdata;

    gchar *value_str = n_value_to_string ((NValue*) value);
    N_DEBUG (LOG_CAT "+ %s = %s", key, value_str);
    g_free (value_str);
}

void
n_core_add_event (NCore *core, NEvent *event)
{
    g_assert (core != NULL);
    g_assert (event != NULL);

    GList  *event_list = NULL;
    GList  *iter       = NULL;
    NEvent *found      = NULL;

    /* get the event list for the specific event name. */

    event_list = g_hash_table_lookup (core->event_table, event->name);

    /* iterate through the event list and try to find an event that has the
       same rules. */

    for (iter = g_list_first (event_list); iter; iter = g_list_next (iter)) {
        found = (NEvent*) iter->data;

        if (n_proplist_match_exact (found->rules, event->rules)) {
            /* match found. merge the properties to the pre-existing event
               and free the new one. */

            N_DEBUG (LOG_CAT "merging event '%s'", event->name);
            n_proplist_foreach (event->rules, n_core_dump_value_cb, NULL);

            n_proplist_merge (found->properties, event->properties);
            n_event_free (event);

            return;
        }
    }

    /* completely new event, add it to the list and sort it. */

    N_DEBUG (LOG_CAT "new event '%s'", event->name);
    if (n_proplist_size (event->rules) > 0)
        n_proplist_foreach (event->rules, n_core_dump_value_cb, NULL);
    else
        N_DEBUG (LOG_CAT "+ default");

    N_DEBUG (LOG_CAT "properties");
    n_proplist_foreach (event->properties, n_core_dump_value_cb, NULL);

    event_list = g_list_append (event_list, event);
    event_list = g_list_sort (event_list, n_core_sort_event_cb);
    g_hash_table_replace (core->event_table, g_strdup (event->name), event_list);

    core->event_list = g_list_append (core->event_list, event);
}

static void
n_core_parse_events_from_file (NCore *core, const char *filename)
{
    g_assert (core != NULL);
    g_assert (filename != NULL);

    GKeyFile  *keyfile    = NULL;
    GHashTable *defines   = NULL;
    GError    *error      = NULL;
    gchar    **group_list = NULL;
    gchar    **group      = NULL;
    NEvent    *event      = NULL;

    keyfile = g_key_file_new ();
    if (!g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, &error)) {
        N_WARNING (LOG_CAT "failed to load event file: %s", error->message);
        g_error_free    (error);
        g_key_file_free (keyfile);
        return;
    }

    /* each unique group is considered as an event, even if split within
       separate files. */

    N_DEBUG (LOG_CAT "processing event file '%s'", filename);

    group_list = g_key_file_get_groups (keyfile, NULL);

    for (group = group_list; *group; ++group)
        n_event_parse_defines (core, keyfile, *group, &defines);

    for (group = group_list; *group; ++group) {
        event = n_event_new_from_group (core, keyfile, *group, defines);
        if (event)
            n_core_add_event (core, event);
    }

    g_strfreev      (group_list);
    g_key_file_free (keyfile);
    if (defines)
        g_hash_table_destroy (defines);
}

static int
n_core_parse_events (NCore *core)
{
    GSList        *conf_files = NULL;
    GSList        *i          = NULL;
    gchar         *filename   = NULL;

    /* find all the events within the given path */
    conf_files = n_core_conf_files_from_path (core->conf_path, EVENT_CONF_PATH);

    if (!conf_files) {
        N_ERROR (LOG_CAT "no events defined.");
        return FALSE;
    }

    for (i = conf_files; i; i = g_slist_next (i)) {
        filename = i->data;
        n_core_parse_events_from_file (core, filename);
    }

    g_slist_free_full (conf_files, g_free);

    return TRUE;
}

static void
n_core_parse_keytypes (NCore *core, GKeyFile *keyfile)
{
    g_assert (core != NULL);
    g_assert (keyfile != NULL);

    gchar **conf_keys = NULL;
    gchar **key       = NULL;
    gchar  *value     = NULL;
    int     key_type  = 0;

    /* load all the event configuration key entries. */

    conf_keys = g_key_file_get_keys (keyfile, CORE_CONF_KEYTYPES, NULL, NULL);
    if (!conf_keys)
        return;

    for (key = conf_keys; *key; ++key) {
        value = g_key_file_get_string (keyfile, CORE_CONF_KEYTYPES, *key, NULL);
        if (!value) {
            N_WARNING (LOG_CAT "no datatype defined for key '%s'", *key);
            continue;
        }

        if (strncmp (value, "INTEGER", 7) == 0)
            key_type = N_VALUE_TYPE_INT;
        else if (strncmp (value, "STRING", 6) == 0)
            key_type = N_VALUE_TYPE_STRING;
        else if (strncmp (value, "BOOLEAN", 7) == 0)
            key_type = N_VALUE_TYPE_BOOL;

        if (!key_type) {
            N_WARNING (LOG_CAT "unrecognized datatype '%s' for key '%s'",
                value, *key);
            g_free (value);
            continue;
        }

        N_DEBUG (LOG_CAT "new key type '%s' = %s", *key, value);
        g_hash_table_replace (core->key_types, g_strdup (*key), GINT_TO_POINTER(key_type));
        g_free (value);
    }

    g_strfreev (conf_keys);
}

static void
n_core_parse_sink_order (NCore *core, GKeyFile *keyfile)
{
    g_assert (core != NULL);
    g_assert (keyfile != NULL);

    gchar **sink_list = NULL;
    gchar **item      = NULL;
    gsize   num_items = 0;

    sink_list = g_key_file_get_string_list (keyfile, "general", "sink-order",
        &num_items, NULL);

    if (!sink_list) {
        N_WARNING (LOG_CAT "no sink-order, re-synchronization may be undefined.");
        return;
    }

    for (item = sink_list; *item; ++item)
        core->sink_order = g_list_append (core->sink_order, g_strdup (*item));

    g_strfreev (sink_list);
}

static void
parse_plugins (gchar **plugins, GList **list)
{
    g_assert (plugins);

    gchar    **item       = NULL;

    for (item = plugins; *item; ++item)
        *list = g_list_append (*list, g_strdup (*item));
}

static int
n_core_parse_configuration (NCore *core)
{
    g_assert (core != NULL);
    g_assert (core->conf_path != NULL);

    GKeyFile  *keyfile    = NULL;
    GError    *error      = NULL;
    gchar     *filename   = NULL;
    gchar    **plugins    = NULL;

    filename = g_build_filename (core->conf_path, DEFAULT_CONF_FILENAME, NULL);
    keyfile  = g_key_file_new ();

    if (!g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, &error)) {
        N_WARNING (LOG_CAT "failed to load configuration file file: %s", error->message);
        g_error_free    (error);
        g_key_file_free (keyfile);
        g_free          (filename);
        return FALSE;
    }

    N_DEBUG (LOG_CAT "parsing configuration file '%s'", filename);

    /* parse the required plugins. */
    if ((plugins = g_key_file_get_string_list (keyfile, "general", "plugins", NULL, NULL)) != NULL) {
        parse_plugins (plugins, &core->required_plugins);
        g_strfreev (plugins);
    }

    /* parse the optional plugins. */
    if ((plugins = g_key_file_get_string_list (keyfile, "general", "plugins-optional", NULL, NULL)) != NULL) {
        parse_plugins (plugins, &core->optional_plugins);
        g_strfreev (plugins);
    }

    /* load all the event configuration key entries. */

    n_core_parse_keytypes (core, keyfile);

    /* load the sink order. the first sink has the highest priority. */

    n_core_parse_sink_order (core, keyfile);

    g_key_file_free (keyfile);
    g_free          (filename);

    return TRUE;
}

static const char*
n_core_match_and_strip_prefix (const char *value, const char *prefix)
{
    g_assert (value != NULL);
    g_assert (prefix != NULL);

    if (g_str_has_prefix (value, prefix))
        return (const char*) (value + strlen (prefix));

    return NULL;
}

static void
n_core_match_event_rule_cb (const char *key, const NValue *value,
                            gpointer userdata)
{
    NEventMatchResult *result      = (NEventMatchResult*) userdata;
    NRequest          *request     = result->request;
    NValue            *match_value = NULL;
    const char        *context_key = NULL;
    const char        *str         = NULL;

    if (result->skip_rest)
        return;

    /* assume positive result */

    result->has_match = TRUE;

    /* if the key has a context@ prefix, then we will lookup the value from
       the current context. */

    context_key = n_core_match_and_strip_prefix (key, "context@");
    match_value = context_key ?
        (NValue*) n_context_get_value (result->context, context_key) :
        (NValue*) n_proplist_get (request->properties, key);

    /* if match value has a *, then any value for request will do. */

    str = n_value_get_string ((NValue*) value);
    if (str && g_str_equal (str, "*"))
        return;

    /* the moment we find a key and value that does not match, we're done
       here. */

    if (!match_value || !n_value_equals (value, match_value)) {
        result->has_match  = FALSE;
        result->skip_rest  = TRUE;
    }
}

NEvent*
n_core_evaluate_request (NCore *core, NRequest *request)
{
    g_assert (core != NULL);
    g_assert (request != NULL);

    NEvent *event      = NULL;
    NEvent *found      = NULL;
    GList  *event_list = NULL;
    GList  *iter       = NULL;

    NEventMatchResult result;

    N_DEBUG (LOG_CAT "evaluating events for request '%s'",
        request->name);

    /* find the list of events that have the same name. */

    event_list = (GList*) g_hash_table_lookup (core->event_table, request->name);
    if (!event_list)
        return NULL;

    /* for each event, match the properties. */

    for (iter = g_list_first (event_list); iter; iter = g_list_next (iter)) {
        event = (NEvent*) iter->data;

        /* default event with no properties, accept. */

        if (n_proplist_size (event->rules) == 0) {
            found = event;
            break;
        }

        result.request    = request;
        result.context    = core->context;
        result.has_match  = FALSE;
        result.skip_rest  = FALSE;

        n_proplist_foreach (event->rules, n_core_match_event_rule_cb, &result);

        if (result.has_match) {
            found = event;
            break;
        }
    }

    if (found) {
        N_DEBUG (LOG_CAT "evaluated to '%s'", found->name);
        n_proplist_foreach (event->rules, n_core_dump_value_cb, NULL);
    }

    return found;
}

NContext*
n_core_get_context (NCore *core)
{
    return (core != NULL) ? core->context : NULL;
}

GList*
n_core_get_requests (NCore *core)
{
    if (!core)
        return NULL;

    return core->requests;
}

NSinkInterface**
n_core_get_sinks (NCore *core)
{
    if (!core)
        return NULL;

    return core->sinks;
}

GList*
n_core_get_events (NCore *core)
{
    if (!core)
        return NULL;

    return core->event_list;
}

int
n_core_connect (NCore *core, NCoreHook hook, int priority,
                NHookCallback callback, void *userdata)
{
    if (!core || !callback)
        return FALSE;

    if (hook >= N_CORE_HOOK_LAST)
        return FALSE;

    N_DEBUG (LOG_CAT "0x%p connected to hook '%s'", callback,
        n_core_hook_to_string (hook));

    n_hook_connect (&core->hooks[hook], priority, callback, userdata);

    return TRUE;
}

void
n_core_disconnect (NCore *core, NCoreHook hook, NHookCallback callback,
                   void *userdata)
{
    if (!core || !callback)
        return;

    if (hook >= N_CORE_HOOK_LAST)
        return;

    n_hook_disconnect (&core->hooks[hook], callback, userdata);
}

void
n_core_fire_hook (NCore *core, NCoreHook hook, void *data)
{
    if (!core || hook >= N_CORE_HOOK_LAST)
        return;

    N_DEBUG (LOG_CAT "firing hook '%s'", n_core_hook_to_string (hook));
    n_hook_fire (&core->hooks[hook], data);
}
