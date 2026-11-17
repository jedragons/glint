// Run an actual example entry point through a stub game lifecycle.
#include <glint/config.h>
#include <glint/glint.h>
#include <glint/unit/unit.h>

#include "./game.h"

#include <stddef.h>
#include <string.h>

int example_entry(int argc, char** argv);

typedef struct glint__EntryContext {
    glint_Game game;
    int allocator_state;
    size_t destroys;
    bool fail_create;
    bool fail_run;
#if GLINT_PLATFORM_EMSCRIPTEN
    glint_RunEndFn end;
    galloc_Allocator alloc;
    void* end_user;
#endif
} glint__EntryContext;

static glint__EntryContext* glint__test_context;

static void glint__test_context_create(glint__EntryContext* context);
static void glint__test_context_free(void* user);

galloc_Allocator galloc_malloc_allocator(void) {
    return (galloc_Allocator) {.data = &glint__test_context->allocator_state};
}

glint_Game* glint_new(galloc_Allocator alloc, const glint_Config* config) {
    if (!gunit_check("entry uses supplied allocator", alloc.data == &glint__test_context->allocator_state)) {
        return NULL;
    }
    if (!gunit_check(
            "example asset root forwarded",
            config != NULL && config->asset_root != NULL && strcmp(config->asset_root, "test-assets") == 0
        )) {
        return NULL;
    }
    if (!gunit_check(
            "example initial script forwarded",
            config != NULL && config->initial_script != NULL && strcmp(config->initial_script, "/game.lua") == 0
        )) {
        return NULL;
    }
    if (glint__test_context->fail_create) {
        return NULL;
    }

    glint__test_context->game.alive = true;
    return &glint__test_context->game;
}

bool glint_destroy(galloc_Allocator alloc, glint_Game* value) {
    if (!gunit_check("destroy receives allocator", alloc.data == &glint__test_context->allocator_state)) {
        return false;
    }
    if (!gunit_check("destroy receives live game", value == &glint__test_context->game && value->alive)) {
        return false;
    }
    value->alive = false;
    glint__test_context->destroys += 1;
    return true;
}

const char* glint_error(const glint_Game* value) {
    if (!gunit_check("error receives live game", value == &glint__test_context->game && value->alive)) {
        return "invalid game";
    }
    return "test run error";
}

bool glint_run_with_completion(galloc_Allocator alloc, glint_Game* game, glint_RunEndFn end, void* user) {
#if GLINT_PLATFORM_EMSCRIPTEN
    if (!gunit_check(
            "schedule receives unscheduled game",
            game == &glint__test_context->game && glint__test_context->end == NULL
        )) {
        return false;
    }
    glint__test_context->end = end;
    glint__test_context->alloc = alloc;
    glint__test_context->end_user = user;
    return true;
#else
    bool ok = !glint__test_context->fail_run;
    end(alloc, game, ok, user);
    return ok;
#endif
}

#ifndef GLINT_TEST_EXAMPLE_SMOKE
GUNIT_TEST(entry_creation_failure,
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    context.fail_create = true;
    gunit_assert("creation failure returns error", example_entry(0, NULL) == 1);
    gunit_assert("failed creation does not destroy game", context.destroys == 0);
)
#endif

#if GLINT_PLATFORM_EMSCRIPTEN
GUNIT_TEST(web_entry_schedules_without_running_frames,
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    gunit_assert("web entry schedules successfully", example_entry(0, NULL) == 0);
    gunit_assert("game remains alive and scheduled", context.end != NULL && context.game.alive);
    gunit_assert("entry defers game destruction", context.destroys == 0);
)

GUNIT_TEST(web_entry_success_completion_destroys_game,
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    gunit_assert("web entry schedules", example_entry(0, NULL) == 0);
    gunit_assert("completion registered", context.end != NULL);
    context.end(context.alloc, &context.game, true, context.end_user);
    gunit_assert("successful completion destroys game", !context.game.alive && context.destroys == 1);
)

GUNIT_TEST(web_entry_failure_completion_destroys_game,
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    gunit_assert("web entry schedules", example_entry(0, NULL) == 0);
    gunit_assert("completion registered", context.end != NULL);
    context.end(context.alloc, &context.game, false, context.end_user);
    gunit_assert("failed completion destroys game", !context.game.alive && context.destroys == 1);
)
#else
GUNIT_TEST(native_entry_completes_and_destroys_game,
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    gunit_assert("native entry succeeds", example_entry(0, NULL) == 0);
    gunit_assert(
        "native entry completes and destroys game",
        !context.game.alive && context.destroys == 1
    );
)

#ifndef GLINT_TEST_EXAMPLE_SMOKE
GUNIT_TEST(native_entry_run_failure_destroys_game,
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    context.fail_run = true;
    gunit_assert("failed native run returns error", example_entry(0, NULL) == 1);
    gunit_assert(
        "failed native run destroys game",
        !context.game.alive && context.destroys == 1
    );
)
#endif
#endif

gunit_main();

static void glint__test_context_create(glint__EntryContext* context) {
    glint__test_context = context;
}

static void glint__test_context_free(void* user) {
    glint__EntryContext* context = user;
    glint__test_context = context;
#if GLINT_PLATFORM_EMSCRIPTEN
    context->end = NULL;
    context->end_user = NULL;
#endif
    if (context->game.alive) {
        glint_destroy(galloc_malloc_allocator(), &context->game);
    }
    glint__test_context = NULL;
}
