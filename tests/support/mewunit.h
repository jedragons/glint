#ifndef GLINT_TESTS_MEWUNIT_H_
#define GLINT_TESTS_MEWUNIT_H_

/* Mewunit-style test registration, kept header-only so each test executable
 * is independent. Checks are active in release builds and may also be used
 * from helper functions. */
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define MEW_STRINGIZE_DETAIL(x) #x
#define MEW_STRINGIZE(x) MEW_STRINGIZE_DETAIL(x)

#if defined(_MSC_VER) && !defined(__clang__)
    #pragma section(".CRT$XCU", read)
    #define MEW_INITIALIZER_DETAIL(f, prefix)                                                                         \
        static void f(void);                                                                                           \
        __declspec(allocate(".CRT$XCU")) void (*f##_)(void) = f;                                                       \
        __pragma(comment(linker, "/include:" prefix #f "_")) static void f(void)
    #ifdef _WIN64
        #define MEW_INITIALIZER(f) MEW_INITIALIZER_DETAIL(f, "")
    #else
        #define MEW_INITIALIZER(f) MEW_INITIALIZER_DETAIL(f, "_")
    #endif
#else
    #define MEW_INITIALIZER(f)                                                                                         \
        static void f(void) __attribute__((constructor));                                                              \
        static void f(void)
#endif

typedef void mew_test_func_t(void);
typedef struct MewTest {
    mew_test_func_t *func;
    const char *name;
} MewTest;

static jmp_buf mew_test_return;
static int mew_test_active;
static int mew_test_argc;
static char **mew_test_argv;

static void mew_test_fail(const char *file, int line, const char *condition, const char *message) {
    fprintf(stderr, "\n    %s:%d: check `%s` failed: %s\n", file, line, condition, message);
    if (mew_test_active) {
        longjmp(mew_test_return, 1);
    }
    exit(EXIT_FAILURE);
}

/* Abort rather than longjmp across library callbacks, which may hold internal
 * locks or invariants. Reserve this for checks made from such callbacks. */
static inline void mew_test_abort(const char *file, int line, const char *condition, const char *message) {
    fprintf(stderr, "\n    %s:%d: check `%s` failed: %s\n", file, line, condition, message);
    exit(EXIT_FAILURE);
}

#define mewrequire(message, ...)                                                                                       \
    do {                                                                                                               \
        if (!(__VA_ARGS__)) {                                                                                          \
            mew_test_abort(__FILE__, __LINE__, #__VA_ARGS__, (message));                                              \
        }                                                                                                              \
    } while (0)

/* The variadic expression supports compound literals containing commas. */
#define mewassert(message, ...)                                                                                        \
    do {                                                                                                               \
        if (!(__VA_ARGS__)) {                                                                                          \
            mew_test_fail(__FILE__, __LINE__, #__VA_ARGS__, (message));                                               \
        }                                                                                                              \
    } while (0)

#define TEST(test_name, ...)                                                                                           \
    extern MewTest mew_tests[];                                                                                        \
    static void test_##test_name(void) __VA_ARGS__                                                                     \
    MEW_INITIALIZER(register_##test_name) {                                                                            \
        mew_tests[__COUNTER__] = (MewTest) {.func = test_##test_name, .name = #test_name};                            \
    }

#define mewtest_main(...)                                                                                              \
    MewTest mew_tests[__COUNTER__];                                                                                    \
    int main(int argc, char **argv) {                                                                                 \
        mew_test_argc = argc;                                                                                          \
        mew_test_argv = argv;                                                                                          \
        __VA_ARGS__;                                                                                                   \
        size_t count = sizeof(mew_tests) / sizeof(*mew_tests);                                                         \
        size_t passed = 0;                                                                                             \
        for (size_t i = 0; i < count; i++) {                                                                           \
            printf("    Running '%s'... ", mew_tests[i].name);                                                        \
            fflush(stdout);                                                                                            \
            mew_test_active = 1;                                                                                       \
            if (setjmp(mew_test_return) == 0) {                                                                        \
                mew_tests[i].func();                                                                                   \
                puts("OK");                                                                                            \
                passed++;                                                                                              \
            } else {                                                                                                   \
                puts("FAIL");                                                                                          \
            }                                                                                                          \
            mew_test_active = 0;                                                                                        \
        }                                                                                                              \
        printf("\n    Passed: %zu/%zu tests.\n", passed, count);                                                     \
        return passed == count ? EXIT_SUCCESS : EXIT_FAILURE;                                                           \
    }

#endif // GLINT_TESTS_MEWUNIT_H_
