#define GUNIT_DEFER_SIZE 3
#include <glint/unit/unit.h>

// Registration must not depend on prior uses of the compiler counter.
enum { unrelated_counter = __COUNTER__ };

typedef struct gunit__TestCleanup {
    const char* name;
    int order[3];
    int expected[3];
    size_t count;
    size_t expected_count;
} gunit__TestCleanup;

typedef struct gunit__TestRecord {
    gunit__TestCleanup* cleanup;
    int value;
} gunit__TestRecord;

static void gunit__test_record(void* user);
static void gunit__test_verify(void* user);
static void gunit__test_cleanup_failure(void* user);
static bool gunit__test_assert_helper(void);

GUNIT_TEST(failure,
    gunit__TestCleanup cleanup = {.name = "failure", .expected = {2, 1}, .expected_count = 2};
    gunit__TestRecord first = {.cleanup = &cleanup, .value = 1};
    gunit__TestRecord second = {.cleanup = &cleanup, .value = 2};

    gunit_defer(gunit__test_verify, &cleanup);
    gunit_defer(gunit__test_record, &first);
    gunit_defer(gunit__test_record, &second);

    gunit_assert("intentional failure", false);
)

GUNIT_TEST(explicit_return,
    gunit__TestCleanup cleanup = {.name = "return", .expected = {3}, .expected_count = 1};
    gunit__TestRecord record = {.cleanup = &cleanup, .value = 3};

    gunit_defer(gunit__test_verify, &cleanup);
    gunit_defer(gunit__test_record, &record);

    gunit_return();
    gunit_assert("early return stops the body", false);
)

GUNIT_TEST(fallthrough,
    gunit__TestCleanup cleanup = {.name = "fallthrough", .expected = {4}, .expected_count = 1};
    gunit__TestRecord record = {.cleanup = &cleanup, .value = 4};

    gunit_defer(gunit__test_verify, &cleanup);
    gunit_defer(gunit__test_record, &record);
)

GUNIT_TEST(overflow,
    gunit__TestCleanup cleanup = {.name = "overflow", .expected = {6, 5}, .expected_count = 2};
    gunit__TestRecord first = {.cleanup = &cleanup, .value = 5};
    gunit__TestRecord second = {.cleanup = &cleanup, .value = 6};

    gunit_defer(gunit__test_verify, &cleanup);
    gunit_defer(gunit__test_record, &first);
    gunit_defer(gunit__test_record, &second);
    gunit_defer(gunit__test_record, &first);
    gunit_assert("overflow stops the body", false);
)

GUNIT_TEST(cleanup_failure,
    gunit__TestCleanup cleanup = {.name = "cleanup-failure", .expected = {8}, .expected_count = 1};
    gunit__TestRecord record = {.cleanup = &cleanup, .value = 8};

    gunit_defer(gunit__test_verify, &cleanup);
    gunit_defer(gunit__test_record, &record);
    gunit_defer(gunit__test_cleanup_failure, NULL);
)

GUNIT_TEST(helper_assertion,
    gunit__TestCleanup cleanup = {.name = "helper", .expected = {10}, .expected_count = 1};
    gunit__TestRecord record = {.cleanup = &cleanup, .value = 10};

    gunit_defer(gunit__test_verify, &cleanup);
    gunit_defer(gunit__test_record, &record);

    gunit_assert("helper failure propagates to its caller", gunit__test_assert_helper());
    gunit_assert("caller failure stops the test", false);
)

GUNIT_TEST(empty_defer_stack,
    gunit_assert("arguments reach cases", gunit_argc == 2 && gunit_argv[1][0] == 'x');
)

GUNIT_TEST(mixed_sections,
    GUNIT_SECTION("first fails", {
        gunit_assert("intentional section failure", false);
    })

    GUNIT_SECTION("second passes", {
        gunit_assert("the next section gets a fresh result", true);
    })
)

GUNIT_TEST(passing_sections,
    GUNIT_SECTION("one", {})
    GUNIT_SECTION("two", {})
)

GUNIT_TEST(discovery_failure,
    GUNIT_SECTION("unreachable", {})
    gunit_assert("intentional discovery failure", false);
)

gunit_main();

static void gunit__test_record(void* user) {
    gunit__TestRecord* record = user;
    record->cleanup->order[record->cleanup->count] = record->value;
    record->cleanup->count += 1;
}

static void gunit__test_verify(void* user) {
    gunit__TestCleanup* cleanup = user;
    if (!gunit_check("registered cleanup count matches", cleanup->count == cleanup->expected_count)) {
        return;
    }
    for (size_t i = 0; i < cleanup->count; i++) {
        if (!gunit_check("registered cleanup runs in reverse order", cleanup->order[i] == cleanup->expected[i])) {
            return;
        }
    }
    fprintf(stderr, "verified %s cleanup\n", cleanup->name);
}

static void gunit__test_cleanup_failure(void* user) {
    (void)user;
    (void)gunit_check("intentional cleanup failure", false);
}

static bool gunit__test_assert_helper(void) {
    return gunit_check("intentional helper failure", false);
}
