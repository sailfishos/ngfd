#include <stdlib.h>
#include <check.h>

#include "ngf/plugin.h"
#include "src/ngf/plugin-internal.h"
#include "src/ngf/core-internal.h"
#include <stdio.h>

START_TEST (test_get_core)
{
    NPlugin *plugin = NULL;
    /* NULL checking */
    ck_assert (n_plugin_get_core (plugin) == NULL);
    plugin = g_new0 (NPlugin, 1);
    ck_assert (plugin != NULL);

    NCore *core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    core->conf_path = g_strdup ("conf_path");
    plugin->core = core;
    NCore *receivedCore = n_plugin_get_core (plugin);
    ck_assert (receivedCore != NULL);
    ck_assert (g_strcmp0 (receivedCore->conf_path, core->conf_path) == 0);

    g_free (plugin);
    plugin = NULL;
}
END_TEST

START_TEST (test_get_params)
{
    NPlugin *plugin = NULL;
    /* NULL checking */
    ck_assert (n_plugin_get_params (plugin) == NULL);
    plugin = g_new0 (NPlugin, 1);
    ck_assert (plugin != NULL);

    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    plugin->params = proplist;
    const NProplist *receivedParams = NULL;
    const char *key1 = "key1";
    n_proplist_set_int (proplist, key1, -100);
    receivedParams = n_plugin_get_params (plugin);
    ck_assert (receivedParams != NULL);
    ck_assert (n_proplist_match_exact (receivedParams, proplist) == TRUE);
    
    n_proplist_free (proplist);
    proplist = NULL;
    g_free (plugin);
    plugin = NULL;
}
END_TEST

static int
plugin_play (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    (void) request;
    return TRUE;
}

static void
plugin_stop (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    (void) request;
}

START_TEST (test_register_sink)
{
    static const NSinkInterfaceDecl decl = {
        .name       = "unit_test_SINK",
        .initialize = NULL,
        .shutdown   = NULL,
        .can_handle = NULL,
        .prepare    = NULL,
        .play       = plugin_play,
        .pause      = NULL,
	.stop       = plugin_stop
    };
    NPlugin *plugin = NULL;
    plugin = g_new0 (NPlugin, 1);
    ck_assert (plugin != NULL);
    NCore *core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    plugin->core = core;
    int size = -1;
    
    n_plugin_register_sink (NULL, &decl);
    size = core->num_sinks;
    ck_assert (size == 0);
    n_plugin_register_sink (plugin, NULL);
    size = core->num_sinks;
    ck_assert (size == 0);
    
    /* tests for core -> get sink */
    ck_assert (n_core_get_sinks (NULL) == NULL);
    ck_assert (n_core_get_sinks (core) == NULL);
    
    /* register sink */
    n_plugin_register_sink (plugin, &decl);
    size = core->num_sinks;
    ck_assert (size == 1);

    /* tests for core -> get sink */
    NSinkInterface **ifacev = NULL;
    ifacev = n_core_get_sinks (core);
    ck_assert (ifacev != NULL);
    NSinkInterface *iface = ifacev[0];
    ck_assert (g_strcmp0 (iface->name, decl.name) == 0);

    n_core_free (core);
    core = NULL;
    g_free (plugin);
    plugin = NULL;
}
END_TEST

START_TEST (test_register_input)
{
    static const NInputInterfaceDecl decl = {
        .name       = "unit_test_INPUT",
        .initialize = NULL,
        .shutdown   = NULL,
	.send_error = NULL,
	.send_reply = NULL
    };
    NPlugin *plugin = NULL;
    plugin = g_new0 (NPlugin, 1);
    ck_assert (plugin != NULL);
    NCore *core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    plugin->core = core;
    int size = -1;
    
    n_plugin_register_input (NULL, &decl);
    size = core->num_inputs;
    ck_assert (size == 0);
    n_plugin_register_input (plugin, NULL);
    size = core->num_inputs;
    ck_assert (size == 0);
    
    /* register input */
    n_plugin_register_input (plugin, &decl);
    size = core->num_inputs;
    ck_assert (size == 1);
    
    n_core_free (core);
    core = NULL;
    g_free (plugin);
    plugin = NULL;							
}
END_TEST

START_TEST (test_load_plugin)
{
    NCore *core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);

    /* allow execution inside build tree */
    const char *build_tree_plugin_path = "./libngfd_test_fake.la";
    gchar *plugin_path = NULL;
    if (g_file_test (build_tree_plugin_path, G_FILE_TEST_EXISTS)) {
        plugin_path = g_strdup (build_tree_plugin_path);
    } else {
        plugin_path = g_build_filename (core->plugin_path, "libngfd_test_fake.so", NULL);
    }

    NPlugin *loaded_plugin = NULL;
    /* try to load not existing plugin file */
    loaded_plugin = n_plugin_open ("./not_existing_plugin.la");
    ck_assert (loaded_plugin == NULL);
    /* load dummyu plugin file - valid one*/
    loaded_plugin = n_plugin_open (plugin_path);
    ck_assert (loaded_plugin != NULL);
    ck_assert (loaded_plugin->module != NULL);
    loaded_plugin->core = core;
    loaded_plugin->params = n_proplist_new ();

    /* load actual plugin */
    int result = loaded_plugin->load (loaded_plugin);
    ck_assert (result == TRUE);

    const char *name = "test-fake";
    ck_assert (g_strcmp0 (name, loaded_plugin->get_name ()) == 0);
    const char *desc = "Fake plugin for unit test purposes";
    ck_assert (g_strcmp0 (desc, loaded_plugin->get_desc ()) == 0);
    const char *version = "0.1";
    ck_assert (g_strcmp0 (version, loaded_plugin->get_version ()) == 0);

    int size = -1;
    size = core->num_sinks;
    ck_assert (size == 1);

    /* tests for core -> get sink */
    NSinkInterface **ifacev = NULL;
    ifacev = n_core_get_sinks (core);
    ck_assert (ifacev != NULL);
    NSinkInterface *iface = ifacev[0];
    ck_assert (g_strcmp0 (iface->name, "fake") == 0);

    /* call unload for specific plugin */
    loaded_plugin->unload (loaded_plugin);

    /* it will also unload plugin and free plugin structre*/
    n_core_free (core);
    core = NULL;

    g_free (plugin_path);
}
END_TEST

int
main (int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    int num_failed = 0;

    Suite *s = NULL;
    TCase *tc = NULL;
    SRunner *sr = NULL;

    s = suite_create ("\tPlugin tests");

    tc = tcase_create ("get core");
    tcase_add_test (tc, test_get_core);
    suite_add_tcase (s, tc);

    tc = tcase_create ("get params");
    tcase_add_test (tc, test_get_params);
    suite_add_tcase (s, tc);

    tc = tcase_create ("register sink");
    tcase_add_test (tc, test_register_sink);
    suite_add_tcase (s, tc);

    tc = tcase_create ("register input");
    tcase_add_test (tc, test_register_input);
    suite_add_tcase (s, tc);

    tc = tcase_create ("load plug-in");
    tcase_add_test (tc, test_load_plugin);
    suite_add_tcase (s, tc);

    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    num_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
