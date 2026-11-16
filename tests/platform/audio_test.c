#include <glint/platform/audio.h>
#include <glint/unit/unit.h>

#include <raylib.h>

typedef struct gplatform_audio__Context {
    bool ready;
    bool initialization_succeeds;
    int buffer_size;
    size_t opens;
    size_t closes;
} gplatform_audio__Context;

static gplatform_audio__Context* gplatform_audio__current;

static void gplatform_audio__free(void* user);

bool IsAudioDeviceReady(void) {
    return gplatform_audio__current->ready;
}

void InitAudioDevice(void) {
    gplatform_audio__current->opens += 1;
    gplatform_audio__current->ready = gplatform_audio__current->initialization_succeeds;
}

void CloseAudioDevice(void) {
    gplatform_audio__current->closes += 1;
    gplatform_audio__current->ready = false;
}

void SetAudioStreamBufferSizeDefault(int size) {
    gplatform_audio__current->buffer_size = size;
}

void SetTraceLogCallback(TraceLogCallback callback) {
    (void)callback;
}

void SetTraceLogLevel(int level) {
    (void)level;
}

GUNIT_TEST(open_initializes_backend_with_requested_buffer, {
    gplatform_audio__Context ctx = {.initialization_succeeds = true};
    gplatform_audio__current = &ctx;
    gunit_defer(gplatform_audio__free, &ctx);
    gplatform_audio_open(512);
    gunit_assert("requested buffer forwarded", ctx.buffer_size == 512);
    gunit_assert("backend initialized", ctx.opens == 1 && gplatform_audio_ready());
    gunit_return();
})

GUNIT_TEST(readiness_reports_failed_backend_initialization, {
    gplatform_audio__Context ctx = {0};
    gplatform_audio__current = &ctx;
    gunit_defer(gplatform_audio__free, &ctx);
    gplatform_audio_open(512);
    gunit_assert("failed backend remains unavailable", ctx.opens == 1 && !gplatform_audio_ready());
    gunit_return();
})

GUNIT_TEST(close_releases_backend_device, {
    gplatform_audio__Context ctx = {.initialization_succeeds = true};
    gplatform_audio__current = &ctx;
    gunit_defer(gplatform_audio__free, &ctx);
    gplatform_audio_open(512);
    gplatform_audio_close();
    gunit_assert("backend closed", ctx.closes == 1 && !gplatform_audio_ready());
    gunit_return();
})

gunit_main();

static void gplatform_audio__free(void* user) {
    gplatform_audio__Context* ctx = user;
    if (ctx->ready) {
        gplatform_audio_close();
    }
    gplatform_audio__current = NULL;
}
