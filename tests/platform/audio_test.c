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

GUNIT_TEST(audio_device,
    gplatform_audio__Context ctx = {0};

    GUNIT_SETUP({
        gplatform_audio__current = &ctx;
        gunit_defer(gplatform_audio__free, &ctx);
    })

    GUNIT_SECTION("successful initialization applies the requested buffer size", {
        ctx.initialization_succeeds = true;
        gplatform_audio_open(512);

        gunit_assert("requested buffer forwarded", ctx.buffer_size == 512);
        gunit_assert("backend initializes exactly once", ctx.opens == 1);
        gunit_assert("backend reports ready", gplatform_audio_ready());
    })

    GUNIT_SECTION("failed initialization leaves the device unavailable", {
        gplatform_audio_open(512);

        gunit_assert("backend attempts initialization once", ctx.opens == 1);
        gunit_assert("backend remains unavailable", !gplatform_audio_ready());
    })

    GUNIT_SECTION("closing the device releases the backend", {
        ctx.initialization_succeeds = true;
        gplatform_audio_open(512);
        gunit_assert("backend reports ready before closing", gplatform_audio_ready());

        gplatform_audio_close();
        gunit_assert("backend closes exactly once", ctx.closes == 1);
        gunit_assert("closed backend reports unavailable", !gplatform_audio_ready());
    })
)

gunit_main();

static void gplatform_audio__free(void* user) {
    gplatform_audio__Context* ctx = user;
    if (ctx->ready) {
        gplatform_audio_close();
    }
    gplatform_audio__current = NULL;
}
