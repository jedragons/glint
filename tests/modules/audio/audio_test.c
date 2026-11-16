#include <glint/modules/audio/audio.h>
#include <glint/modules/audio/dsp.h>
#include <glint/modules/audio/module.h>
#include <glint/platform/audio.h>
#include <glint/unit/unit.h>

#include <math.h>
#include <stddef.h>
#include <stdint.h>

// Platform stand-ins isolate device ownership and module policy from audio hardware.
static bool gaudio__test_device_ready;
static bool gaudio__test_device_start_succeeds;
static size_t gaudio__test_device_init_calls;
static size_t gaudio__test_device_close_calls;
static int gaudio__test_last_buffer_size;

typedef struct gaudio__TestContext {
    galloc_Allocator alloc;
    gaudio_Device owner;
    gaudio_Device borrowed;
    gecs_World* world;
    gmod_Modules* modules;
    gaudio_Service host;
    bool ready_at_unload;
} gaudio__TestContext;

static void gaudio__test_create(gaudio__TestContext* ctx);
static bool gaudio__test_create_modules(gaudio__TestContext* ctx);
static void gaudio__test_free(void* user);

static bool gaudio__test_import_audio_dependency(gmod_Modules* modules, gecs_World* world, void* user);
static void gaudio__test_dependent_unload(gecs_World* world, void* user);

bool gplatform_audio_ready(void) {
    return gaudio__test_device_ready;
}

void gplatform_audio_open(int buffer_size) {
    gaudio__test_last_buffer_size = buffer_size;
    gaudio__test_device_init_calls += 1;
    gaudio__test_device_ready = gaudio__test_device_start_succeeds;
}

void gplatform_audio_close(void) {
    gaudio__test_device_close_calls += 1;
    gaudio__test_device_ready = false;
}

GUNIT_TEST(device_uses_default_buffer, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("open owner", gaudio_Device_open(ctx.alloc, &ctx.owner, 0));
    gunit_assert("default buffer", gaudio__test_last_buffer_size == 2048);
    gunit_assert("device is ready", gaudio_Device_ready(&ctx.owner));
    gunit_return();
})

GUNIT_TEST(device_uses_requested_buffer, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("open owner", gaudio_Device_open(ctx.alloc, &ctx.owner, 1024));
    gunit_assert("requested buffer", gaudio__test_last_buffer_size == 1024);
    gunit_return();
})

GUNIT_TEST(device_rejects_oversized_buffer, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("reject impossible buffer size", !gaudio_Device_open(ctx.alloc, &ctx.owner, UINT32_MAX));
    gunit_assert("invalid size never initializes hardware", gaudio__test_device_init_calls == 0);
    gunit_return();
})

GUNIT_TEST(device_rejects_reopening_handle, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("open owner", gaudio_Device_open(ctx.alloc, &ctx.owner, 0));
    gunit_assert("opening the same handle twice fails", !gaudio_Device_open(ctx.alloc, &ctx.owner, 1024));
    gunit_assert("device initialized only once", gaudio__test_device_init_calls == 1);
    gunit_return();
})

GUNIT_TEST(borrowed_device_preserves_owner, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("open owner", gaudio_Device_open(ctx.alloc, &ctx.owner, 0));
    gunit_assert("second handle borrows", gaudio_Device_open(ctx.alloc, &ctx.borrowed, 1024));
    gunit_assert(
        "borrowing preserves initialization and buffer",
        gaudio__test_device_init_calls == 1 && gaudio__test_last_buffer_size == 2048
    );
    gaudio_Device_close(&ctx.borrowed);
    gunit_assert(
        "closing borrower preserves device",
        gaudio__test_device_ready && gaudio__test_device_close_calls == 0
    );
    gunit_return();
})

GUNIT_TEST(owner_close_is_idempotent, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("open owner", gaudio_Device_open(ctx.alloc, &ctx.owner, 0));
    gaudio_Device_close(&ctx.owner);
    gunit_assert("owner closes device", !gaudio__test_device_ready && gaudio__test_device_close_calls == 1);
    gaudio_Device_close(&ctx.owner);
    gunit_assert("close is idempotent", gaudio__test_device_close_calls == 1);
    gunit_return();
})

GUNIT_TEST(device_initialization_failure, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gaudio__test_device_start_succeeds = false;
    gunit_assert("failed initialization is reported", !gaudio_Device_open(ctx.alloc, &ctx.owner, 256));
    gunit_assert("failed device is not marked open", !ctx.owner.open && !gaudio_Device_ready(&ctx.owner));
    gunit_return();
})

GUNIT_TEST(module_import_is_idempotent, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("create module context", gaudio__test_create_modules(&ctx));
    gmod_Module audio = gaudio_module();
    gunit_assert("import audio", gmod_Modules_import(ctx.alloc, ctx.modules, audio));
    gunit_assert("duplicate import", gmod_Modules_import(ctx.alloc, ctx.modules, audio));
    gunit_assert("load", gmod_Modules_load(ctx.alloc, ctx.modules));
    gaudio_Service* service = gecs_context_get(ctx.world, GECS_CONTEXT(ctx.world, gaudio_Service));
    gunit_assert("device available through world", service != NULL && gaudio_Device_ready(&service->device));
    gunit_assert("initialized exactly once", gaudio__test_device_init_calls == 1);
    gunit_return();
})

GUNIT_TEST(module_unloads_dependents_before_audio, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("create module context", gaudio__test_create_modules(&ctx));
    gmod_Module dependent = {
        GMOD_INIT,
        .name = "test.audio-dependent",
        .user = &ctx.ready_at_unload,
        .import = gaudio__test_import_audio_dependency,
        .unload = gaudio__test_dependent_unload
    };
    gunit_assert("import dependent", gmod_Modules_import(ctx.alloc, ctx.modules, dependent));
    gunit_assert("load", gmod_Modules_load(ctx.alloc, ctx.modules));
    gecs_Component type = GECS_CONTEXT(ctx.world, gaudio_Service);
    gunit_assert("unload", gmod_Modules_unload(ctx.alloc, ctx.modules));
    gunit_assert("dependent unloads while device ready", ctx.ready_at_unload);
    gunit_assert("service removed", gecs_context_get(ctx.world, type) == NULL);
    gunit_assert("device closed exactly once", gaudio__test_device_close_calls == 1);
    gunit_return();
})

GUNIT_TEST(module_load_propagates_device_failure, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("create module context", gaudio__test_create_modules(&ctx));
    gaudio__test_device_start_succeeds = false;
    gunit_assert("import audio", gmod_Modules_import(ctx.alloc, ctx.modules, gaudio_module()));
    gunit_assert("device failure propagates", !gmod_Modules_load(ctx.alloc, ctx.modules));
    gunit_assert(
        "failure leaves no service",
        gecs_context_get(ctx.world, GECS_CONTEXT(ctx.world, gaudio_Service)) == NULL
    );
    gunit_return();
})

GUNIT_TEST(module_borrows_host_service, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("create module context", gaudio__test_create_modules(&ctx));
    gecs_Component type = GECS_CONTEXT(ctx.world, gaudio_Service);
    gunit_assert("install host service", gecs_context_set(ctx.world, type, &ctx.host) != NULL);
    gunit_assert("import audio", gmod_Modules_import(ctx.alloc, ctx.modules, gaudio_module()));
    gunit_assert("load borrows unavailable device", gmod_Modules_load(ctx.alloc, ctx.modules));
    gmod_Modules_free(ctx.alloc, ctx.modules);
    ctx.modules = NULL;
    gunit_assert("host service remains", gecs_context_get(ctx.world, type) != NULL);
    gunit_assert("host device untouched", gaudio__test_device_init_calls == 0 && gaudio__test_device_close_calls == 0);
    gunit_return();
})

GUNIT_TEST(headless_policy, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("create module context", gaudio__test_create_modules(&ctx));
    gaudio__test_device_start_succeeds = false;
    gmod_Host policy = {.headless = true};
    gunit_assert(
        "install host policy",
        gecs_context_set(ctx.world, GECS_CONTEXT(ctx.world, gmod_Host), &policy) != NULL
    );
    gunit_assert("import audio", gmod_Modules_import(ctx.alloc, ctx.modules, gaudio_module()));
    gunit_assert("load without available device", gmod_Modules_load(ctx.alloc, ctx.modules));
    gecs_Component type = GECS_CONTEXT(ctx.world, gaudio_Service);
    gaudio_Service* service = gecs_context_get(ctx.world, type);
    gunit_assert(
        "unavailable service is module owned",
        service != NULL && service->module_owned && !service->device.open
    );
    gunit_assert("expected initialization policy", gaudio__test_device_init_calls == 0);
    gunit_assert("unload", gmod_Modules_unload(ctx.alloc, ctx.modules));
    gunit_assert("service removed", gecs_context_get(ctx.world, type) == NULL);
    gunit_assert("unavailable device not closed", gaudio__test_device_close_calls == 0);
    gunit_return();
})

GUNIT_TEST(optional_device_policy, {
    gaudio__TestContext ctx = {0};
    gunit_defer(gaudio__test_free, &ctx);
    gaudio__test_create(&ctx);
    gunit_assert("create module context", gaudio__test_create_modules(&ctx));
    gaudio__test_device_start_succeeds = false;
    gmod_Host policy = {.allow_unavailable_devices = true};
    gunit_assert(
        "install host policy",
        gecs_context_set(ctx.world, GECS_CONTEXT(ctx.world, gmod_Host), &policy) != NULL
    );
    gunit_assert("import audio", gmod_Modules_import(ctx.alloc, ctx.modules, gaudio_module()));
    gunit_assert("load without available device", gmod_Modules_load(ctx.alloc, ctx.modules));
    gecs_Component type = GECS_CONTEXT(ctx.world, gaudio_Service);
    gaudio_Service* service = gecs_context_get(ctx.world, type);
    gunit_assert(
        "unavailable service is module owned",
        service != NULL && service->module_owned && !service->device.open
    );
    gunit_assert("expected initialization policy", gaudio__test_device_init_calls == 1);
    gunit_assert("unload", gmod_Modules_unload(ctx.alloc, ctx.modules));
    gunit_assert("service removed", gecs_context_get(ctx.world, type) == NULL);
    gunit_assert("unavailable device not closed", gaudio__test_device_close_calls == 0);
    gunit_return();
})

GUNIT_TEST(filter_complementary_outputs, {
    gdsp_OnePoleFilter low = {.mode = GDSP_FILTER_LOW_PASS};
    gdsp_OnePoleFilter high = {.mode = GDSP_FILTER_HIGH_PASS};
    gunit_assert("set low cutoff", gdsp_OnePoleFilter_set_cutoff(&low, 1000.0f, 48000.0f));
    gunit_assert("set high cutoff", gdsp_OnePoleFilter_set_cutoff(&high, 1000.0f, 48000.0f));
    float samples[] = {1.0f, 0.5f, -0.25f, 0.0f};
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        float low_sample = gdsp_OnePoleFilter_process_sample(&low, samples[i]);
        float high_sample = gdsp_OnePoleFilter_process_sample(&high, samples[i]);
        gunit_assert("outputs sum to input", fabsf(low_sample + high_sample - samples[i]) < 0.00001f);
    }
    gunit_return();
})

GUNIT_TEST(filter_reset_clears_state, {
    gdsp_OnePoleFilter filter = {0};
    gunit_assert("set cutoff", gdsp_OnePoleFilter_set_cutoff(&filter, 1000.0f, 48000.0f));
    gdsp_OnePoleFilter_process_sample(&filter, 1.0f);
    gdsp_OnePoleFilter_reset(&filter);
    gunit_assert("reset clears state", filter.integrator_state == 0.0f);
    gunit_return();
})

GUNIT_TEST(filter_batch_matches_sequential, {
    gdsp_OnePoleFilter filter = {0};
    gunit_assert("set cutoff", gdsp_OnePoleFilter_set_cutoff(&filter, 1000.0f, 48000.0f));
    gdsp_OnePoleFilter sequential = filter;
    float batch[] = {1.0f, 0.0f, 0.0f};
    gdsp_OnePoleFilter_process(&filter, batch, 3);
    for (size_t i = 0; i < 3; i++) {
        gunit_assert(
            "batch matches sequential",
            batch[i] == gdsp_OnePoleFilter_process_sample(&sequential, i == 0 ? 1.0f : 0.0f)
        );
    }
    gunit_return();
})

GUNIT_TEST(filter_rejects_invalid_parameters, {
    gdsp_OnePoleFilter filter = {0};
    gunit_assert("set valid cutoff", gdsp_OnePoleFilter_set_cutoff(&filter, 1000.0f, 48000.0f));
    float gain = filter.integrator_gain;
    gunit_assert("invalid sample rate rejected", !gdsp_OnePoleFilter_set_cutoff(&filter, 1000.0f, 0.0f));
    gunit_assert("nonfinite cutoff rejected", !gdsp_OnePoleFilter_set_cutoff(&filter, NAN, 48000.0f));
    gunit_assert("invalid inputs preserve gain", filter.integrator_gain == gain);
    gunit_return();
})

GUNIT_TEST(filter_clamps_cutoff, {
    gdsp_OnePoleFilter filter = {0};
    gunit_assert("negative cutoff clamps", gdsp_OnePoleFilter_set_cutoff(&filter, -10.0f, 48000.0f));
    gunit_assert("clamped gain is zero", filter.integrator_gain == 0.0f);
    gunit_assert("large cutoff clamps", gdsp_OnePoleFilter_set_cutoff(&filter, 100000.0f, 48000.0f));
    gunit_assert("clamped gain stays finite", isfinite(filter.integrator_gain) && filter.integrator_gain > 0.0f);
    gunit_return();
})

GUNIT_TEST(low_pass_step_response_settles_to_dc, {
    gdsp_OnePoleFilter filter = {.mode = GDSP_FILTER_LOW_PASS};
    gunit_assert("set cutoff", gdsp_OnePoleFilter_set_cutoff(&filter, 1000, 48000));
    float output = gdsp_OnePoleFilter_process_sample(&filter, 1);
    gunit_assert("first sample is filtered", output > 0 && output < 1);
    float first = output;
    for (size_t i = 0; i < 1000; i++) {
        output = gdsp_OnePoleFilter_process_sample(&filter, 1);
    }
    gunit_assert("low pass responds over time", output > first);
    gunit_assert("low pass preserves DC", fabsf(output - 1) < 0.0001f);
    gunit_return();
})

GUNIT_TEST(high_pass_step_response_rejects_dc, {
    gdsp_OnePoleFilter filter = {.mode = GDSP_FILTER_HIGH_PASS};
    gunit_assert("set cutoff", gdsp_OnePoleFilter_set_cutoff(&filter, 1000, 48000));
    float output = gdsp_OnePoleFilter_process_sample(&filter, 1);
    gunit_assert("step produces a filtered transient", output > 0 && output < 1);
    float first = output;
    for (size_t i = 0; i < 1000; i++) {
        output = gdsp_OnePoleFilter_process_sample(&filter, 1);
    }
    gunit_assert("transient decays", fabsf(output) < first);
    gunit_assert("high pass rejects DC", fabsf(output) < 0.0001f);
    gunit_return();
})

gunit_main();

static bool gaudio__test_import_audio_dependency(gmod_Modules* modules, gecs_World* world, void* user) {
    galloc_Allocator alloc = glint_allocator(world);
    (void)world;
    (void)user;
    gmod_Module audio = gaudio_module();
    return gmod_Modules_import(alloc, modules, audio);
}

static void gaudio__test_dependent_unload(gecs_World* world, void* user) {
    bool* ready_at_unload = user;
    gecs_Component type = GECS_CONTEXT(world, gaudio_Service);
    gaudio_Service* service = type != 0 ? gecs_context_get(world, type) : NULL;
    *ready_at_unload = service != NULL && gaudio_Device_ready(&service->device);
}

static void gaudio__test_create(gaudio__TestContext* ctx) {
    ctx->alloc = galloc_malloc_allocator();
    gaudio__test_device_ready = false;
    gaudio__test_device_start_succeeds = true;
    gaudio__test_device_init_calls = 0;
    gaudio__test_device_close_calls = 0;
    gaudio__test_last_buffer_size = 0;
}

static bool gaudio__test_create_modules(gaudio__TestContext* ctx) {
    ctx->world = gecs_World_new(ctx->alloc);
    if (!gunit_check("create world", ctx->world != NULL)) {
        return false;
    }
    ctx->modules = gmod_Modules_new(ctx->world);
    return gunit_check("create modules", ctx->modules != NULL);
}

static void gaudio__test_free(void* user) {
    gaudio__TestContext* ctx = user;
    if (ctx->modules != NULL) {
        gmod_Modules_free(ctx->alloc, ctx->modules);
    }
    if (ctx->world != NULL) {
        gecs_World_free(ctx->world);
    }
    gaudio_Device_close(&ctx->borrowed);
    gaudio_Device_close(&ctx->owner);
}
