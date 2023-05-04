// See https://nemequ.github.io/munit 
#include "munit/munit.h"

// your includes here...



// FIRST, individual tests. Note that each may test multiple cases within it
static MunitResult random_positive_tests(const MunitParameter params[], void *data) {
  munit_assert_true(5==5);
  return MUNIT_OK;
}

static MunitResult random_negative_tests(const MunitParameter params[], void *data) {
  munit_assert_false(5 < 0);
  return MUNIT_OK;
}

// SECOND, compile them into AT LEAST one list of tests. OK to split to two lists.
MunitTest tests[] = {{
                         "/negative-stuff",       /* name */
                         random_negative_tests,     /* test */
                         NULL,                   /* setup */
                         NULL,                   /* tear_down */
                         MUNIT_TEST_OPTION_NONE, /* options */
                         NULL                    /* parameters */
                     },
                     {
                         "/positive-stuff",              /* name */
                         random_positive_tests,            /* test */
                         NULL,                   /* setup */
                         NULL,                   /* tear_down */
                         MUNIT_TEST_OPTION_NONE, /* options */
                         NULL                    /* parameters */
                     },
                     /* Mark the end of the array with an entry where the test
                      * function is NULL */
                     {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

// WE HAVE ONE PRIMARY SUITE OF TESTS...
// but that could include nested suites if you wanted.
// If you do that, probably don't include tests in the primary suite
static const MunitSuite primary_test_suite = {
    "/primary",             /* name */
    tests,                  /* tests */
    NULL,                   /* nested suites */
    1,                      /* iterations */
    MUNIT_SUITE_OPTION_NONE /* options */
};

int execute_unit_tests(int argc, char *argv[]) {
  return munit_suite_main(&primary_test_suite, NULL, argc, argv);
}

int main(int argc, char *argv[]) {
  return execute_unit_tests(argc, argv);
}


