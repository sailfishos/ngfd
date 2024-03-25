#include <stdlib.h>
#include <check.h>

#include "ngf/inputinterface.h"
#include "src/ngf/inputinterface-internal.h"
#include "ngf/request.h"
#include "src/ngf/core-internal.h"

START_TEST (test_get_core)
{
    NInputInterface *iface = NULL;
    iface = g_new0 (NInputInterface, 1);
    ck_assert (iface != NULL);
    /* NULL checking */
    ck_assert (n_input_interface_get_core (iface) == NULL);
    NCore *core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    core->conf_path = g_strdup ("conf_path");
    iface->core = core;
    NCore *receivedCore = NULL;
    receivedCore = n_input_interface_get_core (iface);
    ck_assert (receivedCore != NULL);
    ck_assert (g_strcmp0 (receivedCore->conf_path, core->conf_path) == 0);

    n_core_free (core);
    core = NULL;
    g_free (iface);
    iface = NULL;
}
END_TEST

typedef enum _State
{
    PREPARED,
    PLAYING,
    PAUSED,
} State;

typedef struct _Data
{
    State          state;
} Data;

#define DATA_KEY "plugin.test.data"

static int
plugin_prepare (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    Data *data = g_slice_new0 (Data);
    data->state = PREPARED;
    n_request_store_data (request, DATA_KEY, data);
    n_sink_interface_synchronize (iface, request);
    return TRUE;
}

static int
plugin_play (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    Data *data = (Data*) n_request_get_data (request, DATA_KEY);
    data->state = PLAYING;
    return TRUE;
}

static int 
plugin_pause (NSinkInterface *iface, NRequest *request)
{
    (void) iface;
    Data *data = (Data*) n_request_get_data (request, DATA_KEY);
    data->state = PAUSED;
    return TRUE;
}

static void
plugin_stop (NSinkInterface *iface, NRequest *request)
{
    (void) iface;

    Data *data = (Data*) n_request_get_data (request, DATA_KEY);
    g_slice_free (Data, data);
}

START_TEST (test_play_pause_request)
{
    static const NSinkInterfaceDecl decl = {
        .name       = "unit_test_SINK",
        .initialize = NULL,
        .shutdown   = NULL,
        .can_handle = NULL,
        .prepare    = plugin_prepare,
        .play       = plugin_play,
        .pause      = plugin_pause,
   	.stop       = plugin_stop
    };

    NInputInterface *iface = NULL;
    iface = g_new0 (NInputInterface, 1);
    ck_assert (iface != NULL);
    NCore *core = n_core_new (NULL, NULL);
    const char *event_name = "testing_event";

    GKeyFile *keyfile = NULL;
    keyfile = g_key_file_new ();
    g_key_file_set_value (keyfile, event_name, "sink.null", "true");
    n_event_list_parse_keyfile (core->eventlist, keyfile);
    g_key_file_free (keyfile);

    NPlugin *plugin = g_new0 (NPlugin, 1);
    ck_assert (plugin != NULL);
    plugin->core = core;
    n_plugin_register_sink (plugin, &decl);
    iface->core = core;
    NRequest *request = NULL;
    NProplist *proplist = n_proplist_new ();
    request = n_request_new_with_event_and_properties (event_name, proplist);
    ck_assert (request != NULL);
    Data *data = NULL;
    /* play */
    ck_assert (n_input_interface_play_request (NULL, request) == FALSE);
    ck_assert (n_input_interface_play_request (iface, NULL) == FALSE);
    ck_assert (n_input_interface_play_request (iface, request) == TRUE);
    data = (Data*) n_request_get_data (request, DATA_KEY);
    ck_assert (data->state == PREPARED);
    /* pause*/
    ck_assert (n_input_interface_pause_request (NULL, request) == FALSE);
    ck_assert (n_input_interface_pause_request (iface, NULL) == FALSE);
    ck_assert (n_input_interface_pause_request (iface, request) == TRUE);
    data = (Data*) n_request_get_data (request, DATA_KEY);
    ck_assert (data->state == PAUSED);
    ck_assert (n_input_interface_pause_request (iface, request) == TRUE);
    data = (Data*) n_request_get_data (request, DATA_KEY);
    ck_assert (data->state == PAUSED);
    /* resume */
    ck_assert (n_input_interface_play_request (iface, request) == TRUE);
    data = (Data*) n_request_get_data (request, DATA_KEY);
    ck_assert (data->state == PLAYING);
    
    g_free (iface);
    iface = NULL;
    n_request_free (request);
    request = NULL;
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

    s = suite_create ("\tInputinterface tests");

    tc = tcase_create ("get core");
    tcase_add_test (tc, test_get_core);
    suite_add_tcase (s, tc);

    tc = tcase_create ("play and pause request");
    tcase_add_test (tc, test_play_pause_request);
    suite_add_tcase (s, tc);

    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    num_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
