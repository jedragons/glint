#pragma once

#include "../platform/mock.h"

#include <glint/unit/unit.h>

typedef struct gtest__Suite {
    gunit_Test* first;
    gunit_Test* last;
} gtest__Suite;

static inline void gtest__register(gtest__Suite* suite, gunit_Test* test) {
    if (suite->last != NULL) {
        suite->last->next = test;
    } else {
        suite->first = test;
    }
    suite->last = test;
}

#define GTEST_JOIN_DETAIL(prefix, name) prefix##name
#define GTEST_JOIN(prefix, name) GTEST_JOIN_DETAIL(prefix, name)
#define GTEST_SYMBOL(name) GTEST_JOIN(GTEST_PREFIX, name)

// Each included file keeps its own registry and entry point.
#undef GUNIT_TEST_NAME
#define GUNIT_TEST_NAME(name) GTEST_SYMBOL(name)
#define gunit_register(test) gtest__register(&GTEST_SUITE, (test))
#define gunit_first GTEST_SUITE.first

#define GTEST_PLATFORM_DECLARE(result, name, params) result GTEST_SYMBOL(name) params;
