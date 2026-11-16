// Link this driver against either actual example entry point, compiled with
// -Dmain=example_entry. No real window, browser or graphics backend needed.
#include <glint/config.h>
#include <glint/glint.h>
#include <glint/unit/unit.h>

#include <stddef.h>
#include <string.h>

int example_entry(int argc, char** argv);

struct glint_Game {
    bool alive;
};

typedef struct glint__EntryContext {
    glint_Game game;
    int allocator_state;
    size_t destroys;
    size_t frames;
    bool fail_create;
    bool fail_frame;
#if GLINT_PLATFORM_EMSCRIPTEN
    glint_RunEndFn end;
    galloc_Allocator alloc;
    void* end_user;
    size_t cancellations;
    bool should_close;
#endif
} glint__EntryContext;

static glint__EntryContext* glint__test_context;

static void glint__test_context_create(glint__EntryContext* context);
static void glint__test_context_free(void* user);
#if GLINT_PLATFORM_EMSCRIPTEN
static bool glint__test_tick(void);
#endif

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

bool glint_frame(galloc_Allocator alloc, glint_Game* value, float dt) {
    if (!gunit_check("frame receives allocator", alloc.data == &glint__test_context->allocator_state)) {
        return false;
    }
    if (!gunit_check(
            "frame receives live game and delta",
            value == &glint__test_context->game && value->alive && dt > 0
        )) {
        return false;
    }
    glint__test_context->frames += 1;
    return !glint__test_context->fail_frame;
}

const char* glint_error(const glint_Game* value) {
    if (!gunit_check("error receives live game", value == &glint__test_context->game && value->alive)) {
        return "invalid game";
    }
    return "test frame error";
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
    bool ok = glint_frame(alloc, game, 1.0f);
    end(alloc, game, ok, user);
    return ok;
#endif
}

GUNIT_TEST(entry_creation_failure, {
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    context.fail_create = true;
    gunit_assert("creation failure returns error", example_entry(0, NULL) == 1);
    gunit_assert("failed creation does not destroy game", context.destroys == 0);
    gunit_return();
})

#if GLINT_PLATFORM_EMSCRIPTEN
GUNIT_TEST(web_entry_schedules_without_running_frames, {
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    gunit_assert("web entry schedules successfully", example_entry(0, NULL) == 0);
    gunit_assert("game remains alive and scheduled", context.end != NULL && context.game.alive);
    gunit_assert("entry does not run or destroy game", context.destroys == 0 && context.frames == 0);
    gunit_return();
})

GUNIT_TEST(web_entry_renders_scheduled_frames, {
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    gunit_assert("web entry schedules", example_entry(0, NULL) == 0);
    gunit_assert("tick", glint__test_tick());
    gunit_assert("tick", glint__test_tick());
    gunit_assert(
        "scheduled frames keep game alive",
        context.frames == 2 && context.game.alive && context.destroys == 0
    );
    gunit_return();
})

GUNIT_TEST(web_entry_window_close_destroys_game, {
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    gunit_assert("web entry schedules", example_entry(0, NULL) == 0);
    gunit_assert("tick", glint__test_tick());
    gunit_assert("tick", glint__test_tick());
    context.should_close = true;
    gunit_assert("tick", glint__test_tick());
    gunit_assert("window close cancels scheduling", context.end == NULL && context.cancellations == 1);
    gunit_assert(
        "window close destroys game without another frame",
        !context.game.alive && context.destroys == 1 && context.frames == 2
    );
    gunit_return();
})

GUNIT_TEST(web_entry_frame_failure_destroys_game, {
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    context.fail_frame = true;
    gunit_assert(
        "web entry schedules before frame fails",
        example_entry(0, NULL) == 0 && context.end != NULL && context.game.alive
    );
    gunit_assert("tick", glint__test_tick());
    gunit_assert("failed frame cancels scheduling", context.end == NULL && context.cancellations == 1);
    gunit_assert("failed frame destroys game", !context.game.alive && context.destroys == 1 && context.frames == 1);
    gunit_return();
})
#else
GUNIT_TEST(native_entry_completes_and_destroys_game, {
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    gunit_assert("native entry succeeds", example_entry(0, NULL) == 0);
    gunit_assert(
        "native entry renders and destroys game",
        !context.game.alive && context.destroys == 1 && context.frames == 1
    );
    gunit_return();
})

GUNIT_TEST(native_entry_frame_failure_destroys_game, {
    glint__EntryContext context = {0};
    gunit_defer(glint__test_context_free, &context);
    glint__test_context_create(&context);
    context.fail_frame = true;
    gunit_assert("failed native frame returns error", example_entry(0, NULL) == 1);
    gunit_assert(
        "failed native frame destroys game",
        !context.game.alive && context.destroys == 1 && context.frames == 1
    );
    gunit_return();
})
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

#if GLINT_PLATFORM_EMSCRIPTEN
static bool glint__test_tick(void) {
    glint__EntryContext* context = glint__test_context;
    if (!gunit_check("loop is scheduled", context->end != NULL)) {
        return false;
    }
    bool ok = context->should_close || glint_frame(context->alloc, &context->game, 1.0f);
    if (context->should_close || !ok) {
        glint_RunEndFn end = context->end;
        context->end = NULL;
        context->cancellations += 1;
        end(context->alloc, &context->game, ok, context->end_user);
    }

    return true;
}
#endif
