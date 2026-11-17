#include <glint/unit/unit.h>

static void gunit__test_fatal_record(void* user);

GUNIT_TEST(fatal_failure_runs_cleanup,
    int value = 9;
    gunit_defer(gunit__test_fatal_record, &value);
    gunit_require("intentional fatal failure", false);
)

gunit_main();

static void gunit__test_fatal_record(void* user) {
    fprintf(stderr, "fatal cleanup: %d\n", *(int*)user);
}
