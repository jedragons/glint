#include <glint/modules.h>

#include <mewunit.h>
#include <string.h>

typedef struct gmod__TestData {
    char events[128];
    size_t count;
    gmod_Module *dependency;
    bool fail_load;
    void *scratch;
} gmod__TestData;

static void gmod__test_event(gmod__TestData *data, char c) {
    data->events[data->count++] = c;
    data->events[data->count] = '\0';
}

static bool gmod__test_import_dependency(galloc_Allocator alloc, gmod_Modules *modules, gecs_World *world, void *user) {
    gmod__TestData *data = user;
    (void)world;
    return gmod_Modules_import(alloc, modules, data->dependency);
}

static bool gmod__test_load_a(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)alloc;
    (void)world;
    gmod__test_event(user, 'A');
    return true;
}

static bool gmod__test_load_b(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)alloc;
    (void)world;
    gmod__TestData *data = user;
    gmod__test_event(data, 'B');
    return !data->fail_load;
}

static void gmod__test_unload_a(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)alloc;
    (void)world;
    gmod__test_event(user, 'a');
}

static void gmod__test_unload_b(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)alloc;
    (void)world;
    gmod__test_event(user, 'b');
}

static void gmod__test_update(galloc_Allocator alloc, gecs_World *world, void *user) {
    gmod__TestData *data = user;
    gecs_Component id = GECS_CONTEXT(alloc, world, gmod_FrameAllocator);
    gmod_FrameAllocator *frame = gecs_context_get(world, id);
    mewrequire("Test expectation failed", frame);
    data->scratch = galloc_alloc(frame->alloc, 64);
    mewrequire("Test expectation failed", data->scratch);
    gmod__test_event(data, 'u');
}

static void gmod__test_render(galloc_Allocator alloc, gecs_World *world, void *user) {
    gmod__TestData *data = user;
    (void)alloc;
    (void)world;
    mewrequire("Test expectation failed", data->scratch);
    gmod__test_event(data, 'r');
}

TEST(modules_test, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gmod_Modules *modules = gmod_Modules_new(alloc, world);
    mewassert("Test expectation failed", world && modules);
    mewassert("Test expectation failed", !gmod_Modules_new(alloc, world));
    gmod__TestData data = {0};
    gmod_Module a = {GMOD_INIT, .name = "a", .user = &data, .load = gmod__test_load_a, .unload = gmod__test_unload_a};
    gmod_Module b = {GMOD_INIT, .name = "b", .user = &data, .import = gmod__test_import_dependency, .load = gmod__test_load_b,
                     .unload = gmod__test_unload_b, .update = gmod__test_update, .render = gmod__test_render};
    data.dependency = &a;
    gmod__TestData cycle_a = {0}, cycle_b = {0};
    gmod_Module x = {GMOD_INIT, .name = "x", .user = &cycle_a, .import = gmod__test_import_dependency};
    gmod_Module y = {GMOD_INIT, .name = "y", .user = &cycle_b, .import = gmod__test_import_dependency};
    cycle_a.dependency = &y;
    cycle_b.dependency = &x;
    mewassert("Test expectation failed", !gmod_Modules_import(alloc, modules, &x));
    mewassert("Test expectation failed", gmod_Modules_import(alloc, modules, &b));
    mewassert("Test expectation failed", gmod_Modules_import(alloc, modules, &b));
    data.fail_load = true;
    mewassert("Test expectation failed", !gmod_Modules_load(alloc, modules));
    mewassert("Test expectation failed", strcmp(data.events, "ABa") == 0);
    data.fail_load = false;
    mewassert("Test expectation failed", gmod_Modules_load(alloc, modules));
    mewassert("Test expectation failed", !gmod_Modules_import(alloc, modules, &a));
    gmod_Modules_frame(alloc, modules);
    mewassert("Test expectation failed", strcmp(data.events, "ABaABur") == 0);
    void *first = data.scratch;
    gmod_Modules_frame(alloc, modules);
    mewassert("Test expectation failed", data.scratch == first);
    gmod_Modules_unload(alloc, modules);
    mewassert("Test expectation failed", strcmp(data.events, "ABaABururba") == 0);
    gecs_Component id = GECS_CONTEXT(alloc, world, gmod_FrameAllocator);
    mewassert("Test expectation failed", gecs_context_get(world, id));
    gmod_Modules_free(alloc, modules);
    mewassert("Test expectation failed", !gecs_context_get(world, id));
    gecs_World_free(alloc, world);
})

TEST(staged_modules, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gmod_Modules *modules = gmod_Modules_new(alloc, world);
    gmod__TestData data = {0};
    gmod_Module a = {GMOD_INIT, .name = "a", .user = &data, .load = gmod__test_load_a, .unload = gmod__test_unload_a};
    gmod_Module b = {GMOD_INIT, .name = "b", .user = &data, .load = gmod__test_load_b, .unload = gmod__test_unload_b};
    mewrequire("load baseline", gmod_Modules_import(alloc, modules, &a) && gmod_Modules_load(alloc, modules));
    mewrequire("queue duplicate and new module", gmod_Modules_import_active(alloc, modules, &a) &&
               gmod_Modules_import_active(alloc, modules, &b));
    data.fail_load = true;
    mewassert("failed pending load keeps baseline loaded", !gmod_Modules_load_pending(alloc, modules) &&
              !strcmp(data.events, "AB"));
    data.fail_load = false;
    mewrequire("retry pending load", gmod_Modules_load_pending(alloc, modules));
    gmod_Modules_unload_except(alloc, modules, "a");
    mewassert("dependent unloads first", !strcmp(data.events, "ABBb"));
    gmod_Modules_unload(alloc, modules);
    mewassert("retained module unloads last", !strcmp(data.events, "ABBba"));
    gmod_Modules_free(alloc, modules);
    gecs_World_free(alloc, world);
})

static bool gmod__test_config_import(galloc_Allocator alloc, gmod_Modules *modules, gecs_World *world, void *user) {
    (void)alloc;
    (void)world;
    (void)user;
    const gmod_ModuleConfig *config = gmod_Modules_config(modules, "configured");
    const gmod_ModuleConfigEntry *value = gmod_ModuleConfig_get(config, "text");
    return value && value->kind == GMOD_STRING && strcmp(value->value.string, "hello") == 0;
}

static bool gmod__test_config_dependency_import(galloc_Allocator alloc, gmod_Modules *modules, gecs_World *world, void *user) {
    (void)world;
    bool fail = *(bool *)user;
    const gmod_ModuleConfig *config = gmod_Modules_config(modules, "parent");
    const gmod_ModuleConfigEntry *entry = gmod_ModuleConfig_get(config, "parent-value");
    if (!entry || entry->value.integer != 123) return false;
    for (int i = 0; i < 12; i++) {
        char name[32];
        snprintf(name, sizeof(name), "dependency-%d", i);
        gmod_ModuleConfigEntry value = {.key = "child-value", .kind = GMOD_INT, .value.integer = i};
        gmod_Module dependency = {GMOD_INIT, .name = name, .config = {.entries = &value, .count = 1}};
        if (!gmod_Modules_import(alloc, modules, &dependency)) return false;
    }
    config = gmod_Modules_config(modules, "parent");
    const gmod_ModuleConfigEntry *again = gmod_ModuleConfig_get(config, "parent-value");
    return !fail && again && again == entry && again->value.integer == 123;
}

TEST(module_config_nested_import, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gmod_Modules *modules = gmod_Modules_new(alloc, world);
    mewassert("create module manager", world && modules);
    bool fail = true;
    gmod_ModuleConfigEntry value = {.key = "parent-value", .kind = GMOD_INT, .value.integer = 123};
    gmod_Module parent = {GMOD_INIT, .name = "parent", .user = &fail, .import = gmod__test_config_dependency_import,
                          .config = {.entries = &value, .count = 1}};
    mewassert("failed hook rejects parent", !gmod_Modules_import(alloc, modules, &parent));
    mewassert("failed parent config removed", !gmod_Modules_config(modules, "parent"));
    const gmod_ModuleConfig *child = gmod_Modules_config(modules, "dependency-11");
    mewassert("successful dependency config retained", child &&
              gmod_ModuleConfig_get(child, "child-value")->value.integer == 11);
    fail = false;
    mewassert("parent import can be retried", gmod_Modules_import(alloc, modules, &parent));
    gmod_Modules_free(alloc, modules);
    gecs_World_free(alloc, world);
})

TEST(module_config, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gmod_Modules *modules = gmod_Modules_new(alloc, world);
    mewassert("create module manager", world && modules);
    char key[] = "text", text[] = "hello";
    gmod_ModuleConfigEntry values[] = {
        {.key = key, .kind = GMOD_STRING, .value.string = text},
        {.key = "null", .kind = GMOD_NULL},
        {.key = "bool", .kind = GMOD_BOOL, .value.boolean = true},
        {.key = "int", .kind = GMOD_INT, .value.integer = -42},
        {.key = "double", .kind = GMOD_DOUBLE, .value.real = 1.5},
    };
    gmod_Module module = {GMOD_INIT, .name = "configured", .import = gmod__test_config_import,
                          .config = {.entries = values, .count = 5}};
    mewassert("import with config", gmod_Modules_import(alloc, modules, &module));
    key[0] = 'X'; text[0] = 'X';
    const gmod_ModuleConfig *config = gmod_Modules_config(modules, "configured");
    mewassert("config is copied", config && config->count == 5 &&
              strcmp(gmod_ModuleConfig_get(config, "text")->value.string, "hello") == 0);
    mewassert("null", gmod_ModuleConfig_get(config, "null")->kind == GMOD_NULL);
    mewassert("boolean", gmod_ModuleConfig_get(config, "bool")->value.boolean);
    mewassert("integer", gmod_ModuleConfig_get(config, "int")->value.integer == -42);
    mewassert("double", gmod_ModuleConfig_get(config, "double")->value.real == 1.5);
    mewassert("missing key", !gmod_ModuleConfig_get(config, "missing"));
    mewassert("missing module", !gmod_Modules_config(modules, "missing"));
    gmod_ModuleConfigEntry duplicate[] = {{.key = "x"}, {.key = "x"}};
    gmod_Module invalid = {GMOD_INIT, .name = "invalid", .config = {.entries = duplicate, .count = 2}};
    mewassert("duplicate keys rejected", !gmod_Modules_import(alloc, modules, &invalid));
    mewassert("failed import not retained", !gmod_Modules_config(modules, "invalid"));
    duplicate[1].key = "y";
    duplicate[1].kind = GMOD_STRING;
    mewassert("null string rejected", !gmod_Modules_import(alloc, modules, &invalid));
    mewassert("idempotent import preserves config", gmod_Modules_import(alloc, modules, &module));
    config = gmod_Modules_config(modules, "configured");
    mewassert("original config remains", gmod_ModuleConfig_get(config, "text"));
    gmod_Modules_free(alloc, modules);
    gecs_World_free(alloc, world);
})

TEST(module_descriptor_contract, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gmod_Modules *modules = gmod_Modules_new(alloc, world);
    gmod__TestData data = {0};
    gmod_Module module = {GMOD_INIT, .name = "contract", .user = &data, .load = gmod__test_load_a};
    module.abi_version++;
    mewassert("unknown ABI rejected", !gmod_Modules_import(alloc, modules, &module));
    module.abi_version = GMOD_ABI_VERSION;
    module.struct_size--;
    mewassert("short descriptor rejected", !gmod_Modules_import(alloc, modules, &module));
    module.struct_size = sizeof(module);
    galloc_Allocator wrong = alloc;
    wrong.data = &data;
    mewassert("different allocator rejected", !gmod_Modules_import(wrong, modules, &module));
    mewassert("valid descriptor imported", gmod_Modules_import(alloc, modules, &module));
    module.load = NULL;
    module.user = NULL;
    mewassert("descriptor copied", gmod_Modules_load(alloc, modules));
    mewassert("original callback and user retained", strcmp(data.events, "A") == 0);
    gmod_Modules_free(wrong, modules);
    gmod_Modules_free(alloc, modules);
    gecs_World_free(alloc, world);
})

typedef struct gmod__TestReentrantData {
    gmod_Modules *modules;
    int calls;
} gmod__TestReentrantData;

static void gmod__test_reentrant_update(galloc_Allocator alloc, gecs_World *world, void *user) {
    (void)world;
    gmod__TestReentrantData *data = user;
    data->calls++;
    gmod_Modules_free(alloc, data->modules);
    gmod_Modules_unload(alloc, data->modules);
    gmod_Modules_update(alloc, data->modules);
    gmod_Modules_frame(alloc, data->modules);
}

TEST(module_reentrant_dispatch, {
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gmod_Modules *modules = gmod_Modules_new(alloc, world);
    gmod__TestReentrantData data = {.modules = modules};
    gmod_Module module = {GMOD_INIT, .name = "reentrant", .user = &data, .update = gmod__test_reentrant_update};
    mewassert("import", gmod_Modules_import(alloc, modules, &module));
    mewassert("load", gmod_Modules_load(alloc, modules));
    gmod_Modules_frame(alloc, modules);
    gmod_Modules_frame(alloc, modules);
    mewassert("recursive dispatch and destruction ignored", data.calls == 2);
    gmod_Modules_free(alloc, modules);
    gecs_World_free(alloc, world);
})

typedef struct gmod__TestConfigFailAlloc {
    galloc_Allocator backing;
    size_t calls;
    size_t fail_at;
    size_t live;
} gmod__TestConfigFailAlloc;

static void *gmod__test_config_alloc(void *data, size_t size) {
    gmod__TestConfigFailAlloc *state = data;
    if (++state->calls == state->fail_at) return NULL;
    void *ptr = galloc_alloc(state->backing, size);
    if (ptr) state->live++;
    return ptr;
}

static void *gmod__test_config_calloc(void *data, size_t count, size_t size) {
    gmod__TestConfigFailAlloc *state = data;
    if (++state->calls == state->fail_at) return NULL;
    void *ptr = galloc_calloc(state->backing, count, size);
    if (ptr) state->live++;
    return ptr;
}

static void *gmod__test_config_realloc(void *data, void *ptr, size_t old_size, size_t size) {
    gmod__TestConfigFailAlloc *state = data;
    if (++state->calls == state->fail_at) return NULL;
    void *result = galloc_realloc(state->backing, ptr, old_size, size);
    if (result && !ptr) state->live++;
    return result;
}

static void gmod__test_config_free(void *data, void *ptr) {
    gmod__TestConfigFailAlloc *state = data;
    if (ptr) state->live--;
    galloc_free(state->backing, ptr);
}

TEST(module_config_allocation_failure, {
    static const galloc_FunctionTable gmod__test_functions = {
        .alloc = gmod__test_config_alloc, .calloc = gmod__test_config_calloc, .realloc = gmod__test_config_realloc, .free = gmod__test_config_free
    };
    for (size_t failure = 1; failure <= 6; failure++) {
        gmod__TestConfigFailAlloc state = {.backing = galloc_malloc_allocator()};
        galloc_Allocator alloc = {.data = &state, .ftable = &gmod__test_functions};
        gecs_World *world = gecs_World_new(alloc);
        gmod_Modules *modules = gmod_Modules_new(alloc, world);
        mewassert("create manager", world && modules);
        size_t baseline = state.live;
        gmod_ModuleConfigEntry entries[] = {
            {.key = "one", .kind = GMOD_STRING, .value.string = "value"},
            {.key = "two", .kind = GMOD_INT, .value.integer = 2},
        };
        gmod_Module module = {GMOD_INIT, .name = "test", .config = {.entries = entries, .count = 2}};
        state.fail_at = state.calls + failure;
        mewassert("allocation failure rejects import", !gmod_Modules_import(alloc, modules, &module));
        mewassert("failure frees partial config", state.live == baseline && !gmod_Modules_config(modules, "test"));
        state.fail_at = 0;
        mewassert("import can be retried", gmod_Modules_import(alloc, modules, &module));
        gmod_Modules_free(alloc, modules);
        gecs_World_free(alloc, world);
        mewassert("all allocations released", state.live == 0);
    }
})

mewtest_main();
