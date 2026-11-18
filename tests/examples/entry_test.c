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
    int32_t allocator_state;
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
    const bool ok = !glint__test_context->fail_run;
    end(alloc, game, ok, user);
    return ok;
#endif
}

#ifndef GLINT_TEST_EXAMPLE_SMOKE
GUNIT_TEST(entry_creation_failure,
    glint__EntryContext context = {.fail_create = true};
    glint__test_context_create(&context);
    gunit_defer(glint__test_context_free, &context);

    gunit_assert("creation failure returns an error status", example_entry(0, NULL) == 1);
    gunit_assert("failed creation leaves no game to destroy", context.destroys == 0);
)
#endif

#if GLINT_PLATFORM_EMSCRIPTEN
GUNIT_TEST(web_entry,
    glint__EntryContext context = {0};

    GUNIT_SETUP({
        glint__test_context_create(&context);
        gunit_defer(glint__test_context_free, &context);
        gunit_assert("web entry schedules successfully", example_entry(0, NULL) == 0);
        gunit_assert("completion callback is registered", context.end != NULL);
    })

    GUNIT_SECTION("the entry point returns before completion", {
        gunit_assert("scheduled game remains alive", context.game.alive);
        gunit_assert("entry defers game destruction", context.destroys == 0);
    })

    GUNIT_SECTION("successful completion destroys the game", {
        context.end(context.alloc, &context.game, true, context.end_user);
        gunit_assert("game is destroyed once", !context.game.alive && context.destroys == 1);
    })

    GUNIT_SECTION("failed completion also destroys the game", {
        context.end(context.alloc, &context.game, false, context.end_user);
        gunit_assert("game is destroyed once", !context.game.alive && context.destroys == 1);
    })
)
#elif defined(GLINT_TEST_EXAMPLE_SMOKE)
GUNIT_TEST(native_entry_completes_and_destroys_game,
    glint__EntryContext context = {0};
    glint__test_context_create(&context);
    gunit_defer(glint__test_context_free, &context);

    gunit_assert("native entry returns success", example_entry(0, NULL) == 0);
    gunit_assert("game is destroyed once", !context.game.alive && context.destroys == 1);
)
#else
GUNIT_TEST(native_entry,
    glint__EntryContext context = {0};

    GUNIT_SETUP({
        glint__test_context_create(&context);
        gunit_defer(glint__test_context_free, &context);
    })

    GUNIT_SECTION("a successful run destroys the game and returns success", {
        gunit_assert("native entry returns success", example_entry(0, NULL) == 0);
        gunit_assert("game is destroyed once", !context.game.alive && context.destroys == 1);
    })

    GUNIT_SECTION("a failed run destroys the game and returns an error status", {
        context.fail_run = true;
        gunit_assert("native entry returns an error status", example_entry(0, NULL) == 1);
        gunit_assert("game is destroyed once", !context.game.alive && context.destroys == 1);
    })
)
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
