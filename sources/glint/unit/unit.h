#pragma once

#include <glint/config.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef enum gunit_RunKind {
    GUNIT_RUN_EXPLORE = 0,
    GUNIT_RUN_EXECUTE = 1,
} gunit_RunKind;

typedef struct gunit_Args {
    gunit_RunKind kind;
    const char* section;
    const char** sections;
    size_t section_count;
    size_t section_capacity;
} gunit_Args;

typedef struct gunit_Test {
    const char* (*func)(gunit_Args*);
    const char* name;
    struct gunit_Test* next;
} gunit_Test;

static gunit_Test* gunit_first;
static gunit_Test* gunit_last;
static const char* gunit__failure;
static char* gunit__diagnostic;
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

// A returned diagnostic stays valid until the next test invocation.
static inline void gunit__reset_result(void) {
    free(gunit__diagnostic);
    gunit__diagnostic = NULL;
    gunit__failure = NULL;
}

/// Keep the first failure, including messages borrowed from helper or callback locals.
static inline void gunit_fail(const char* file, int line, const char* condition, const char* message) {
    if (gunit__failure != NULL) {
        return;
    }

    gunit__failure = "Could not format the failed check";
    const char* format = "%s:%d: check `%s` failed: %s";
    int length = snprintf(NULL, 0, format, file, line, condition, message);
    if (length >= 0) {
        size_t size = (size_t)length + 1;
        gunit__diagnostic = malloc(size);
        if (gunit__diagnostic != NULL) {
            snprintf(gunit__diagnostic, size, format, file, line, condition, message);
            gunit__failure = gunit__diagnostic;
        }
    }
}

static inline bool gunit_Args__append_section(gunit_Args* args, const char* section) {
    if (args->section_count == args->section_capacity) {
        if (args->section_capacity > SIZE_MAX / sizeof(*args->sections) / 2) {
            return false;
        }
        size_t capacity = args->section_capacity == 0 ? 16 : args->section_capacity * 2;
        const char** sections = realloc(args->sections, capacity * sizeof(*sections));
        if (sections == NULL) {
            return false;
        }
        args->sections = sections;
        args->section_capacity = capacity;
    }

    args->sections[args->section_count] = section;
    args->section_count += 1;
    return true;
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

/// Drain callbacks and return the first diagnostic, or NULL when all checks passed.
#define gunit_return() \
    do { \
        gunit__drain_defers(); \
        return gunit__failure; \
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
            gunit__report(gunit__failure, gunit_args); \
            fflush(stdout); \
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
    static const char* test_##test_name(gunit_Args* gunit_args) { \
        gunit__Deferred gunit__entries[GUNIT_DEFER_SIZE]; \
        size_t gunit__defer_count = 0; \
        (void)gunit_args; \
        gunit__reset_result(); \
        __VA_ARGS__; \
        gunit_return(); \
    } \
    static gunit_Test gunit_case_##test_name = {test_##test_name, display_name, NULL}; \
    GUNIT_INITIALIZER(register_##test_name) { \
        gunit_register(&gunit_case_##test_name); \
    }

#define GUNIT_SETUP(...) \
    if (gunit_args->kind == GUNIT_RUN_EXECUTE) { \
        __VA_ARGS__; \
    }

#define GUNIT_SECTION(name, ...) \
    if ((gunit_args->kind == GUNIT_RUN_EXECUTE) && (gunit_args->section != NULL) \
        && (strcmp(gunit_args->section, (name)) == 0)) { \
        __VA_ARGS__; \
    } else if (gunit_args->kind == GUNIT_RUN_EXPLORE) { \
        gunit_assert("register section", gunit_Args__append_section(gunit_args, (name))); \
    }

static inline bool gunit__report(const char* result, const gunit_Args* args) {
    if (result == NULL) {
        puts("OK");
        return true;
    }
    puts("FAIL");
    printf("%s%s\n", args->section == NULL ? "    " : "      ", result);
    return false;
}

static inline bool gunit__run_test(gunit_Test* test) {
    printf("  Running '%s'... ", test->name);
    fflush(stdout);

    gunit_Args args = {.kind = GUNIT_RUN_EXPLORE};
    const char* result = test->func(&args);
    bool passed = true;
    if (result != NULL || args.section_count == 0) {
        passed = gunit__report(result, &args);
    } else {
        puts("");
        for (size_t i = 0; i < args.section_count; i++) {
            gunit_Args section = {.kind = GUNIT_RUN_EXECUTE, .section = args.sections[i]};
            printf("    > section '%s'... ", section.section);
            fflush(stdout);
            result = test->func(&section);
            if (!gunit__report(result, &section)) {
                passed = false;
            }
        }
    }
    free(args.sections);
    return passed;
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
            if (gunit__run_test(test)) { \
                passed += 1; \
            } \
        } \
        gunit__reset_result(); \
        printf("\n  Passed: %zu/%zu tests.\n", passed, count); \
        return passed == count ? EXIT_SUCCESS : EXIT_FAILURE; \
    }
