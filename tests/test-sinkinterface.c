#include <stdlib.h>
#include <check.h>
#include <stdio.h>		// only to get some printout

#include "ngf/sinkinterface.h"
//#include "src/ngf/sinkinterface-internal.h"
#include "src/ngf/request-internal.h"
#include "src/ngf/core-player.c"

START_TEST (test_get_core_and_name)
{
    static const char fake_sink_name[] = "TEST_GET_CORE_AND_NAME_sink_name";
    static const char fake_path[] = "TEST_GET_CORE_AND_NAME_path";

    NCore *core = n_core_new (NULL, NULL);
    core->conf_path = g_strdup (fake_path);

    NSinkInterface *iface = g_new0 (NSinkInterface, 1);
    iface->core = core;
    iface->name = fake_sink_name;

    ck_assert (n_sink_interface_get_core (NULL) == NULL);
    ck_assert (n_sink_interface_get_name (NULL) == NULL);

    const char *receivedName = n_sink_interface_get_name (iface);
    ck_assert (receivedName != NULL);
    ck_assert (g_strcmp0 (receivedName, fake_sink_name) == 0);

    NCore *receivedCore = n_sink_interface_get_core (iface);
    ck_assert (receivedCore != NULL);
    ck_assert (receivedCore == core);

    n_core_free (core);
    core = NULL;

    g_free (iface);
    iface = NULL;
}
END_TEST

START_TEST (test_resync_on_master)
{
    static const char fake_sink_name[] = "TEST_RESYNC_ON_MASTER_sink_name";
    static const char fake_master_name[] = "TEST_RESYNC_ON_MASTER_master_sink_name";
    static const char fake_req_name[] = "TEST_RESYNC_ON_MASTER_request_name";

    NCore *core = n_core_new (NULL, NULL);

    NSinkInterface *iface = g_new0 (NSinkInterface, 1);
    iface->core = core;
    iface->name = fake_sink_name;

    NRequest *request = n_request_new ();
    request->core = core;
    request->name = g_strdup (fake_req_name);
    request->sinks_resync = NULL;

    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
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
    master_sink->name = fake_master_name;
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

    n_request_free (request);
    request = NULL;

    g_free (iface);
    iface = NULL;

    n_core_free (core);
    core = NULL;
}
END_TEST

#define DATA_KEY "sinkInterface.test.data"

static void
iface_stop (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    int *data = g_slice_new0 (int);
    *data = 0;
    n_request_store_data (request, DATA_KEY, data);
}

static int
iface_prepare (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    int *data = (int *) n_request_get_data (request, DATA_KEY);
    if (data != NULL)
        *data += 1;
    return TRUE;
}

START_TEST (test_resynchronize)
{
    static const char fake_req_name[] = "TEST_RESYNC_request_name";
    static const char fake_sink_name[] = "TEST_RESYNC_sink_name";
    static const char fake_interface_name[] = "TEST_RESYNC_interface_name";
    static const NSinkInterfaceDecl fake_interface_decl = {
        .name       = fake_interface_name,
        .initialize = NULL,
        .shutdown   = NULL,
        .can_handle = NULL,
        .prepare    = iface_prepare,
        .play       = NULL,
        .pause      = NULL,
        .stop       = iface_stop
    };

    const guint fake_timer_id = 100;

    NCore *core = n_core_new (NULL, NULL);

    NSinkInterface *iface = g_new0 (NSinkInterface, 1);
    iface->core = core;
    iface->name = fake_sink_name;

    NRequest *request = n_request_new ();
    ck_assert (request->sinks_preparing == NULL);
    request->core = core;
    request->name = g_strdup (fake_req_name);

    NProplist *proplist = n_proplist_new ();
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
    request->master_sink = iface;

    /* play_source_id > 0 */
    ck_assert (request->play_source_id == 0);
    request->play_source_id = fake_timer_id;

    n_sink_interface_resynchronize (iface, request);
    ck_assert (request->sinks_prepared == NULL);

    ck_assert (request->play_source_id == fake_timer_id);
    request->play_source_id = 0;

    /* sink_resync is NULL */
    request->sinks_playing = g_list_append (request->sinks_playing, iface);
    n_sink_interface_resynchronize (iface, request);
    ck_assert (request->sinks_playing == NULL);
    ck_assert (request->sinks_prepared != NULL);

    ck_assert (request->play_source_id != 0);
    g_source_remove (request->play_source_id);
    request->play_source_id = 0;

    NSinkInterface *sink_in_resync = g_new0 (NSinkInterface, 1);
    sink_in_resync->name = fake_sink_name;
    sink_in_resync->funcs = fake_interface_decl;
    request->sinks_resync = g_list_append (request->sinks_resync, sink_in_resync);

    /*sink_resync != NULL */
    n_sink_interface_resynchronize (iface, request);
    /*
     * n_core_stop_sinks -> here data is created
     * n_core_prepare_sinks -> fere data is modified to equals 1
     * */

    int *data = (int *) n_request_get_data (request, DATA_KEY);
    ck_assert (data != NULL);
    ck_assert (*data == 1);
    ck_assert (request->sinks_resync == NULL);
    ck_assert (g_list_find (request->sinks_preparing, sink_in_resync) != NULL);

    g_slice_free (int, data);
    data = NULL;

    g_free (sink_in_resync);
    sink_in_resync = NULL;

    n_request_free (request);
    request = NULL;

    g_free (iface);
    iface = NULL;

    n_core_free (core);
    core = NULL;
}
END_TEST

START_TEST (test_synchronize)
{
    static const char fake_sink_name[] = "TEST_SYNCHRONIZE_sink_name";
    static const char fake_req_name[] = "TEST_SYNCHRONIZE_request_name";

    NCore *core = n_core_new (NULL, NULL);

    NSinkInterface *iface = g_new0 (NSinkInterface, 1);
    iface->core = core;
    iface->name = fake_sink_name;

    NRequest *request = n_request_new ();
    ck_assert (request->sinks_preparing == NULL);
    request->core = core;
    request->name = g_strdup (fake_req_name);

    NProplist *proplist = n_proplist_new ();
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

    n_request_free (request);
    request = NULL;

    g_free (iface);
    iface = NULL;

    n_core_free (core);
    core = NULL;
}
END_TEST

START_TEST (test_complete)
{
    static const char fake_sink_name[] = "TEST_COMPLETE_sink_name";
    static const char fake_req_name[] = "TEST_COMPLETE_request_name";

    NCore *core = n_core_new (NULL, NULL);

    NSinkInterface *iface = g_new0 (NSinkInterface, 1);
    iface->core = core;
    iface->name = fake_sink_name;

    NRequest *request = n_request_new ();
    ck_assert (request->sinks_playing == NULL);
    request->core = core;
    request->name = g_strdup (fake_req_name);

    NProplist *proplist = n_proplist_new ();
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
    ck_assert (request->stop_source_id != 0);

    n_request_free (request);
    request = NULL;

    g_free (iface);
    iface = NULL;

    n_core_free (core);
    core = NULL;
}
END_TEST

START_TEST (test_fail)
{
    static const char fake_req_name[] = "TEST_FAIL_request_name";
    static const char fake_sink_name[] = "TEST_FAIL_sink_name";

    NCore *core = n_core_new (NULL, NULL);

    NSinkInterface *iface = g_new0 (NSinkInterface, 1);
    iface->name = fake_sink_name;
    iface->core = core;

    NRequest *request = n_request_new_with_event (fake_req_name);
    ck_assert (!g_strcmp0(request->name, fake_req_name));
    ck_assert (request->has_failed == FALSE);
    ck_assert (request->stop_source_id == 0);
    request->core = core;

    NProplist *proplist = n_proplist_new ();
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
    ck_assert (request->stop_source_id == 0);
    n_sink_interface_fail (iface, request);
    ck_assert (request->has_failed == TRUE);
    ck_assert (request->stop_source_id != 0);

    request->has_failed = FALSE;
    n_sink_interface_fail (iface, request);
    ck_assert (request->has_failed == FALSE);

    n_request_free (request);
    request = NULL;

    g_free (iface);
    iface = NULL;

    n_core_free (core);
    core = NULL;
}
END_TEST

int
main (int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    setlinebuf (stdout);
    setlinebuf (stderr);

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
