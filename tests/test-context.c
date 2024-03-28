#include <stdlib.h>
#include <check.h>

#include "src/ngf/context.c"
#include "ngf/value.h"

START_TEST (test_set_get_value)
{
    NContext *context = NULL;
    context = n_context_new ();
    ck_assert (context != NULL);

    const char *key = "key";
    NValue *value = NULL;
    value = n_value_new ();
    n_value_init (value);
    n_value_set_int (value, -100);

    /* gets value with some NULL arguments */
    ck_assert (n_context_get_value (context, NULL) == NULL);
    ck_assert (n_context_get_value (NULL, key) == NULL);

    /* sets value sith some NULL arguments */
    n_context_set_value (NULL, key, n_value_copy (value));
    ck_assert (n_context_get_value (context, key) == NULL);
    n_context_set_value (context, NULL, n_value_copy (value));
    ck_assert (n_context_get_value (context, key) == NULL);

    /* calls set value with valid arguments */
    n_context_set_value (context, key, value);
    const NValue *ret_val = n_context_get_value (context, key);

    ck_assert (n_value_equals (ret_val, value) == TRUE);

    value = NULL;
    n_context_free (context);
    context = NULL;
}
END_TEST

void n_context_callback (NContext *context, const char *key, const NValue *old_value, const NValue *new_value, void *userdata)
{
    (void) context;
    (void) key;
    (void) old_value;
    (void) new_value;
    (void) userdata;
}

START_TEST (test_subscribe_unsubscribe_value_change)
{
    NContext *context = NULL;
    context = n_context_new ();
    ck_assert (context != NULL);
    const char *key = "key";
    gboolean result = TRUE;
    int item = 0;

    /* subscribe */
    result = n_context_subscribe_value_change (NULL, key, n_context_callback, NULL);
    ck_assert (result == FALSE);
    item = g_hash_table_size (context->keys);
    ck_assert (item == 0);
    result = n_context_subscribe_value_change (context, key, NULL, NULL);
    ck_assert (result == FALSE);
    item = g_hash_table_size (context->keys);
    ck_assert (item == 0);
    /* proper subscribtion */
    result = n_context_subscribe_value_change (context, key, n_context_callback, NULL);
    ck_assert (result == TRUE);
    item = g_hash_table_size (context->keys);
    ck_assert (item == 1);

    /* unsubscribe */
    n_context_unsubscribe_value_change (NULL, key, n_context_callback);
    item = g_hash_table_size (context->keys);
    ck_assert (item == 1);
    n_context_unsubscribe_value_change (context, NULL, n_context_callback);
    item = g_hash_table_size (context->keys);
    ck_assert (item == 1);
    n_context_unsubscribe_value_change (context, key, NULL);
    item = g_hash_table_size (context->keys);
    ck_assert (item == 1);
    /* proper unsubscribtion */
    n_context_unsubscribe_value_change (context, key, n_context_callback);
    item = g_hash_table_size (context->keys);
    ck_assert (item == 0);

    /* subscribe callback to key=NULL */
    result = n_context_subscribe_value_change (context, NULL, n_context_callback, NULL);
    ck_assert (result == TRUE);
    item = g_list_length (context->all_keys);
    ck_assert (item == 1);
    /*
     * TODO: unsubscribe callback when key=NULL
     * */

    n_context_free (context);
    context = NULL;
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

    s = suite_create ("\tContext tests");

    tc = tcase_create ("set and get value");
    tcase_add_test (tc, test_set_get_value);
    suite_add_tcase (s, tc);

    tc = tcase_create ("test subscribe & unsubscribe value change");
    tcase_add_test (tc, test_subscribe_unsubscribe_value_change);
    suite_add_tcase (s, tc);

    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    num_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
