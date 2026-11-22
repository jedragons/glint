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

static const galloc_FunctionTable gdim2_atlas__test_table =
    {gdim2_atlas__test_alloc, gdim2_atlas__test_free, gdim2_atlas__test_calloc, gdim2_atlas__test_realloc};

typedef struct gdim2_atlas__TestContext {
    galloc_Allocator alloc;
    gdim2_atlas_Atlas atlas;
    gdim2_atlas__TestAllocator allocations;
} gdim2_atlas__TestContext;

static bool gdim2_atlas__test_parse(gdim2_atlas__TestContext* ctx, const char* text);
static bool gdim2_atlas__test_rejects(gdim2_atlas__TestContext* ctx, const char* document);
static void gdim2_atlas__test_destroy(void* user);

static const char* gdim2_atlas__test_document =
    "{\"frames\":{\"f0\":{\"x\":16,\"w\":16,\"h\":16,\"duration\":150},"
    "\"f1\":{\"duration\":100}},\"animations\":{\"walk\":[\"f0\",\"f1\"]}}";

GUNIT_TEST(atlas_frame_lookup,
    gdim2_atlas__TestContext ctx = {0};

    GUNIT_SETUP({
        gunit_defer(gdim2_atlas__test_destroy, &ctx);
        gunit_assert("parse the atlas fixture", gdim2_atlas__test_parse(&ctx, gdim2_atlas__test_document));
    })

    GUNIT_SECTION("reads the metadata for a named frame", {
        gunit_assert("two frames parsed", ctx.atlas.frame_count == 2);
        gunit_assert("one animation parsed", ctx.atlas.animation_count == 1);

        const gdim2_atlas_Frame* frame = gdim2_atlas_frame(&ctx.atlas, "f0");
        gunit_assert("first frame found", frame != NULL);
        gunit_assert("frame starts at pixel 16 in the atlas", frame->x == 16);
        gunit_assert("frame is 16 pixels wide", frame->width == 16);
        gunit_assert("150 milliseconds converted to seconds", frame->duration > 0.149f && frame->duration < 0.151f);
    })

    GUNIT_SECTION("returns NULL for a missing frame", {
        gunit_assert("missing frame returns NULL", gdim2_atlas_frame(&ctx.atlas, "missing") == NULL);
    })
)

GUNIT_TEST(atlas_animation_sequence,
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("parse the atlas fixture", gdim2_atlas__test_parse(&ctx, gdim2_atlas__test_document));

    const gdim2_atlas_Animation* animation = gdim2_atlas_animation(&ctx.atlas, "walk");
    gunit_assert("walk animation found", animation != NULL);
    gunit_assert("walk animation contains two frames", animation->count == 2);

    const gdim2_atlas_Frame* first = gdim2_atlas_frame(&ctx.atlas, "f0");
    const gdim2_atlas_Frame* second = gdim2_atlas_frame(&ctx.atlas, "f1");
    gunit_assert("first frame found", first != NULL);
    gunit_assert("second frame found", second != NULL);

    gdim2_atlas_AnimationPlayer player = gdim2_atlas_AnimationPlayer_new(&ctx.atlas, animation);
    gunit_assert("player starts at the first frame", gdim2_atlas_AnimationPlayer_frame(&player) == first);

    gdim2_atlas_AnimationPlayer_update(&player, 0.16f);
    gunit_assert("player advances to the second frame", gdim2_atlas_AnimationPlayer_frame(&player) == second);

    gdim2_atlas_AnimationPlayer_update(&player, 0.11f);
    gunit_assert("player wraps to the first frame", gdim2_atlas_AnimationPlayer_frame(&player) == first);
)

GUNIT_TEST(atlas_empty_document,
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);

    gunit_assert("empty atlas accepted", gdim2_atlas__test_parse(&ctx, "{}"));
    gdim2_atlas_AnimationPlayer player = {.atlas = &ctx.atlas};
    gunit_assert("player without an animation has no frame", gdim2_atlas_AnimationPlayer_frame(&player) == NULL);
)

GUNIT_TEST(atlas_rejects_invalid_documents,
    gdim2_atlas__TestContext ctx = {0};
    GUNIT_SETUP({
        gunit_defer(gdim2_atlas__test_destroy, &ctx);
    })

    GUNIT_SECTION("unknown frame in an animation", {
        gunit_assert(
            "reject invalid atlas",
            gdim2_atlas__test_rejects(&ctx, "{\"animations\":{\"bad\":[\"missing\"]}}")
        );
    })

    GUNIT_SECTION("negative frame coordinate", {
        gunit_assert("reject invalid atlas", gdim2_atlas__test_rejects(&ctx, "{\"frames\":{\"bad\":{\"x\":-1}}}"));
    })

    GUNIT_SECTION("fractional frame coordinate", {
        gunit_assert("reject invalid atlas", gdim2_atlas__test_rejects(&ctx, "{\"frames\":{\"bad\":{\"x\":1.5}}}"));
    })

    GUNIT_SECTION("duplicate frame name", {
        gunit_assert("reject invalid atlas", gdim2_atlas__test_rejects(&ctx, "{\"frames\":{\"x\":{},\"x\":{}}}"));
    })

    GUNIT_SECTION("NUL character in a frame name", {
        gunit_assert("reject invalid atlas", gdim2_atlas__test_rejects(&ctx, "{\"frames\":{\"x\\u0000y\":{}}}"));
    })

    GUNIT_SECTION("array at the root", {
        gunit_assert("reject invalid atlas", gdim2_atlas__test_rejects(&ctx, "[1,2]"));
    })
)

GUNIT_TEST(atlas_destroy_releases_allocations,
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("parse the atlas fixture", gdim2_atlas__test_parse(&ctx, gdim2_atlas__test_document));
    gdim2_atlas_destroy(ctx.alloc, &ctx.atlas);
    gunit_assert("atlas destruction releases memory", ctx.allocations.live == 0);
)

GUNIT_TEST(atlas_allocation_failures_release_partial_state,
    gdim2_atlas__TestContext ctx = {0};
    gunit_defer(gdim2_atlas__test_destroy, &ctx);
    gunit_assert("parse the atlas fixture", gdim2_atlas__test_parse(&ctx, gdim2_atlas__test_document));
    gdim2_atlas_destroy(ctx.alloc, &ctx.atlas);
    const size_t allocation_count = ctx.allocations.calls;

    for (size_t fail_at = 1; fail_at <= allocation_count; fail_at++) {
        ctx.allocations = (gdim2_atlas__TestAllocator) {.fail = fail_at};
        gunit_assert(
            "allocation failure reported",
            !gdim2_atlas__test_parse(&ctx, gdim2_atlas__test_document)
        );
        gunit_assert("failed parse releases partial atlas", ctx.allocations.live == 0);
    }
)

gunit_main();

static bool gdim2_atlas__test_parse(gdim2_atlas__TestContext* ctx, const char* text) {
    ctx->alloc = (galloc_Allocator) {.data = &ctx->allocations, .ftable = &gdim2_atlas__test_table};
    return gdim2_atlas_parse(ctx->alloc, text, strlen(text), &ctx->atlas);
}

static bool gdim2_atlas__test_rejects(gdim2_atlas__TestContext* ctx, const char* document) {
    if (!gunit_check("invalid document rejected", !gdim2_atlas__test_parse(ctx, document))) {
        return false;
    }
    if (!gunit_check("failed parse clears frames", ctx->atlas.frames == NULL)) {
        return false;
    }
    return gunit_check("failed parse clears animations", ctx->atlas.animations == NULL);
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
