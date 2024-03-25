#include <stdlib.h>
#include <check.h>

#include "ngf/core.h"
#include "src/ngf/core-internal.h"
#include "ngf/event.h"

START_TEST (test_create)
{
    NCore *core = NULL;
    core = n_core_new (NULL, NULL);
    fail_unless (core != NULL);
    fail_unless (core->conf_path != NULL);
    fail_unless (core->plugin_path != NULL);
    fail_unless (core->context != NULL);
    fail_unless (core->eventlist != NULL);
    fail_unless (core->key_types != NULL);

    n_core_free (core);
    core = NULL;
}
END_TEST

START_TEST (test_get_context)
{
    NCore *core = NULL;
    fail_unless (n_core_get_context (core) == NULL);
    core = n_core_new (NULL, NULL);
    fail_unless (core != NULL);
    fail_unless (n_core_get_context (core) != NULL);
    NContext *context = n_context_new ();
    n_context_free (core->context);
    core->context = context;
    NContext *received_context = n_core_get_context (core);
    fail_unless (received_context != NULL);
    fail_unless (received_context == context);

    n_core_free (core);
    core = NULL;
}
END_TEST

START_TEST (test_get_requests)
{
    NCore *core = NULL;
    fail_unless (n_core_get_requests (core) == NULL);
    core = n_core_new (NULL, NULL);
    fail_unless (core != NULL);
    fail_unless (n_core_get_requests (core) == NULL);
    NRequest *request = n_request_new ();
    GList *list = NULL;
    list = g_list_append (list, request);
    core->requests = list;
    GList *received_list = n_core_get_requests (core);
    fail_unless (received_list != NULL);
    fail_unless (received_list == list);

    n_core_free (core);
    core = NULL;
}
END_TEST

START_TEST (test_add_get_events)
{
    NCore *core = NULL;
    fail_unless (n_core_get_events (core) == NULL);
    core = n_core_new (NULL, NULL);
    fail_unless (core != NULL);
    fail_unless (n_core_get_events (core) == NULL);

    GKeyFile *keyfile = NULL;
    keyfile = g_key_file_new ();
    g_key_file_set_value (keyfile, "sms", "sink.null", "true");
    n_event_list_parse_keyfile (core->eventlist, keyfile);
    g_key_file_free (keyfile);

    GList *events = NULL;
    events = n_core_get_events (core);
    fail_unless (events != NULL);

    uint length = g_list_length (events);
    fail_unless (length == 1);
    NEvent *receivedEvent = (NEvent*)events->data;
    fail_unless (g_strcmp0 (receivedEvent->name, "sms") == 0);

    events = NULL;
    n_core_free (core);
    core = NULL;
}
END_TEST

static void callback (NHook *hook, void *data, void *userdata)
{
    (void) hook;
    (void) data;
    (void) userdata;
}

START_TEST (test_connect)
{
    NCore *core = NULL;
    core = n_core_new (NULL, NULL);
    fail_unless (core != NULL);
    gboolean result = TRUE;
    int priority = 10;
    NCoreHook hook = N_CORE_HOOK_INIT_DONE;
    void *userdata = NULL;
    GList *list = NULL;
    
    list = core->hooks[hook].slots;
    fail_unless (g_list_length (list) == 0);
    list = NULL;

    /* connect tests */
    
    /* core is NULL */
    result = n_core_connect (NULL, hook, priority, callback, userdata);
    fail_unless (result == FALSE);
    
    /* callback is NULL */
    result = n_core_connect (core, hook, priority, NULL, userdata);
    fail_unless (result == FALSE);
    
    /* hook >= N_CORE_HOOK_LAST */
    hook = N_CORE_HOOK_LAST;
    result = n_core_connect (core, hook, priority, callback, userdata);
    fail_unless (result == FALSE);
    
    /* valid case */
    hook = N_CORE_HOOK_INIT_DONE;
    result = n_core_connect (core, hook, priority, callback, userdata);
    fail_unless (result == TRUE);
    list = core->hooks[hook].slots;
    fail_unless (list != NULL);
    fail_unless (g_list_length (list) == 1);

    /* disconnect tests */

    /* core is NULL */
    n_core_disconnect (NULL, hook, callback, userdata);
    list = core->hooks[hook].slots;
    fail_unless (list != NULL);
    fail_unless (g_list_length (list) == 1);
    list = NULL;

    /* callback is NULL */
    n_core_disconnect (core, hook, NULL, userdata);
    list = core->hooks[hook].slots;
    fail_unless (list != NULL);
    fail_unless (g_list_length (list) == 1);
    list = NULL;

    /* hook >= N_CORE_HOOK_LAST */
    hook = N_CORE_HOOK_LAST;
    n_core_disconnect (core, hook, callback, userdata);

    /* disconnect callback */
    hook = N_CORE_HOOK_INIT_DONE;
    n_core_disconnect (core, hook, callback, userdata);
    list = core->hooks[hook].slots;
    fail_unless (list == NULL);
    fail_unless (g_list_length (list) == 0);

    n_core_free (core);
    core = NULL;
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

    s = suite_create ("\tCore tests");

    tc = tcase_create ("test create");
    tcase_add_test (tc, test_create);
    suite_add_tcase (s, tc);

    tc = tcase_create ("get context");
    tcase_add_test (tc, test_get_context);
    suite_add_tcase (s, tc);

    tc = tcase_create ("get requests");
    tcase_add_test (tc, test_get_requests);
    suite_add_tcase (s, tc);

    tc = tcase_create ("add & get events");
    tcase_add_test (tc, test_add_get_events);
    suite_add_tcase (s, tc);

    tc = tcase_create ("connect/disconnect callback to/from hook");
    tcase_add_test (tc, test_connect);
    suite_add_tcase (s, tc);
    
    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    num_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
