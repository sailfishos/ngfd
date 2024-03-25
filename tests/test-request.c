#include <stdlib.h>
#include <check.h>

#include "src/include/ngf/request.h"
#include "src/include/ngf/proplist.h"

START_TEST (test_create)
{
    NRequest *request = NULL;
    request = n_request_new ();
    ck_assert (request != NULL);
    ck_assert (n_request_get_name (request) == NULL);
    n_request_free (request);
    request = NULL;

    /* create request with event name */
    const char *event = "event";
    request = n_request_new_with_event (event);
    ck_assert (request != NULL);
    const char *name = n_request_get_name (request);
    ck_assert (g_strcmp0 (name, event) == 0);
    n_request_free (request);
    request = NULL;

    /* create request with event name and proplist */
    const char *key = "key";
    int value = -100;
    NProplist *proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    n_proplist_set_int (proplist, key, value);
    request = n_request_new_with_event_and_properties (NULL, proplist);
    ck_assert (request == NULL);
    request = n_request_new_with_event_and_properties (event, NULL);
    ck_assert (request != NULL);
    ck_assert (n_request_get_properties (request) == NULL);
    n_request_free (request);
    request = NULL;
    
    request = n_request_new_with_event_and_properties (event, proplist);
    const NProplist *set = n_request_get_properties (request);
    ck_assert (n_proplist_match_exact (set, proplist) == TRUE);
    name = NULL;
    name = n_request_get_name (request);
    ck_assert (g_strcmp0 (name, event) == 0);
    
    n_proplist_free (proplist);
    proplist = NULL;
    n_request_free (request);
    request = NULL;
}
END_TEST

START_TEST (test_properties)
{
    NRequest *request = NULL;
    request = n_request_new ();
    ck_assert (request != NULL);
    NProplist *proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    const char *key = "key";
    /* set pointer to proplist */
    n_proplist_set_pointer (proplist, key, (gpointer)request);
    /* get properties from NULL request */
    ck_assert (n_request_get_properties (NULL) == NULL);
    
    n_request_set_properties (NULL, proplist);
    ck_assert (n_request_get_properties (request) == NULL);
    
    n_request_set_properties (request, NULL);
    ck_assert (n_request_get_properties (request) == NULL);
    
    n_request_set_properties (request, proplist);
    const NProplist *set = n_request_get_properties (request);
    ck_assert (n_proplist_match_exact (set, proplist) == TRUE);

    n_request_set_properties (NULL, proplist);
    set = NULL;
    set = n_request_get_properties (request);
    ck_assert (n_proplist_match_exact (set, proplist) == TRUE);
    n_request_set_properties (request, NULL);
    set = NULL;
    set = n_request_get_properties (request);
    ck_assert (n_proplist_match_exact (set, proplist) == TRUE);
    
    n_proplist_free (proplist);
    proplist = NULL;
    n_request_free (request);
    request = NULL;
}
END_TEST

START_TEST (test_data)
{
    NRequest *request = NULL;
    request = n_request_new ();
    ck_assert (request != NULL);
    const char *key = "key";
    NProplist *proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    n_request_set_properties (request, proplist);
    n_proplist_free (proplist);
    proplist = NULL;

    ck_assert (n_request_get_data (NULL, key) == NULL);
    ck_assert (n_request_get_data (request, NULL) == NULL);
    ck_assert (n_request_get_data (request, key) == NULL);

    void *data;
    n_request_store_data (NULL, key, request);
    data = n_request_get_data (request, key);
    ck_assert (data == NULL);
    n_request_store_data (request, NULL, request);
    data = n_request_get_data (request, key);
    ck_assert (data == NULL);
    n_request_store_data (request, key, request);
    /* verification */
    data = n_request_get_data (request, key);
    ck_assert (data != NULL);
    ck_assert (data == request);

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

    s = suite_create ("\tRequest tests");

    tc = tcase_create ("Create");
    tcase_add_test (tc, test_create);
    suite_add_tcase (s, tc);

    tc = tcase_create ("properties in request - set and get");
    tcase_add_test (tc, test_properties);
    suite_add_tcase (s, tc);

    tc = tcase_create ("store and get data");
    tcase_add_test (tc, test_data);
    suite_add_tcase (s, tc);

    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    num_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
