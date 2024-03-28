#include <stdlib.h>
#include <stdio.h>
#include <check.h>

#include "src/include/ngf/proplist.h"
#define ARRAY_SIZE(array) (sizeof((array)) / sizeof((array)[0]))

START_TEST (test_match_exact)
{
    NProplist *proplistA = NULL;
    proplistA = n_proplist_new ();
    ck_assert (proplistA != NULL);
    NProplist *proplistB = NULL;
    proplistB = n_proplist_new ();
    ck_assert (proplistB != NULL);
    const char* key1 = "key1";
    const char* key2 = "key2";

    gboolean result = n_proplist_match_exact (NULL, proplistB);
    ck_assert (result == FALSE);
    result = n_proplist_match_exact (proplistA, NULL);
    ck_assert (result == FALSE);

    n_proplist_set_int (proplistA, key1, 100);
    result = n_proplist_match_exact (proplistA, proplistB);
    ck_assert (result == FALSE);
    n_proplist_set_int (proplistB, key1, 100);
    result = n_proplist_match_exact (proplistA, proplistB);
    ck_assert (result == TRUE);

    n_proplist_set_bool (proplistA, key2, TRUE);
    n_proplist_set_bool (proplistB, key2, FALSE);
    result = n_proplist_match_exact (proplistA, proplistB);
    ck_assert (result == FALSE);

    n_proplist_free (proplistA);
    proplistA = NULL;
    n_proplist_free (proplistB);
    proplistB = NULL;
}
END_TEST


START_TEST (test_copy)
{
    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    NProplist *copy = NULL;
    copy = n_proplist_copy (NULL);
    ck_assert (copy == NULL);
    copy = n_proplist_copy (proplist);
    ck_assert (copy != NULL);
    ck_assert (n_proplist_match_exact (proplist, copy) == TRUE);
    n_proplist_set_int (proplist, "key", 100);
    n_proplist_free (copy);
    copy = NULL;
    copy = n_proplist_copy (proplist);
    ck_assert (copy != NULL);
    ck_assert (n_proplist_match_exact (proplist, copy) == TRUE);
    
    n_proplist_free (copy);
    copy = NULL;
    n_proplist_free (proplist);
    proplist = NULL;
}
END_TEST

START_TEST (test_copy_keys)
{
    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    NProplist *keys = NULL;
    GList *list = NULL;
    keys = n_proplist_copy_keys (proplist, list);
    ck_assert (keys != NULL);
    ck_assert (n_proplist_is_empty (keys) == TRUE);
    n_proplist_free (keys);
    keys = NULL;
	    
    const char *key1 = "key1";
    const char *key2 = "key2";
    uint val_key2 = 100;
    list = g_list_append (list, (gpointer)key2);

    keys = n_proplist_copy_keys (NULL, list);
    ck_assert (keys != NULL);
    ck_assert (n_proplist_is_empty (keys) == TRUE);
    n_proplist_free (keys);
    keys = NULL;
    keys = n_proplist_copy_keys (proplist, NULL);
    ck_assert (keys != NULL);
    ck_assert (n_proplist_is_empty (keys) == TRUE);
    n_proplist_free (keys);
    keys = NULL;

    keys = n_proplist_copy_keys (proplist, list);
    ck_assert (keys != NULL);
    ck_assert (n_proplist_is_empty (keys) == TRUE);
    n_proplist_set_bool (proplist, key1, FALSE);
    n_proplist_set_uint (proplist, key2, val_key2);
    n_proplist_free (keys);
    keys = NULL;
    keys = n_proplist_copy_keys (proplist, list);
    ck_assert (keys != NULL);
    ck_assert (n_proplist_is_empty (keys) == FALSE);
    ck_assert (n_proplist_size (keys) == 1);
    ck_assert (n_proplist_get_uint (keys, key2) == val_key2);
    
    g_list_free (list);
    list = NULL;
    n_proplist_free (keys);
    keys = NULL;
    n_proplist_free (proplist);
    proplist = NULL;
}
END_TEST
void listing (const char*, const NValue*, gpointer);

START_TEST (test_merge)
{
    NProplist *source = NULL;
    source = n_proplist_new ();
    ck_assert (source != NULL);
    NProplist *target = NULL;
    target = n_proplist_new ();
    ck_assert (target != NULL);
    static char* keys[] = {"key1", "key2", "key3", "key4", "key5"};
    uint i = 0;
    for (i = 0; i < ARRAY_SIZE (keys); i++)
        n_proplist_set_string (target, keys[i], keys[i]);

    char *merge_key1 = keys[0];
    char *merge_key2 = keys[3];
    char *key1_value = keys[3];

    n_proplist_set_string (source, merge_key1, key1_value);
    n_proplist_set_int (source, merge_key2, i);
    char *new_key = "diff";
    n_proplist_set_bool (source, new_key, TRUE);

    NProplist *targetCopy = n_proplist_copy (target);
    NProplist *sourceCopy = n_proplist_copy (source);
    
    n_proplist_merge (NULL, source);
    ck_assert (n_proplist_has_key (target, new_key) == FALSE);
    ck_assert (n_proplist_match_exact (target, targetCopy) == TRUE);
    ck_assert (n_proplist_match_exact (source, sourceCopy) == TRUE);
    
    n_proplist_merge (target, NULL);
    ck_assert (n_proplist_has_key (target, new_key) == FALSE);
    ck_assert (n_proplist_match_exact (target, targetCopy) == TRUE);
    ck_assert (n_proplist_match_exact (source, sourceCopy) == TRUE);
    

    /* Two keys are common (will be replaces),
     * three will be added from source,
     * one will stay from target
     */
    n_proplist_merge (target, source);
    //n_proplist_foreach (target, listing, NULL);
    
    ck_assert (n_proplist_match_exact (target, targetCopy) == FALSE);
    ck_assert (n_proplist_match_exact (source, sourceCopy) == TRUE);
    ck_assert (n_proplist_size (target) == ARRAY_SIZE (keys) + 1);
    const char *merged1 = n_proplist_get_string (target, merge_key1);
    ck_assert (g_strcmp0 (key1_value, merged1) == 0);
    int merged2 = n_proplist_get_int (target, merge_key2);
    ck_assert (merged2 == ARRAY_SIZE (keys));
    
    n_proplist_free (sourceCopy);
    sourceCopy = NULL;
    n_proplist_free (targetCopy);
    targetCopy = NULL;
    n_proplist_free (source);
    source = NULL;
    n_proplist_free (target);
    target = NULL;
}
END_TEST

void 
listing (const char *key, const NValue *value, gpointer userdata)
{
	(void) userdata;
	char *string_value = n_value_to_string ((NValue*)value);
	printf ("\nKey = %s\t|\tValue = %s\n\n", key, string_value);
	g_free (string_value);
}


START_TEST (test_merge_keys)
{
    NProplist *source = NULL;
    source = n_proplist_new ();
    ck_assert (source != NULL);
    NProplist *target = NULL;
    target = n_proplist_new ();
    ck_assert (target != NULL);

    /* keys and values at the same time */
    static char* keys[] = {"key1", "key2", "key3", "key4",
                           "key5", "key6", "key7", "key8",
                           "key9", "key10", "key11", "key12"};

    /* int values for source proplist */
    int i_values[] = {1, 2, 3, 4, 5, 6};
    /* amount of elements in GList */
    uint n_list = ARRAY_SIZE (keys) / 4;
    uint i = 0;
    /* set s_values to target & set i_values to source */
    for (i = 0; i < ARRAY_SIZE (keys); i++)
    {
        n_proplist_set_string (target, keys[i], keys[i]);
        if (i < ARRAY_SIZE (keys) / 2)
            n_proplist_set_int (source, keys[i], i_values[i]);
    }
    /* set list 0f three keys accepted for merging */
    GList *list = NULL;
    for (i = 0; i < n_list; i++)
    {
        list = g_list_append (list, (gpointer)keys[i]);
    }

    /* copy of proplist for comparision */
    NProplist *sourceCopy = n_proplist_copy (source);
    NProplist *targetCopy = n_proplist_copy (target);
    
    n_proplist_merge_keys (NULL, source, list);
    ck_assert (n_proplist_match_exact (source, sourceCopy) == TRUE);
    ck_assert (n_proplist_match_exact (target, targetCopy) == TRUE);
    
    n_proplist_merge_keys (target, NULL, list);
    ck_assert (n_proplist_match_exact (source, sourceCopy) == TRUE);
    ck_assert (n_proplist_match_exact (target, targetCopy) == TRUE);
    
    /* copy of target proplist to check merge_keys with NULL GList = normal merge in the next line */
    NProplist *secondTarget = n_proplist_copy (target);
    /* first half of seconfTarget will be fill with ints from source */
    n_proplist_merge_keys (secondTarget, source, NULL);
    //n_proplist_foreach (secondTarget, listing, NULL);
    ck_assert (n_proplist_match_exact (target, secondTarget) == FALSE);
    for (i = 0; i < ARRAY_SIZE (keys); i++)
    {
        if (i < ARRAY_SIZE (keys) / 2)
        {
            int result = n_proplist_get_int (secondTarget, keys[i]);
            ck_assert (result == i_values[i]);
        }
        else
        {
            const char *result = n_proplist_get_string (secondTarget, keys[i]);
            ck_assert (g_strcmp0 (result, keys[i]) == 0);
        }
    }
    
    /* half elements from source will be transfer to the target 
     * first 1/4 of the elements from target will be int type */
    n_proplist_merge_keys (target, source, list);
    //n_proplist_foreach (target, listing, NULL);
    ck_assert (n_proplist_match_exact (target, targetCopy) == FALSE);
    ck_assert (n_proplist_match_exact (source, sourceCopy) == TRUE);

    for (i = 0; i < ARRAY_SIZE (keys); i++)
    {
        if (i < n_list)
	{
            int result = n_proplist_get_int (target, keys[i]);
            ck_assert (result == i_values[i]);
	}
	else
	{
            const char *result = n_proplist_get_string (target, keys[i]);
        ck_assert (g_strcmp0 (result, keys[i]) == 0);
	}
    }
    
    n_proplist_free (secondTarget);
    secondTarget = NULL;
    n_proplist_free (targetCopy);
    targetCopy = NULL;
    n_proplist_free (sourceCopy);
    sourceCopy = NULL;
    n_proplist_free (target);
    target = NULL;
    n_proplist_free (source);
    source = NULL;
}
END_TEST

START_TEST (test_size)
{
    const char *key = "ngf";
    NProplist *proplist = NULL;
    int size = n_proplist_size (proplist);
    ck_assert (size == 0);
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    size = n_proplist_size (proplist);
    ck_assert (size == 0);
    n_proplist_set_pointer (proplist, key, proplist);
    size = n_proplist_size (proplist);
    ck_assert (size == 1);

    n_proplist_free (proplist);
    proplist = NULL;
}
END_TEST

void
n_proplist_Callback (const char *key, const NValue *value, gpointer data)
{
    (void) key;
    (void) value;
    int *check = data;
    (*check)++;
}

START_TEST (test_foreach)
{
    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    uint i = 0, check_n = 0;
    char* keys[] = { "random", "data", "keys"};
    for (i = 0; i < ARRAY_SIZE (keys); i++)
        n_proplist_set_string (proplist, keys[i], keys[i]);
    n_proplist_foreach (proplist, n_proplist_Callback, &check_n);
    ck_assert (check_n == ARRAY_SIZE (keys));

    n_proplist_free (proplist);
    proplist = NULL;
}
END_TEST

START_TEST (test_is_empty)
{
    const char *key = "ngf";
    NProplist *proplist = NULL;
    gboolean result = n_proplist_is_empty (proplist);
    ck_assert (result == FALSE);
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    result = n_proplist_is_empty (proplist);
    ck_assert (result == TRUE);
    n_proplist_set_pointer (proplist, key, proplist);
    result = n_proplist_is_empty (proplist);
    ck_assert (result == FALSE);

    n_proplist_free (proplist);
    proplist = NULL;
}
END_TEST

START_TEST (test_has_key)
{
    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    const char *key = "key";
    gboolean result = n_proplist_has_key (proplist, key);
    ck_assert (result == FALSE);
    result = n_proplist_has_key (NULL, key);
    ck_assert (result == FALSE);
//    result = n_proplist_has_key (proplist, NULL);
//    ck_assert (result == FALSE);
    
    n_proplist_set_int (proplist, key, 100);
    result = n_proplist_has_key (proplist, key);
    ck_assert (result == TRUE);

    n_proplist_free (proplist);
    proplist = NULL;
}
END_TEST

START_TEST (test_set_get_unset)
{
    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    NValue *value = NULL;
    value = n_value_new ();
    n_value_set_int (value, 100);
    NValue *result = NULL;
    const char *key = "key";
    
    // these should fail to set
    n_proplist_set (NULL, key, n_value_copy (value));
    ck_assert (n_proplist_is_empty (proplist) == TRUE);
    n_proplist_set (proplist, NULL, n_value_copy (value));
    ck_assert (n_proplist_is_empty (proplist) == TRUE);
    n_proplist_set (proplist, key, NULL);
    ck_assert (n_proplist_is_empty (proplist) == TRUE);
    // this should succeed
    n_proplist_set (proplist, key, value);
    ck_assert (n_proplist_size (proplist) == 1);

    result = n_proplist_get (NULL, key);
    ck_assert (result == NULL);
    result = n_proplist_get (proplist, NULL);
    ck_assert (result == NULL);
    result = n_proplist_get (proplist, key);
    ck_assert (result == value);
    ck_assert (n_value_equals (result, value) == TRUE);

    n_proplist_unset (NULL, key);
    ck_assert (n_proplist_size (proplist) == 1);
    n_proplist_unset (proplist, NULL);
    ck_assert (n_proplist_size (proplist) == 1);
    n_proplist_unset (proplist, key);
    ck_assert (n_proplist_is_empty (proplist) == TRUE);
    ck_assert (n_proplist_size (proplist) == 0);

    n_proplist_free (proplist);
    proplist = NULL;
    value = NULL;
}
END_TEST


START_TEST (test_proplist_values)
{
    NProplist *proplist = NULL;
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    const char *key = "ngf";
    const char *value_str = "ValuE";

    /* string */    
    n_proplist_set_string (NULL, key, value_str);
    ck_assert (n_proplist_size (proplist) == 0);
    n_proplist_set_string (proplist, NULL, value_str);
    ck_assert (n_proplist_size (proplist) == 0);
    n_proplist_set_string (proplist, key, NULL);
    ck_assert (n_proplist_size (proplist) == 0);

    n_proplist_set_string (proplist, key, value_str);
    ck_assert (n_proplist_size (proplist) == 1);
    const char *result_str = n_proplist_get_string (proplist, key);
    ck_assert (result_str != NULL);
    ck_assert (g_strcmp0 (result_str, value_str) == 0);
    ck_assert (n_proplist_get_string (NULL, key) == NULL);
    ck_assert (n_proplist_get_string (proplist, NULL) == NULL);

    /* duplicate string */
    char *duplicate = n_proplist_dup_string (proplist, key);
    ck_assert (duplicate != NULL);
    ck_assert (g_strcmp0 (duplicate, value_str) == 0);

    n_proplist_free (proplist);
    proplist = NULL;
    g_free (duplicate);
    duplicate = NULL;

    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    int value = -100;

    /* int */
    n_proplist_set_int (NULL, key, value);
    ck_assert (n_proplist_size (proplist) == 0);
    n_proplist_set_int (proplist, NULL, value);
    ck_assert (n_proplist_size (proplist) == 0);

    n_proplist_set_int (proplist, key, value);
    ck_assert (n_proplist_size (proplist) == 1);
    int result_int = n_proplist_get_int (proplist, key);
    ck_assert (result_int == value);
    ck_assert (n_proplist_get_int (NULL, key) == 0);
    ck_assert (n_proplist_get_int (proplist, NULL) == 0);

    n_proplist_free (proplist);
    proplist = NULL;

    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    uint value_uint = 100;

    /* uint */
    n_proplist_set_uint (NULL, key, value_uint);
    ck_assert (n_proplist_size (proplist) == 0);
    n_proplist_set_uint (proplist, NULL, value_uint);
    ck_assert (n_proplist_size (proplist) == 0);
    
    n_proplist_set_uint (proplist, key, value_uint);
    ck_assert (n_proplist_size (proplist) == 1);
    uint result_uint = n_proplist_get_uint (proplist, key);
    ck_assert (result_uint == value_uint);
    ck_assert (n_proplist_get_uint (NULL, key) == 0);
    ck_assert (n_proplist_get_uint (proplist, NULL) == 0);

    n_proplist_free (proplist);
    proplist = NULL;
    
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    gboolean value_bool = TRUE;

    /* bool */
    n_proplist_set_bool (NULL, key, value_bool);
    ck_assert (n_proplist_size (proplist) == 0);
    n_proplist_set_bool (proplist, NULL, value_bool);
    ck_assert (n_proplist_size (proplist) == 0);
    
    n_proplist_set_bool (proplist, key, value_bool);
    ck_assert (n_proplist_size (proplist) == 1);
    gboolean result_bool = n_proplist_get_bool (proplist, key);
    ck_assert (result_bool == value_bool);
    ck_assert (n_proplist_get_bool (NULL, key) == FALSE);
    ck_assert (n_proplist_get_bool (proplist, NULL) == FALSE);

    n_proplist_free (proplist);
    proplist = NULL;
    
    proplist = n_proplist_new ();
    ck_assert (proplist != NULL);
    gpointer value_gpointer = proplist;
    
    /* gpointer */
    n_proplist_set_pointer (NULL, key, value_gpointer);
    ck_assert (n_proplist_size (proplist) == 0);
    n_proplist_set_pointer (proplist, NULL, value_gpointer);
    ck_assert (n_proplist_size (proplist) == 0);
//    n_proplist_set_pointer (proplist, key, NULL);
//    ck_assert (n_proplist_size (proplist) == 0);
    
    n_proplist_set_pointer (proplist, key, value_gpointer);
    ck_assert (n_proplist_size (proplist) == 1);
    gpointer result_gpointer = n_proplist_get_pointer (proplist, key);
    ck_assert (result_gpointer == value_gpointer);
    ck_assert (n_proplist_get_pointer (NULL, key) == 0);
    ck_assert (n_proplist_get_pointer (proplist, NULL) == 0);

    n_proplist_free (proplist);
    proplist = NULL;
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

    s = suite_create ("\tProplist tests");

    tc = tcase_create ("match exact");
    tcase_add_test (tc, test_match_exact);
    suite_add_tcase (s, tc);

    tc = tcase_create ("copy");
    tcase_add_test (tc, test_copy);
    suite_add_tcase (s, tc);

    tc = tcase_create ("copy keys");
    tcase_add_test (tc, test_copy_keys);
    suite_add_tcase (s, tc);

    tc = tcase_create ("merge");
    tcase_add_test (tc, test_merge);
    suite_add_tcase (s, tc);
    
    tc = tcase_create ("merge keys");
    tcase_add_test (tc, test_merge_keys);
    suite_add_tcase (s, tc);

    tc = tcase_create ("size");
    tcase_add_test (tc, test_size);
    suite_add_tcase (s, tc);

    tc = tcase_create ("foreach");
    tcase_add_test (tc, test_foreach);
    suite_add_tcase (s, tc);

    tc = tcase_create ("is empty");
    tcase_add_test (tc, test_is_empty);
    suite_add_tcase (s, tc);

    tc = tcase_create ("has key");
    tcase_add_test (tc, test_has_key);
    suite_add_tcase (s, tc);

    tc = tcase_create ("set, get, unset value");
    tcase_add_test (tc, test_set_get_unset);
    suite_add_tcase (s, tc);

    tc = tcase_create ("values - get, set");
    tcase_add_test (tc, test_proplist_values);
    suite_add_tcase (s, tc);

    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    num_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
