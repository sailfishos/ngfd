#include <stdlib.h>
#include <check.h>

#include "src/include/ngf/event.h"
#include "src/ngf/event-internal.h"
#include "output.c"

START_TEST (test_create)
{
    NEvent *event = NULL;
    event = n_event_new ();
    ck_assert (event != NULL);

    n_event_free (event);
    event = NULL;
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

	s = suite_create ("\tEvent tests");

	tc = tcase_create ("");
	tcase_add_test (tc, test_create);
	suite_add_tcase (s, tc);

	sr = srunner_create (s);
	srunner_run_all (sr, CK_NORMAL);
	num_failed = srunner_ntests_failed (sr);
	srunner_free (sr);

	return num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
