#pragma once

#include <glint/config.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#if GLINT_COMPILER_MSVC && !GLINT_COMPILER_CLANG
    #pragma section(".CRT$XCU", read)
    #define GUNIT_INITIALIZER_DETAIL(f, prefix) \
    static void f(void); \
    __declspec(allocate(".CRT$XCU")) void (*f##_)(void) = f; \
    __pragma(comment(linker, "/include:" prefix #f "_")) static void f(void)
    #if GLINT_PLATFORM_WINDOWS_64
        #define GUNIT_INITIALIZER(f) GUNIT_INITIALIZER_DETAIL(f, "")
    #else
        #define GUNIT_INITIALIZER(f) GUNIT_INITIALIZER_DETAIL(f, "_")
    #endif
#else
    #define GUNIT_INITIALIZER(f) \
    static void f(void) __attribute__((constructor)); \
    static void f(void)
#endif

#ifndef GUNIT_DEFER_SIZE
    #define GUNIT_DEFER_SIZE 32
#endif

#if GUNIT_DEFER_SIZE < 1
    #error "GUNIT_DEFER_SIZE must be positive"
#endif

typedef void (*gunit_DeferFn)(void* user);

typedef struct gunit__Deferred {
    gunit_DeferFn func;
    void* user;
} gunit__Deferred;

typedef struct gunit_Test {
    void (*func)(void);
    const char* name;
    struct gunit_Test* next;
} gunit_Test;

static gunit_Test* gunit_first;
static gunit_Test* gunit_last;
static bool gunit__failed;
static int gunit_argc;
static char** gunit_argv;

static inline void gunit_register(gunit_Test* test) {
    if (gunit_last != NULL) {
        gunit_last->next = test;
    } else {
        gunit_first = test;
    }

    gunit_last = test;
}

/// Record a failed check. Helpers return their own failure status to the test.
static inline void gunit_fail(const char* file, int line, const char* condition, const char* message) {
    fprintf(stderr, "\n    %s:%d: check `%s` failed: %s\n", file, line, condition, message);
    gunit__failed = true;
}

/// Check a helper or callback condition without changing control flow.
/// The caller must handle false and propagate its own failure status.
#define gunit_check(message, ...) \
    ((__VA_ARGS__) ? true : (gunit_fail(__FILE__, __LINE__, #__VA_ARGS__, (message)), false))

#define gunit__drain_defers() \
    do { \
        while (gunit__defer_count > 0) { \
            gunit__defer_count -= 1; \
            gunit__Deferred gunit__deferred = gunit__entries[gunit__defer_count]; \
            gunit__deferred.func(gunit__deferred.user); \
        } \
    } while (0)

/// Drain this test's stack-local callbacks in reverse order, then return directly.
/// Use for an early exit; GUNIT_TEST also calls this automatically at the end.
#define gunit_return(...) \
    do { \
        while (gunit__defer_count > 0) { \
            gunit__defer_count -= 1; \
            gunit__entries[gunit__defer_count].func(gunit__entries[gunit__defer_count].user); \
        } \
        return __VA_ARGS__; \
    } while (0)

/// Register a borrowed cleanup argument on this test's stack.
/// Define GUNIT_DEFER_SIZE before including this header to override its capacity.
#define gunit_defer(callback, argument) \
    do { \
        gunit_DeferFn gunit__callback = (callback); \
        void* gunit__argument = (argument); \
        if (!gunit_check("cleanup callback is required", gunit__callback != NULL)) { \
            gunit_return(); \
        } \
        if (!gunit_check("GUNIT_DEFER_SIZE exceeded", gunit__defer_count < GUNIT_DEFER_SIZE)) { \
            gunit_return(); \
        } \
        gunit__entries[gunit__defer_count] = (gunit__Deferred) {.func = gunit__callback, .user = gunit__argument}; \
        gunit__defer_count += 1; \
    } while (0)

/// Drain this test's callbacks and terminate the executable on failure.
#define gunit_require(message, ...) \
    do { \
        if (!gunit_check(message, __VA_ARGS__)) { \
            gunit__drain_defers(); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

// Variadic expressions allow compound literals containing commas.
#define gunit_assert(message, ...) \
    do { \
        if (!gunit_check(message, __VA_ARGS__)) { \
            gunit_return(); \
        } \
    } while (0)

#ifndef GUNIT_TEST_NAME
    #define GUNIT_TEST_NAME(name) name
#endif

// Allow unity runners to scope symbols while retaining the original display name.
// Pass statements directly, without an enclosing block, so locals remain alive
// when the automatic gunit_return drains their deferred callbacks.
#define GUNIT_TEST(test_name, ...) GUNIT_TEST_EXPAND(GUNIT_TEST_NAME(test_name), #test_name, __VA_ARGS__)
#define GUNIT_TEST_EXPAND(test_name, display_name, ...) GUNIT_TEST_DETAIL(test_name, display_name, __VA_ARGS__)
#define GUNIT_TEST_DETAIL(test_name, display_name, ...) \
    static void test_##test_name(void) { \
        gunit__Deferred gunit__entries[GUNIT_DEFER_SIZE]; \
        size_t gunit__defer_count = 0; \
        __VA_ARGS__; \
        gunit_return(); \
    } \
    static gunit_Test gunit_case_##test_name = {test_##test_name, display_name, NULL}; \
    GUNIT_INITIALIZER(register_##test_name) { \
        gunit_register(&gunit_case_##test_name); \
    }

#define gunit_main(...) \
    int main(int argc, char** argv) { \
        gunit_argc = argc; \
        gunit_argv = argv; \
        __VA_ARGS__; \
        size_t count = 0; \
        size_t passed = 0; \
        for (gunit_Test* test = gunit_first; test != NULL; test = test->next) { \
            count += 1; \
            printf("    Running '%s'... ", test->name); \
            fflush(stdout); \
            gunit__failed = false; \
            test->func(); \
            if (gunit__failed) { \
                puts("FAIL"); \
            } else { \
                puts("OK"); \
                passed += 1; \
            } \
        } \
        printf("\n    Passed: %zu/%zu tests.\n", passed, count); \
        return passed == count ? EXIT_SUCCESS : EXIT_FAILURE; \
    }
