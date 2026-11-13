#include <glint/audio.h>

#include <mewunit.h>
#include <math.h>
#include <stdint.h>

/* raylib device stubs keep this test deterministic without audio hardware. */
static bool gaudio__test_device_ready;
static bool gaudio__test_device_start_succeeds;
static int gaudio__test_device_init_calls;
static int gaudio__test_device_close_calls;
static int gaudio__test_last_buffer_size;

void InitAudioDevice(void) {
    gaudio__test_device_init_calls++;
    gaudio__test_device_ready = gaudio__test_device_start_succeeds;
}

void CloseAudioDevice(void) {
    gaudio__test_device_close_calls++;
    gaudio__test_device_ready = false;
}

bool IsAudioDeviceReady(void) {
    return gaudio__test_device_ready;
}

void SetAudioStreamBufferSizeDefault(int size) {
    gaudio__test_last_buffer_size = size;
}

TEST(device_lifetime, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gaudio_Device owner = {0};
    gaudio_Device borrowed = {0};
    gaudio__test_device_ready = false;
    gaudio__test_device_start_succeeds = true;
    gaudio__test_device_init_calls = 0;
    gaudio__test_device_close_calls = 0;
    gaudio__test_last_buffer_size = 0;
    mewassert("reject impossible raylib buffer size", !gaudio_Device_open(alloc, &owner, UINT32_MAX));
    mewassert("open owner", gaudio_Device_open(alloc, &owner, 0));
    mewassert("default buffer", gaudio__test_last_buffer_size == 2048);
    mewassert("device is ready", gaudio_Device_ready(&owner));
    mewassert("opening the same handle twice fails", !gaudio_Device_open(alloc, &owner, 1024));
    mewassert("second handle borrows", gaudio_Device_open(alloc, &borrowed, 1024));
    mewassert("opening borrowed device does not reinitialize", gaudio__test_device_init_calls == 1 && gaudio__test_last_buffer_size == 2048);
    gaudio_Device_close(&borrowed);
    mewassert("closing borrowed handle keeps device open", gaudio__test_device_ready && gaudio__test_device_close_calls == 0);
    gaudio_Device_close(&owner);
    mewassert("owner closes device", !gaudio__test_device_ready && gaudio__test_device_close_calls == 1);
    gaudio_Device_close(&owner);
    mewassert("close is idempotent", gaudio__test_device_close_calls == 1);
    gaudio__test_device_start_succeeds = false;
    mewassert("failed device initialization is reported", !gaudio_Device_open(alloc, &owner, 256));
    mewassert("failed device is not marked open", !owner.open && !gaudio_Device_ready(&owner));
})

static bool gaudio__test_import_audio_dependency(galloc_Allocator alloc, gmod_Modules *modules, gecs_World *world, void *user) {
    (void)world;
    (void)user;
    gmod_Module audio = gaudio_module();
    return gmod_Modules_import(alloc, modules, &audio);
}

TEST(importable_audio, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gaudio__test_device_ready = false;
    gaudio__test_device_start_succeeds = true;
    gaudio__test_device_init_calls = gaudio__test_device_close_calls = 0;
    gecs_World *world = gecs_World_new(alloc);
    gmod_Modules *modules = gmod_Modules_new(alloc, world);
    gmod_Module audio = gaudio_module();
    gmod_Module dependent = {GMOD_INIT, .name = "test.audio-dependent", .import = gaudio__test_import_audio_dependency};
    mewrequire("dependency imports", gmod_Modules_import(alloc, modules, &dependent));
    mewrequire("duplicate import", gmod_Modules_import(alloc, modules, &audio));
    mewrequire("load", gmod_Modules_load(alloc, modules));
    gecs_Component type = GECS_CONTEXT(alloc, world, gaudio_Service);
    gaudio_Service *service = gecs_context_get(world, type);
    mewassert("device available through world", service && gaudio_Device_ready(&service->device));
    mewassert("initialized exactly once", gaudio__test_device_init_calls == 1);
    gmod_Modules_unload(alloc, modules);
    mewassert("service removed on unload", !gecs_context_get(world, type));
    mewassert("device closed exactly once", gaudio__test_device_close_calls == 1);
    gmod_Modules_free(alloc, modules);
    gecs_World_free(alloc, world);

    world = gecs_World_new(alloc);
    modules = gmod_Modules_new(alloc, world);
    gaudio__test_device_start_succeeds = false;
    mewrequire("import before failing load", gmod_Modules_import(alloc, modules, &audio));
    mewassert("device failure propagates", !gmod_Modules_load(alloc, modules));
    type = GECS_CONTEXT(alloc, world, gaudio_Service);
    mewassert("failure leaves no service", !gecs_context_get(world, type));
    gmod_Modules_free(alloc, modules);
    gecs_World_free(alloc, world);

    world = gecs_World_new(alloc);
    modules = gmod_Modules_new(alloc, world);
    type = GECS_CONTEXT(alloc, world, gaudio_Service);
    gaudio_Service host = {0};
    mewrequire("headless host service", gecs_context_set(alloc, world, type, &host));
    mewrequire("import into host", gmod_Modules_import(alloc, modules, &audio));
    mewrequire("load borrows unavailable host device", gmod_Modules_load(alloc, modules));
    gmod_Modules_free(alloc, modules);
    mewassert("host service remains", gecs_context_get(world, type) != NULL);
    mewassert("headless host did not open or close audio", gaudio__test_device_init_calls == 2 && gaudio__test_device_close_calls == 1);
    gecs_World_free(alloc, world);
})

TEST(filter, {
    gaudio_OnePoleFilter low = {.mode = GAUDIO_FILTER_LOW_PASS};
    gaudio_OnePoleFilter high = {.mode = GAUDIO_FILTER_HIGH_PASS};
    mewassert("set cutoff", gaudio_OnePoleFilter_set_cutoff(&low, 1000.0f, 48000.0f));
    mewassert("set cutoff", gaudio_OnePoleFilter_set_cutoff(&high, 1000.0f, 48000.0f));
    float samples[] = {1.0f, 0.5f, -0.25f, 0.0f};
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        float a = gaudio_OnePoleFilter_process_sample(&low, samples[i]);
        float b = gaudio_OnePoleFilter_process_sample(&high, samples[i]);
        mewassert("low and high pass outputs sum to input", fabsf(a + b - samples[i]) < 0.00001f);
    }
    gaudio_OnePoleFilter_reset(&low);
    mewassert("reset clears state", low.integrator_state == 0.0f);
    float batch[] = {1.0f, 0.0f, 0.0f};
    gaudio_OnePoleFilter sequential = low;
    gaudio_OnePoleFilter_process(&low, batch, 3);
    mewassert("batch processes first sample", batch[0] == gaudio_OnePoleFilter_process_sample(&sequential, 1.0f));
    mewassert("batch processes second sample", batch[1] == gaudio_OnePoleFilter_process_sample(&sequential, 0.0f));
    mewassert("batch processes third sample", batch[2] == gaudio_OnePoleFilter_process_sample(&sequential, 0.0f));
    float gain = low.integrator_gain;
    mewassert("invalid sample rate rejected", !gaudio_OnePoleFilter_set_cutoff(&low, 1000.0f, 0.0f));
    mewassert("nonfinite cutoff rejected", !gaudio_OnePoleFilter_set_cutoff(&low, NAN, 48000.0f));
    mewassert("invalid inputs leave gain untouched", low.integrator_gain == gain);
    mewassert("negative cutoff clamps", gaudio_OnePoleFilter_set_cutoff(&low, -10.0f, 48000.0f));
    mewassert("clamped gain is zero", low.integrator_gain == 0.0f);
    mewassert("large cutoff clamps", gaudio_OnePoleFilter_set_cutoff(&low, 100000.0f, 48000.0f));
    mewassert("clamped gain stays finite", isfinite(low.integrator_gain) && low.integrator_gain > 0.0f);
})

mewtest_main();
