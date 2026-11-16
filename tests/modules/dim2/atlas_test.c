#include <glint/modules/dim2/atlas.h>
#include <glint/unit/unit.h>

#include <stdlib.h>
#include <string.h>

typedef struct gdim2_atlas__TestAllocator {
    size_t calls;
    size_t fail;
    size_t live;
} gdim2_atlas__TestAllocator;

static void* gdim2_atlas__test_alloc(void* data, size_t size);
static void gdim2_atlas__test_free(void* data, void* ptr);
static void* gdim2_atlas__test_calloc(void* data, size_t count, size_t size);
static void* gdim2_atlas__test_realloc(void* data, void* ptr, size_t old_size, size_t size);
static bool gdim2_atlas__test_parse(galloc_Allocator alloc, const char* text, gdim2_atlas_Atlas* atlas);

static const galloc_FunctionTable gdim2_atlas__test_table =
    {gdim2_atlas__test_alloc, gdim2_atlas__test_free, gdim2_atlas__test_calloc, gdim2_atlas__test_realloc};

typedef struct gdim2_atlas__TestContext {
    galloc_Allocator alloc;
    gdim2_atlas_Atlas atlas;
    gdim2_atlas__TestAllocator allocations;
} gdim2_atlas__TestContext;

static bool gdim2_atlas__test_create(gdim2_atlas__TestContext* ctx);
static void gdim2_atlas__test_destroy(void* user);

static const char* gdim2_atlas__test_document =
    "{\"frames\":{\"f0\":{\"x\":16,\"w\":16,\"h\":16,\"duration\":150},"
    "\"f1\":{\"duration\":100}},\"animations\":{\"walk\":[\"f0\",\"f1\"]}}";

GUNIT_TEST(atlas_frame_metadata, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    const gdim2_atlas_Frame* frame = gdim2_atlas_frame(&ctx.atlas, "f0");
    gunit_assert("frame and animation counts", ctx.atlas.frame_count == 2 && ctx.atlas.animation_count == 1);
    gunit_assert(
        "frame coordinates and duration",
        frame != NULL && frame->x == 16 && frame->width == 16 && frame->duration > 0.149f && frame->duration < 0.151f
    );
    gunit_return();
})

GUNIT_TEST(atlas_missing_frame, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gunit_assert("missing frame returns NULL", gdim2_atlas_frame(&ctx.atlas, "missing") == NULL);
    gunit_return();
})

GUNIT_TEST(atlas_animation_sequence, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    const gdim2_atlas_Animation* animation = gdim2_atlas_animation(&ctx.atlas, "walk");
    gunit_assert("animation contains both frames", animation != NULL && animation->count == 2);
    gdim2_atlas_AnimationPlayer player = gdim2_atlas_AnimationPlayer_new(&ctx.atlas, animation);
    gunit_assert(
        "player starts at first frame",
        gdim2_atlas_AnimationPlayer_frame(&player) == gdim2_atlas_frame(&ctx.atlas, "f0")
    );
    gunit_return();
})

GUNIT_TEST(atlas_animation_advances, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gdim2_atlas_AnimationPlayer player =
        gdim2_atlas_AnimationPlayer_new(&ctx.atlas, gdim2_atlas_animation(&ctx.atlas, "walk"));
    gdim2_atlas_AnimationPlayer_update(&player, 0.16f);
    gunit_assert(
        "player advances to next frame",
        gdim2_atlas_AnimationPlayer_frame(&player) == gdim2_atlas_frame(&ctx.atlas, "f1")
    );
    gunit_return();
})

GUNIT_TEST(atlas_animation_wraps, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gdim2_atlas_AnimationPlayer player =
        gdim2_atlas_AnimationPlayer_new(&ctx.atlas, gdim2_atlas_animation(&ctx.atlas, "walk"));
    gdim2_atlas_AnimationPlayer_update(&player, 0.16f);
    gdim2_atlas_AnimationPlayer_update(&player, 0.11f);
    gunit_assert(
        "player wraps to first frame",
        gdim2_atlas_AnimationPlayer_frame(&player) == gdim2_atlas_frame(&ctx.atlas, "f0")
    );
    gunit_return();
})

GUNIT_TEST(atlas_empty_document, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gdim2_atlas_destroy(ctx.alloc, &ctx.atlas);
    gunit_assert("empty atlas accepted", gdim2_atlas__test_parse(ctx.alloc, "{}", &ctx.atlas));
    gunit_assert(
        "incomplete player has no frame",
        gdim2_atlas_AnimationPlayer_frame(&(gdim2_atlas_AnimationPlayer) {.atlas = &ctx.atlas}) == NULL
    );
    gunit_return();
})

GUNIT_TEST(atlas_rejects_unknown_frame, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gdim2_atlas_destroy(ctx.alloc, &ctx.atlas);
    gunit_assert(
        "unknown frame reference rejected",
        !gdim2_atlas__test_parse(ctx.alloc, "{\"animations\":{\"bad\":[\"missing\"]}}", &ctx.atlas)
    );
    gunit_assert("failed parse clears output", ctx.atlas.frames == NULL && ctx.atlas.animations == NULL);
    gunit_return();
})

GUNIT_TEST(atlas_rejects_negative_coordinate, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gdim2_atlas_destroy(ctx.alloc, &ctx.atlas);
    gunit_assert(
        "negative coordinate rejected",
        !gdim2_atlas__test_parse(ctx.alloc, "{\"frames\":{\"bad\":{\"x\":-1}}}", &ctx.atlas)
    );
    gunit_assert("failed parse clears output", ctx.atlas.frames == NULL && ctx.atlas.animations == NULL);
    gunit_return();
})

GUNIT_TEST(atlas_rejects_fractional_coordinate, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gdim2_atlas_destroy(ctx.alloc, &ctx.atlas);
    gunit_assert(
        "fractional coordinate rejected",
        !gdim2_atlas__test_parse(ctx.alloc, "{\"frames\":{\"bad\":{\"x\":1.5}}}", &ctx.atlas)
    );
    gunit_assert("failed parse clears output", ctx.atlas.frames == NULL && ctx.atlas.animations == NULL);
    gunit_return();
})

GUNIT_TEST(atlas_rejects_duplicate_frame, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gdim2_atlas_destroy(ctx.alloc, &ctx.atlas);
    gunit_assert(
        "duplicate frame rejected",
        !gdim2_atlas__test_parse(ctx.alloc, "{\"frames\":{\"x\":{},\"x\":{}}}", &ctx.atlas)
    );
    gunit_assert("failed parse clears output", ctx.atlas.frames == NULL && ctx.atlas.animations == NULL);
    gunit_return();
})

GUNIT_TEST(atlas_rejects_embedded_nul, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gdim2_atlas_destroy(ctx.alloc, &ctx.atlas);
    gunit_assert(
        "embedded NUL name rejected",
        !gdim2_atlas__test_parse(ctx.alloc, "{\"frames\":{\"x\\u0000y\":{}}}", &ctx.atlas)
    );
    gunit_assert("failed parse clears output", ctx.atlas.frames == NULL && ctx.atlas.animations == NULL);
    gunit_return();
})

GUNIT_TEST(atlas_rejects_nonobject, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gdim2_atlas_destroy(ctx.alloc, &ctx.atlas);
    gunit_assert("nonobject atlas rejected", !gdim2_atlas__test_parse(ctx.alloc, "[1,2]", &ctx.atlas));
    gunit_assert("failed parse clears output", ctx.atlas.frames == NULL && ctx.atlas.animations == NULL);
    gunit_return();
})

GUNIT_TEST(atlas_destroy_releases_allocations, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gdim2_atlas_destroy(ctx.alloc, &ctx.atlas);
    gunit_assert("atlas destruction releases memory", ctx.allocations.live == 0);
    gunit_return();
})

GUNIT_TEST(atlas_allocation_failures_release_partial_state, {
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("create", gdim2_atlas__test_create(&ctx));
    gdim2_atlas_destroy(ctx.alloc, &ctx.atlas);
    size_t calls = ctx.allocations.calls;
    for (size_t i = 1; i <= calls; i++) {
        ctx.allocations = (gdim2_atlas__TestAllocator) {.fail = i};
        gunit_assert(
            "allocation failure reported",
            !gdim2_atlas__test_parse(ctx.alloc, gdim2_atlas__test_document, &ctx.atlas)
        );
        gunit_assert("failed parse releases partial atlas", ctx.allocations.live == 0);
    }
    gunit_return();
})

gunit_main();

static bool gdim2_atlas__test_create(gdim2_atlas__TestContext* ctx) {
    ctx->alloc = (galloc_Allocator) {.data = &ctx->allocations, .ftable = &gdim2_atlas__test_table};
    if (!gunit_check(
            "parse atlas fixture",
            gdim2_atlas__test_parse(ctx->alloc, gdim2_atlas__test_document, &ctx->atlas)
        )) {
        return false;
    }

    return true;
}

static void gdim2_atlas__test_destroy(void* user) {
    gdim2_atlas__TestContext* ctx = user;
    gdim2_atlas_destroy(ctx->alloc, &ctx->atlas);
}

static void* gdim2_atlas__test_alloc(void* data, size_t size) {
    gdim2_atlas__TestAllocator* test = data;
    test->calls += 1;
    if (test->calls == test->fail) {
        return NULL;
    }

    void* ptr = malloc(size);
    if (ptr != NULL) {
        test->live += 1;
    }

    return ptr;
}

static void gdim2_atlas__test_free(void* data, void* ptr) {
    gdim2_atlas__TestAllocator* test = data;
    if (ptr != NULL) {
        if (!gunit_check("free has a live allocation", test->live != 0)) {
            return;
        }
        test->live -= 1;
        free(ptr);
    }
}

static void* gdim2_atlas__test_calloc(void* data, size_t count, size_t size) {
    gdim2_atlas__TestAllocator* test = data;
    test->calls += 1;
    if (test->calls == test->fail) {
        return NULL;
    }

    void* ptr = calloc(count, size);
    if (ptr != NULL) {
        test->live += 1;
    }

    return ptr;
}

static void* gdim2_atlas__test_realloc(void* data, void* ptr, size_t old_size, size_t size) {
    gdim2_atlas__TestAllocator* test = data;
    (void)old_size;
    test->calls += 1;
    if (test->calls == test->fail) {
        return NULL;
    }

    bool fresh = ptr == NULL;
    void* result = realloc(ptr, size);
    if (result != NULL && fresh) {
        test->live += 1;
    }

    return result;
}

static bool gdim2_atlas__test_parse(galloc_Allocator alloc, const char* text, gdim2_atlas_Atlas* atlas) {
    return gdim2_atlas_parse(alloc, text, strlen(text), atlas);
}
