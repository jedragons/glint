#include <glint/dim2/atlas.h>

#include <mewunit.h>
#include <string.h>
#include <stdlib.h>

typedef struct gatlas__TestAllocator {
    size_t calls;
    size_t fail;
    size_t live;
} gatlas__TestAllocator;

static void *gatlas__test_test_alloc(void *data, size_t size) {
    gatlas__TestAllocator *test = data;
    if (++test->calls == test->fail) return NULL;
    void *ptr = malloc(size);
    test->live += ptr != NULL;
    return ptr;
}

static void gatlas__test_test_free(void *data, void *ptr) {
    gatlas__TestAllocator *test = data;
    if (ptr) {
        mewrequire("Atlas expectation failed", test->live);
        test->live--;
        free(ptr);
    }
}

static void *gatlas__test_test_calloc(void *data, size_t count, size_t size) {
    gatlas__TestAllocator *test = data;
    if (++test->calls == test->fail) return NULL;
    void *ptr = calloc(count, size);
    test->live += ptr != NULL;
    return ptr;
}

static void *gatlas__test_test_realloc(void *data, void *ptr, size_t old_size, size_t size) {
    gatlas__TestAllocator *test = data;
    (void)old_size;
    if (++test->calls == test->fail) return NULL;
    bool fresh = ptr == NULL;
    void *result = realloc(ptr, size);
    test->live += result != NULL && fresh;
    return result;
}

static const galloc_FunctionTable gatlas__test_table = {gatlas__test_test_alloc, gatlas__test_test_free, gatlas__test_test_calloc, gatlas__test_test_realloc};

static bool gatlas__test_parse(galloc_Allocator alloc, const char *text, gatlas_Atlas *atlas) {
    return gatlas_parse(alloc, text, strlen(text), atlas);
}

TEST(atlas, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    const char *text = "{\"frames\":{\"f0\":{\"x\":16,\"w\":16,\"h\":16,\"duration\":150},"
                       "\"f1\":{\"duration\":100}},\"animations\":{\"walk\":[\"f0\",\"f1\"]}}";
    gatlas_Atlas atlas;
    mewassert("Atlas expectation failed", gatlas__test_parse(alloc, text, &atlas));
    mewassert("Atlas expectation failed", atlas.frame_count == 2 && atlas.animation_count == 1);
    const gatlas_Frame *frame = gatlas_frame(&atlas, "f0");
    mewassert("Atlas expectation failed", frame && frame->x == 16 && frame->width == 16 && frame->duration > 0.149f && frame->duration < 0.151f);
    mewassert("Atlas expectation failed", !gatlas_frame(&atlas, "missing"));
    const gatlas_Animation *animation = gatlas_animation(&atlas, "walk");
    mewassert("Atlas expectation failed", animation && animation->count == 2);
    gatlas_AnimationPlayer player = gatlas_AnimationPlayer_new(&atlas, animation);
    mewassert("Atlas expectation failed", gatlas_AnimationPlayer_frame(&player) == frame);
    gatlas_AnimationPlayer_update(&player, 0.16f);
    mewassert("Atlas expectation failed", gatlas_AnimationPlayer_frame(&player) == gatlas_frame(&atlas, "f1"));
    gatlas_AnimationPlayer_update(&player, 0.11f);
    mewassert("Atlas expectation failed", gatlas_AnimationPlayer_frame(&player) == frame);
    gatlas_destroy(alloc, &atlas);
    mewassert("Atlas expectation failed", gatlas__test_parse(alloc, "{}", &atlas));
    mewassert("Atlas expectation failed", !gatlas_AnimationPlayer_frame(&(gatlas_AnimationPlayer) {.atlas = &atlas}));
    gatlas_destroy(alloc, &atlas);
    mewassert("Atlas expectation failed", !gatlas__test_parse(alloc, "{\"animations\":{\"bad\":[\"missing\"]}}", &atlas));
    mewassert("Atlas expectation failed", !atlas.frames && !atlas.animations);
    mewassert("Atlas expectation failed", !gatlas__test_parse(alloc, "{\"frames\":{\"bad\":{\"x\":-1}}}", &atlas));
    mewassert("Atlas expectation failed", !gatlas__test_parse(alloc, "{\"frames\":{\"bad\":{\"x\":1.5}}}", &atlas));
    mewassert("Atlas expectation failed", !gatlas__test_parse(alloc, "{\"frames\":{\"x\":{},\"x\":{}}}", &atlas));
    mewassert("Atlas expectation failed", !gatlas__test_parse(alloc, "{\"frames\":{\"x\\u0000y\":{}}}", &atlas));
    mewassert("Atlas expectation failed", !gatlas__test_parse(alloc, "[1,2]", &atlas));
    gatlas__TestAllocator state = {0};
    galloc_Allocator tracked = {.data = &state, .ftable = &gatlas__test_table};
    mewassert("Atlas expectation failed", gatlas__test_parse(tracked, text, &atlas));
    gatlas_destroy(tracked, &atlas);
    mewassert("Atlas expectation failed", !state.live);
    size_t calls = state.calls;
    for (size_t i = 1; i <= calls; i++) {
        state = (gatlas__TestAllocator) {.fail = i};
        mewassert("Atlas expectation failed", !gatlas__test_parse(tracked, text, &atlas));
        mewassert("Atlas expectation failed", !state.live);
    }
})

mewtest_main();
