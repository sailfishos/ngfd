#include <stdlib.h>
#include <check.h>

#include "ngf/core.h"
#include "src/ngf/core-internal.h"
#include "ngf/event.h"

START_TEST (test_create)
{
    NCore *core = NULL;
    core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    ck_assert (core->conf_path != NULL);
    ck_assert (core->plugin_path != NULL);
    ck_assert (core->context != NULL);
    ck_assert (core->eventlist != NULL);
    ck_assert (core->key_types != NULL);

    n_core_free (core);
    core = NULL;
}
END_TEST

START_TEST (test_get_context)
{
    NCore *core = NULL;
    ck_assert (n_core_get_context (core) == NULL);
    core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    ck_assert (n_core_get_context (core) != NULL);
    NContext *context = n_context_new ();
    n_context_free (core->context);
    core->context = context;
    NContext *received_context = n_core_get_context (core);
    ck_assert (received_context != NULL);
    ck_assert (received_context == context);

    n_core_free (core);
    core = NULL;
}
END_TEST

START_TEST (test_get_requests)
{
    NCore *core = NULL;
    ck_assert (n_core_get_requests (core) == NULL);
    core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    ck_assert (n_core_get_requests (core) == NULL);
    NRequest *request = n_request_new ();
    GList *list = NULL;
    list = g_list_append (list, request);
    core->requests = list;
    GList *received_list = n_core_get_requests (core);
    ck_assert (received_list != NULL);
    ck_assert (received_list == list);

    n_core_free (core);
    core = NULL;
}
END_TEST

START_TEST (test_add_get_events)
{
    NCore *core = NULL;
    ck_assert (n_core_get_events (core) == NULL);
    core = n_core_new (NULL, NULL);
    ck_assert (core != NULL);
    ck_assert (n_core_get_events (core) == NULL);

    GKeyFile *keyfile = NULL;
    keyfile = g_key_file_new ();
    g_key_file_set_value (keyfile, "sms", "sink.null", "true");
    n_event_list_parse_keyfile (core->eventlist, keyfile);
    g_key_file_free (keyfile);

    GList *events = NULL;
    events = n_core_get_events (core);
    ck_assert (events != NULL);

    uint length = g_list_length (events);
    ck_assert (length == 1);
    NEvent *receivedEvent = (NEvent*)events->data;
    ck_assert (g_strcmp0 (receivedEvent->name, "sms") == 0);

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
    ck_assert (core != NULL);
    gboolean result = TRUE;
    int priority = 10;
    NCoreHook hook = N_CORE_HOOK_INIT_DONE;
    void *userdata = NULL;
    GList *list = NULL;
    
    list = core->hooks[hook].slots;
    ck_assert (g_list_length (list) == 0);
    list = NULL;

    /* connect tests */
    
    /* core is NULL */
    result = n_core_connect (NULL, hook, priority, callback, userdata);
    ck_assert (result == FALSE);
    
    /* callback is NULL */
    result = n_core_connect (core, hook, priority, NULL, userdata);
    ck_assert (result == FALSE);
    
    /* hook >= N_CORE_HOOK_LAST */
    hook = N_CORE_HOOK_LAST;
    result = n_core_connect (core, hook, priority, callback, userdata);
    ck_assert (result == FALSE);
    
    /* valid case */
    hook = N_CORE_HOOK_INIT_DONE;
    result = n_core_connect (core, hook, priority, callback, userdata);
    ck_assert (result == TRUE);
    list = core->hooks[hook].slots;
    ck_assert (list != NULL);
    ck_assert (g_list_length (list) == 1);

    /* disconnect tests */

    /* core is NULL */
    n_core_disconnect (NULL, hook, callback, userdata);
    list = core->hooks[hook].slots;
    ck_assert (list != NULL);
    ck_assert (g_list_length (list) == 1);
    list = NULL;

    /* callback is NULL */
    n_core_disconnect (core, hook, NULL, userdata);
    list = core->hooks[hook].slots;
    ck_assert (list != NULL);
    ck_assert (g_list_length (list) == 1);
    list = NULL;

    /* hook >= N_CORE_HOOK_LAST */
    hook = N_CORE_HOOK_LAST;
    n_core_disconnect (core, hook, callback, userdata);

    /* disconnect callback */
    hook = N_CORE_HOOK_INIT_DONE;
    n_core_disconnect (core, hook, callback, userdata);
    list = core->hooks[hook].slots;
    ck_assert (list == NULL);
    ck_assert (g_list_length (list) == 0);

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
