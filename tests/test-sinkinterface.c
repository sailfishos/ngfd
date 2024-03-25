#include <stdlib.h>
#include <check.h>
#include <stdio.h>		// only to get some printout

#include "ngf/sinkinterface.h"
//#include "src/ngf/sinkinterface-internal.h"
#include "src/ngf/request-internal.h"
#include "src/ngf/core-player.c"


START_TEST (test_get_core_and_name)
{
    NSinkInterface *iface = NULL;
    iface = g_new0 (NSinkInterface, 1);
    ck_assert (iface != NULL);
    
    ck_assert (n_sink_interface_get_core (NULL) == NULL);
    ck_assert (n_sink_interface_get_name (NULL) == NULL);

    NCore *core = NULL;
    core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    core->conf_path = g_strdup ("path");
    iface->core = core;

    const char *name = "name";
    iface->name = name;

    const char *receivedName = n_sink_interface_get_name (iface);
    ck_assert (receivedName != NULL);
    ck_assert (g_strcmp0 (receivedName, name) == 0);

    NCore *receivedCore = n_sink_interface_get_core (iface);
    ck_assert (receivedCore != NULL);
    ck_assert (g_strcmp0 (core->conf_path, receivedCore->conf_path) == 0);
    
    n_core_free (core);
    core = NULL;
    g_free (iface);
    iface = NULL;
}
END_TEST

START_TEST (test_resync_on_master)
{
    NSinkInterface *iface = NULL;
    iface = g_new0 (NSinkInterface, 1);
    ck_assert (iface != NULL);
    const char *name = "TEST_RESYNC_ON_MASTER_sink_name";
    iface->name = name;
    NRequest *request = NULL;
    request = n_request_new ();
    ck_assert (request != NULL);
    const char *req_name = "TEST_RESYNC_ON_MASTER_REQUST_name";
    request->name = g_strdup (req_name);
    request->sinks_resync = NULL;
    NCore *core = NULL;
    core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    iface->core = core;
    request->core = core;
    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    n_request_set_properties (request, proplist);
    n_proplist_free (proplist);
    proplist = NULL;

    /* test for invalid parameter */
    n_sink_interface_set_resync_on_master (NULL, request);
    ck_assert (request->sinks_resync == NULL);
    /* test for invalid parameter */
    n_sink_interface_set_resync_on_master (iface, NULL);
    ck_assert (request->sinks_resync == NULL);

    /* master_sink = sink */
    request->master_sink = iface;
    n_sink_interface_set_resync_on_master (iface, request);
    ck_assert (request->sinks_resync == NULL);

    
    request->master_sink = NULL;
    NSinkInterface *master_sink = g_new0 (NSinkInterface, 1);
    const char *master_name = "TEST_RESYNC_ON_MASTER_sink_name";
    master_sink->name = master_name;
    request->master_sink = master_sink;
    
    /* add proper sink do resync sinks */
    n_sink_interface_set_resync_on_master (iface, request);
    ck_assert (request->sinks_resync != NULL);
    ck_assert (g_list_length (request->sinks_resync) == 1);
    ck_assert (g_list_find (request->sinks_resync, iface) != NULL);

    /* readd sink that is already synced */
    n_sink_interface_set_resync_on_master (iface, request);
    ck_assert (request->sinks_resync != NULL);
    ck_assert (g_list_length (request->sinks_resync) == 1);

    g_free (master_sink);
    master_sink = NULL;
    g_list_free (request->sinks_resync);
    request->sinks_resync = NULL;
    n_core_free (core);
    core = NULL;
    n_request_free (request);
    request = NULL;
    g_free (iface);
    iface = NULL;
}
END_TEST

#define DATA_KEY "sinkInterface.test.data"

static void
iface_stop (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    (void) request;
    int *data = g_slice_new0 (int);
    *data = 0;
    n_request_store_data (request, DATA_KEY, data);
}

static int
iface_prepare (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    int *data = (int*) n_request_get_data (request, DATA_KEY);
    if (data != NULL)
        *data += 1;
    return TRUE;
}

START_TEST (test_resynchronize)
{
    NSinkInterface *iface = NULL;
    iface = g_new0 (NSinkInterface, 1);
    ck_assert (iface != NULL);
    const char *name = "TEST_RESYNC_sink_name";
    iface->name = name;
    NRequest *request = NULL;
    request = n_request_new ();
    ck_assert (request != NULL);
    const char *req_name = "TEST_RESYNC_REQUST_name";
    request->name = g_strdup (req_name);
    request->sinks_preparing = NULL;
    NCore *core = NULL;
    core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    iface->core = core;
    request->core = core;
    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    n_request_set_properties (request, proplist);
    n_proplist_free (proplist);
    proplist = NULL;

    /* test for invelid parameter */
    n_sink_interface_resynchronize (NULL, request);
    ck_assert (request->sinks_prepared == NULL);
    /* test for invalid parameter */
    n_sink_interface_resynchronize (iface, NULL);
    ck_assert (request->sinks_prepared == NULL);
    /* sink (iface) is not master sink */
    n_sink_interface_resynchronize (iface, request);
    ck_assert (request->sinks_prepared == NULL);

    request->play_source_id = 100;
    request->master_sink = iface;
    /* play_source_id > 0 */
    n_sink_interface_resynchronize (iface, request);
    ck_assert (request->sinks_prepared == NULL);
    /* needs verification */

    request->sinks_playing = g_list_append (request->sinks_playing, iface);
    request->play_source_id = 0;
    /* sink_resync is NULL */
    n_sink_interface_resynchronize (iface, request);
    ck_assert (request->sinks_playing == NULL);
    ck_assert (request->sinks_prepared != NULL);
    ck_assert (request->play_source_id == 1);

    request->play_source_id = 0;
    NSinkInterface *sink_in_resync = g_new0 (NSinkInterface, 1);
    ck_assert (sink_in_resync != NULL);
    const char *sink_name = "TEST_RESYNC_sink_name";
    sink_in_resync->name = sink_name;
    static const NSinkInterfaceDecl decl = {
        .name       = "TEST_RESYNC_unit_test_DECL",
        .initialize = NULL,
        .shutdown   = NULL,
        .can_handle = NULL,
        .prepare    = iface_prepare,
        .play       = NULL,
        .pause      = NULL,
        .stop       = iface_stop
    };
    sink_in_resync->funcs = decl;
    request->sinks_resync = g_list_append (request->sinks_resync, sink_in_resync);

    /*sink_resync != NULL */
    n_sink_interface_resynchronize (iface, request);
    /*
     * n_core_stop_sinks -> here data is created
     * n_core_prepare_sinks -> fere data is modified to equals 1
     * */

    int *data = (int*) n_request_get_data (request, DATA_KEY);
    ck_assert (data != NULL);
    ck_assert (*data == 1);
    ck_assert (request->sinks_resync == NULL);
    ck_assert (g_list_find (request->sinks_preparing, sink_in_resync) != NULL);

    g_slice_free (int, data);
    data = NULL;
    g_free (sink_in_resync);
    sink_in_resync = NULL;
    g_list_free (request->sinks_prepared);
    request->sinks_prepared = NULL;
    n_core_free (core);
    core = NULL;
    n_request_free (request);
    request = NULL;
    g_free (iface);
    iface = NULL;
}
END_TEST

START_TEST (test_synchronize)
{
    NSinkInterface *iface = NULL;
    iface = g_new0 (NSinkInterface, 1);
    ck_assert (iface != NULL);
    const char *name = "TEST_SYNCHRONIZE_sink_name";
    iface->name = name;
    NRequest *request = NULL;
    request = n_request_new ();
    ck_assert (request != NULL);
    const char *req_name = "TEST_SYNCHRONIZE_REQUST_name";
    request->name = g_strdup (req_name);
    request->sinks_preparing = NULL;
    NCore *core = NULL;
    core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    iface->core = core;
    request->core = core;
    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    n_request_set_properties (request, proplist);
    n_proplist_free (proplist);
    proplist = NULL;

    /* test for invalid parameter */
    n_sink_interface_synchronize (NULL, request);
    ck_assert (request->sinks_prepared == NULL);
    /* test for invalid parameter */
    n_sink_interface_synchronize (iface, NULL);
    ck_assert (request->sinks_prepared == NULL);

    /* request->sinks_preparing is NULL */
    n_sink_interface_synchronize (iface, request);
    ck_assert (request->sinks_prepared == NULL);

    NSinkInterface *iface_second = g_new0 (NSinkInterface, 1);
    /* add different sink (iface_second) to preparing list */
    request->sinks_preparing = g_list_append (request->sinks_preparing, iface_second);
    /* sink (iface_second) is already in preparing phase, but we call sync for iface */
    n_sink_interface_synchronize (iface, request);
    ck_assert (g_list_length (request->sinks_preparing) == 1);
    ck_assert (request->sinks_prepared == NULL);

    /* add proper sink to preparing list, at that point two items are in the preparing list */
    request->sinks_preparing = g_list_append (request->sinks_preparing, iface);
    n_sink_interface_synchronize (iface, request);
    ck_assert (g_list_length (request->sinks_preparing) == 1);
    ck_assert (request->sinks_prepared != NULL);
    ck_assert (g_list_length (request->sinks_prepared) == 1);
    ck_assert (g_list_find (request->sinks_prepared, iface) != NULL);

    g_free (iface_second);
    iface_second = NULL;
    g_list_free (request->sinks_prepared);
    request->sinks_prepared = NULL;
    g_list_free (request->sinks_preparing);
    request->sinks_preparing = NULL;
    n_core_free (core);
    core = NULL;
    n_request_free (request);
    request = NULL;
    g_free (iface);
    iface = NULL;
}
END_TEST

START_TEST (test_complete)
{
    NSinkInterface *iface = NULL;
    iface = g_new0 (NSinkInterface, 1);
    ck_assert (iface != NULL);
    const char *name = "TEST_COMPLETE_sink_name";
    iface->name = name;
    NRequest *request = NULL;
    request = n_request_new ();
    ck_assert (request != NULL);
    const char *req_name = "TEST_COMPLETE_REQUST_name";
    request->name = g_strdup (req_name);
    request->sinks_playing = NULL;
    NCore *core = NULL;
    core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    iface->core = core;
    request->core = core;
    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    n_request_set_properties (request, proplist);
    n_proplist_free (proplist);
    proplist = NULL;

    /* sinks_playing = NULL */
    n_sink_interface_complete (iface, request);
    /* ?? verification ?? */

    request->sinks_playing = g_list_append (request->sinks_playing, iface);
    /* test for invalid parameters */
    n_sink_interface_complete (NULL, request);
    ck_assert (request->sinks_playing != NULL);
    ck_assert (g_list_length (request->sinks_playing) == 1);
    /* test for invalid parameters */
    n_sink_interface_complete (iface, NULL);
    ck_assert (request->sinks_playing != NULL);
    ck_assert (g_list_length (request->sinks_playing) == 1);
    
    n_sink_interface_complete (iface, request);
    ck_assert (request->sinks_playing == NULL);
    ck_assert (request->stop_source_id = 1);

    n_core_free (core);
    core = NULL;
    n_request_free (request);
    request = NULL;
    g_free (iface);
    iface = NULL;
}
END_TEST

START_TEST (test_fail)
{
    NSinkInterface *iface = NULL;
    iface = g_new0 (NSinkInterface, 1);
    ck_assert (iface != NULL);
    const char *name = "TEST_FAIL_sink_name";
    iface->name = name;
    NRequest *request = NULL;
    request = n_request_new ();
    ck_assert (request != NULL);
    const char *req_name = "TEST_FAIL_REQUST_name";
    request->name = g_strdup (req_name);
    /* stop_source_id < 0 to go to the end of the function */
    request->stop_source_id = 0;
    request->has_failed = FALSE;
    NCore *core = NULL;
    core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    iface->core = core;
    request->core = core;
    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    n_request_set_properties (request, proplist);
    n_proplist_free (proplist);
    proplist = NULL;
    
    /* test for invalid parameters */
    n_sink_interface_fail (NULL, request);
    ck_assert (request->has_failed == FALSE);
    /* test for invalid parametes */
    n_sink_interface_fail (iface, NULL);
    ck_assert (request->has_failed == FALSE);
    
    /* proper test */
    n_sink_interface_fail (iface, request);
    ck_assert (request->has_failed == TRUE);
    ck_assert (request->stop_source_id == 1);
    
    request->stop_source_id = 100;
    request->has_failed = FALSE;
    n_sink_interface_fail (iface, request);
    ck_assert (request->has_failed == FALSE);

    n_core_free (core);
    core = NULL;
    n_request_free (request);
    request = NULL;
    g_free (iface);
    iface = NULL;
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

    s = suite_create ("\tSinkinterface tests");

    tc = tcase_create ("get name, get core");
    tcase_add_test (tc, test_get_core_and_name);
    suite_add_tcase (s, tc);

    tc = tcase_create ("set resync on master");
    tcase_add_test (tc, test_resync_on_master);
    suite_add_tcase (s, tc);

    tc = tcase_create ("resynchronize");
    tcase_add_test (tc, test_resynchronize);
    suite_add_tcase (s, tc);

    tc = tcase_create ("synchronize");
    tcase_add_test (tc, test_synchronize);
    suite_add_tcase (s, tc);

    tc = tcase_create ("complete");
    tcase_add_test (tc, test_complete);
    suite_add_tcase (s, tc);

    tc = tcase_create ("fail");
    tcase_add_test (tc, test_fail);
    suite_add_tcase (s, tc);

    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    num_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
