#include <glint/modules/core/core_modules.h>
#include <glint/modules/names/names.h>
#include <glint/modules/render/module.h>
#include <glint/modules/script/module.h>
#include <glint/modules/time/bindings.h>
#include <glint/modules/time/module.h>
#include <glint/modules/types/bindings.h>
#include <glint/modules/types/module.h>
#include <glint/modules/window/bindings.h>
#include <glint/modules/window/module.h>
#include <glint/runtime/config.h>
#include <glint/runtime/modules.h>
#include <glint/unit/unit.h>

#include <lauxlib.h>

#include <string.h>

typedef struct gmod__TestData {
    char events[128];
    size_t count;
    gmod_Module* dependency;
    bool fail_load;
    bool veto_unload;
    size_t rendered_frames;
    void* scratch;
} gmod__TestData;

typedef struct gmod__StageProbe {
    gmod__TestData* data;
    char event;
    bool fail;
} gmod__StageProbe;

typedef struct gmod__TestReentrantData {
    gmod_Modules* modules;
    size_t calls;
} gmod__TestReentrantData;

typedef struct gmod__TestConfigFailAlloc {
    galloc_Allocator backing;
    size_t calls;
    size_t fail_at;
    size_t live;
} gmod__TestConfigFailAlloc;

static void gmod__test_event(gmod__TestData* data, char c);
static bool gmod__test_import_dependency(gmod_Modules* modules, gecs_World* world, void* user);
static bool gmod__test_load_a(gecs_World* world, void* user, char* error, size_t error_size);
static bool gmod__test_load_b(gecs_World* world, void* user, char* error, size_t error_size);
static bool gmod__test_start_b(gecs_World* world, void* user, char* error, size_t error_size);
static void gmod__test_unload_a(gecs_World* world, void* user);
static void gmod__test_unload_b(gecs_World* world, void* user);
static bool gmod__test_can_unload(gecs_World* world, void* user);
static bool gmod__test_update(gecs_World* world, void* user, char* error, size_t error_size);
static bool gmod__test_render(gecs_World* world, void* user, char* error, size_t error_size);
static bool gmod__test_stage(gecs_World* world, void* user, char* error, size_t error_size);
static bool gmod__test_config_import(gmod_Modules* modules, gecs_World* world, void* user);
static bool gmod__test_config_dependency_import(gmod_Modules* modules, gecs_World* world, void* user);
static bool gmod__test_reentrant_update(gecs_World* world, void* user, char* error, size_t error_size);
static void* gmod__test_config_alloc(void* data, size_t size);
static void* gmod__test_config_calloc(void* data, size_t count, size_t size);
static void* gmod__test_config_realloc(void* data, void* ptr, size_t old_size, size_t size);
static void gmod__test_config_free(void* data, void* ptr);
static float gmod__test_core_delta(void* user);

typedef struct gmod__TestContext {
    galloc_Allocator alloc;
    gecs_World* world;
    gmod_Modules* modules;
    gmod__TestData data;
    gmod_Module a;
    gmod_Module b;
    gmod__StageProbe early;
    gmod__StageProbe normal;
    gmod__StageProbe late;
    char config_key[5];
    char config_text[6];
    gmod_ModuleConfigEntry config_values[5];
    gmod_Module configured;
    gmod__TestConfigFailAlloc allocations;
    float delta;
    gecs_Component host_type;
} gmod__TestContext;

static bool gmod__test_create(gmod__TestContext* context, bool fail_allocator);
static void gmod__test_free(void* user);
static bool gmod__test_dependencies(gmod__TestContext* context);
static bool gmod__test_stages(gmod__TestContext* context);
static bool gmod__test_configure(gmod__TestContext* context);
static bool gmod__test_core(gmod__TestContext* context);
static bool gmod__test_config_allocation_failure(gmod__TestContext* context, size_t failure, size_t* out_attempts);
static bool gmod__test_binding(gmod__TestContext* context, gmod_Module module, const char* source);

GUNIT_TEST(frame_update_order, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_stages(&context));
    gunit_assert("update succeeds", gmod_Modules_update(context.alloc, context.modules));
    gunit_assert("stages run independently of import order", strcmp(context.data.events, "ENL") == 0);
    gunit_return();
})

GUNIT_TEST(frame_failure_stops_dispatch_and_allows_later_scratch, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_stages(&context));
    context.normal.fail = true;
    void* scratch = galloc_alloc(glint_temp_allocator(context.world), 64);
    gunit_assert("temporary allocation", scratch != NULL);
    gunit_assert("failed update stops the frame", !gmod_Modules_frame(context.alloc, context.modules));
    gunit_assert("failure skips later stages and rendering", strcmp(context.data.events, "EN") == 0);
    gunit_assert("manager retains hook error", strcmp(gmod_Modules_error(context.modules), "stage failed") == 0);
    void* subsequent = galloc_alloc(glint_temp_allocator(context.world), 64);
    gunit_assert("temporary allocation after failed frame succeeds", subsequent != NULL);
    memset(subsequent, 0x5a, 64);
    gunit_assert("temporary allocation after failed frame is writable", ((unsigned char*)subsequent)[63] == 0x5a);
    gunit_return();
})

GUNIT_TEST(render_failure_reaches_caller, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_stages(&context));
    context.normal.fail = true;
    gunit_assert("render failures reach callers", !gmod_Modules_render(context.alloc, context.modules));
    gunit_assert("render error retained", strcmp(gmod_Modules_error(context.modules), "stage failed") == 0);
    gunit_return();
})

GUNIT_TEST(frame_recovers_after_failed_hook, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_stages(&context));
    context.normal.fail = true;
    gunit_assert("first frame fails", !gmod_Modules_frame(context.alloc, context.modules));
    context.normal.fail = false;
    context.data.count = 0;
    context.data.events[0] = '\0';
    gunit_assert("next frame can recover", gmod_Modules_frame(context.alloc, context.modules));
    gunit_assert("successful frame clears error", gmod_Modules_error(context.modules)[0] == '\0');
    gunit_assert("render follows successful updates", strcmp(context.data.events, "ENLN") == 0);
    gunit_return();
})

GUNIT_TEST(cyclic_import_is_rejected, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gmod__TestData left = {0};
    gmod__TestData right = {0};
    gmod_Module x = {GMOD_INIT, .name = "x", .user = &left, .import = gmod__test_import_dependency};
    gmod_Module y = {GMOD_INIT, .name = "y", .user = &right, .import = gmod__test_import_dependency};
    left.dependency = &y;
    right.dependency = &x;
    gunit_assert("cyclic import rejected", !gmod_Modules_import(context.alloc, context.modules, x));
    gunit_return();
})

GUNIT_TEST(repeated_import_loads_once, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_dependencies(&context));
    gunit_assert("repeated import succeeds", gmod_Modules_import(context.alloc, context.modules, context.b));
    gunit_assert("load succeeds", gmod_Modules_load(context.alloc, context.modules));
    gunit_assert("dependency and dependent load once", strcmp(context.data.events, "AB") == 0);
    gunit_return();
})

GUNIT_TEST(failed_load_rolls_back_dependencies, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_dependencies(&context));
    context.data.fail_load = true;
    gunit_assert("load failure reported", !gmod_Modules_load(context.alloc, context.modules));
    gunit_assert(
        "missing diagnostic names failed module",
        strstr(gmod_Modules_error(context.modules), "b") != NULL
            && strstr(gmod_Modules_error(context.modules), "fail") != NULL
    );
    gunit_assert("failed load rolls back dependencies", strcmp(context.data.events, "ABa") == 0);
    gunit_return();
})

GUNIT_TEST(failed_load_can_be_retried, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_dependencies(&context));
    context.data.fail_load = true;
    gunit_assert("first load fails", !gmod_Modules_load(context.alloc, context.modules));
    context.data.fail_load = false;
    gunit_assert("retry load succeeds", gmod_Modules_load(context.alloc, context.modules));
    gunit_assert("retry reloads rolled back dependency", strcmp(context.data.events, "ABaAB") == 0);
    gunit_return();
})

GUNIT_TEST(active_import_is_rejected, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_dependencies(&context));
    gunit_assert("load succeeds", gmod_Modules_load(context.alloc, context.modules));
    gunit_assert("active import rejected", !gmod_Modules_import(context.alloc, context.modules, context.a));
    gunit_return();
})

GUNIT_TEST(frame_dispatch_preserves_dependency_scratch, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_dependencies(&context));
    gunit_assert("load succeeds", gmod_Modules_load(context.alloc, context.modules));
    gunit_assert("frame succeeds", gmod_Modules_frame(context.alloc, context.modules));
    gunit_assert("frame dispatch follows dependency order", strcmp(context.data.events, "ABur") == 0);
    gunit_assert("first frame renders intact temporary contents", context.data.rendered_frames == 1);
    gunit_assert("second frame succeeds", gmod_Modules_frame(context.alloc, context.modules));
    gunit_assert("second frame renders fresh temporary contents", context.data.rendered_frames == 2);
    gunit_assert("both frames complete update and render", strcmp(context.data.events, "ABurur") == 0);
    gunit_return();
})

GUNIT_TEST(unload_reverses_dependency_order, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_dependencies(&context));
    gunit_assert("load succeeds", gmod_Modules_load(context.alloc, context.modules));
    gunit_assert("unload succeeds", gmod_Modules_unload(context.alloc, context.modules));
    gunit_assert("unload reverses dependency order", strcmp(context.data.events, "ABba") == 0);
    gunit_return();
})

GUNIT_TEST(unload_veto_preserves_all_loaded_modules, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    context.a = (gmod_Module) {
        GMOD_INIT,
        .name = "a",
        .user = &context.data,
        .load = gmod__test_load_a,
        .update = gmod__test_load_a,
        .unload = gmod__test_unload_a
    };
    context.b = (gmod_Module) {
        GMOD_INIT,
        .name = "b",
        .user = &context.data,
        .import = gmod__test_import_dependency,
        .load = gmod__test_load_b,
        .update = gmod__test_load_b,
        .unload = gmod__test_unload_b,
        .can_unload = gmod__test_can_unload
    };
    context.data.dependency = &context.a;
    gunit_assert("import vetoing dependent", gmod_Modules_import(context.alloc, context.modules, context.b));
    gunit_assert("load both modules", gmod_Modules_load(context.alloc, context.modules));
    context.data.veto_unload = true;
    gunit_assert(
        "can-unload checks veto without side effects",
        !gmod_Modules_can_unload(context.alloc, context.modules)
    );
    gunit_assert("unload is vetoed", !gmod_Modules_unload(context.alloc, context.modules));
    gunit_assert("no unload hook ran", strcmp(context.data.events, "AB") == 0);
    gunit_assert("both modules remain active", gmod_Modules_update(context.alloc, context.modules));
    gunit_assert("both update hooks still run", strcmp(context.data.events, "ABAB") == 0);
    context.data.veto_unload = false;
    gunit_assert("unload can be retried", gmod_Modules_unload(context.alloc, context.modules));
    gunit_assert("retry unloads both modules in dependency order", strcmp(context.data.events, "ABABba") == 0);
    gunit_return();
})

GUNIT_TEST(pending_load_failure_preserves_baseline, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    context.a = (gmod_Module) {
        GMOD_INIT,
        .name = "a",
        .user = &context.data,
        .load = gmod__test_load_a,
        .unload = gmod__test_unload_a
    };
    context.b = (gmod_Module) {
        GMOD_INIT,
        .name = "b",
        .user = &context.data,
        .load = gmod__test_load_b,
        .unload = gmod__test_unload_b
    };
    gunit_assert(
        "load baseline",
        gmod_Modules_import(context.alloc, context.modules, context.a)
            && gmod_Modules_load(context.alloc, context.modules)
    );
    gunit_assert(
        "queue duplicate and new module",
        gmod_Modules_import_active(context.alloc, context.modules, context.a)
            && gmod_Modules_import_active(context.alloc, context.modules, context.b)
    );
    context.data.fail_load = true;
    gunit_assert(
        "failed pending load keeps baseline loaded",
        !gmod_Modules_load_pending(context.alloc, context.modules)
    );
    gunit_assert("baseline not reloaded or unloaded", strcmp(context.data.events, "AB") == 0);
    gunit_return();
})

GUNIT_TEST(pending_load_can_be_retried, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    context.a = (gmod_Module) {
        GMOD_INIT,
        .name = "a",
        .user = &context.data,
        .load = gmod__test_load_a,
        .unload = gmod__test_unload_a
    };
    context.b = (gmod_Module) {
        GMOD_INIT,
        .name = "b",
        .user = &context.data,
        .load = gmod__test_load_b,
        .unload = gmod__test_unload_b
    };
    gunit_assert(
        "load baseline",
        gmod_Modules_import(context.alloc, context.modules, context.a)
            && gmod_Modules_load(context.alloc, context.modules)
    );
    gunit_assert(
        "queue duplicate and new module",
        gmod_Modules_import_active(context.alloc, context.modules, context.a)
            && gmod_Modules_import_active(context.alloc, context.modules, context.b)
    );
    context.data.fail_load = true;
    gunit_assert("pending load fails", !gmod_Modules_load_pending(context.alloc, context.modules));
    context.data.fail_load = false;
    gunit_assert("retry pending load succeeds", gmod_Modules_load_pending(context.alloc, context.modules));
    gunit_assert("only pending module retried", strcmp(context.data.events, "ABB") == 0);
    gunit_return();
})

GUNIT_TEST(partial_unload_retains_named_module, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    context.a = (gmod_Module) {
        GMOD_INIT,
        .name = "a",
        .user = &context.data,
        .load = gmod__test_load_a,
        .unload = gmod__test_unload_a
    };
    context.b = (gmod_Module) {
        GMOD_INIT,
        .name = "b",
        .user = &context.data,
        .load = gmod__test_load_b,
        .unload = gmod__test_unload_b
    };
    gunit_assert(
        "load baseline",
        gmod_Modules_import(context.alloc, context.modules, context.a)
            && gmod_Modules_load(context.alloc, context.modules)
    );
    gunit_assert(
        "queue duplicate and new module",
        gmod_Modules_import_active(context.alloc, context.modules, context.a)
            && gmod_Modules_import_active(context.alloc, context.modules, context.b)
    );
    gunit_assert("pending load succeeds", gmod_Modules_load_pending(context.alloc, context.modules));
    gunit_assert("partial unload succeeds", gmod_Modules_unload_except(context.alloc, context.modules, "a"));
    gunit_assert("unretained module unloads first", strcmp(context.data.events, "ABb") == 0);
    gunit_assert("remaining unload succeeds", gmod_Modules_unload(context.alloc, context.modules));
    gunit_assert("retained module unloads last", strcmp(context.data.events, "ABba") == 0);
    gunit_return();
})

GUNIT_TEST(start_failure_rolls_back_all_modules, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    context.a = (gmod_Module) {
        GMOD_INIT,
        .name = "a",
        .user = &context.data,
        .load = gmod__test_load_a,
        .unload = gmod__test_unload_a
    };
    context.b = (gmod_Module) {
        GMOD_INIT,
        .name = "b",
        .user = &context.data,
        .start = gmod__test_start_b,
        .unload = gmod__test_unload_b
    };
    gunit_assert(
        "import modules",
        gmod_Modules_import(context.alloc, context.modules, context.a)
            && gmod_Modules_import(context.alloc, context.modules, context.b)
    );
    context.data.fail_load = true;
    gunit_assert("failed start reported", !gmod_Modules_load(context.alloc, context.modules));
    gunit_assert("failed start rolls back all loaded modules", strcmp(context.data.events, "ASba") == 0);
    gunit_assert("start error survives rollback", strcmp(gmod_Modules_error(context.modules), "start failed") == 0);
    gunit_return();
})

GUNIT_TEST(start_failure_can_be_retried, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    context.a = (gmod_Module) {
        GMOD_INIT,
        .name = "a",
        .user = &context.data,
        .load = gmod__test_load_a,
        .unload = gmod__test_unload_a
    };
    context.b = (gmod_Module) {
        GMOD_INIT,
        .name = "b",
        .user = &context.data,
        .start = gmod__test_start_b,
        .unload = gmod__test_unload_b
    };
    gunit_assert(
        "import modules",
        gmod_Modules_import(context.alloc, context.modules, context.a)
            && gmod_Modules_import(context.alloc, context.modules, context.b)
    );
    context.data.fail_load = true;
    gunit_assert("failed start reported", !gmod_Modules_load(context.alloc, context.modules));
    context.data.fail_load = false;
    gunit_assert("load can be retried", gmod_Modules_load(context.alloc, context.modules));
    gunit_return();
})

GUNIT_TEST(failed_parent_import_retains_dependency_config, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    bool fail = true;
    gmod_ModuleConfigEntry value = {.key = "parent-value", .kind = GMOD_INT, .value.integer = 123};
    gmod_Module parent = {
        GMOD_INIT,
        .name = "parent",
        .user = &fail,
        .import = gmod__test_config_dependency_import,
        .config = {.entries = &value, .count = 1}
    };
    gunit_assert("failed hook rejects parent", !gmod_Modules_import(context.alloc, context.modules, parent));
    gunit_assert("failed parent config removed", (gmod_Modules_config(context.modules, "parent") == NULL));
    const gmod_ModuleConfig* child = gmod_Modules_config(context.modules, "dependency-11");
    gunit_assert(
        "successful dependency config retained",
        (child != NULL) && gmod_ModuleConfig_get(child, "child-value")->value.integer == 11
    );
    fail = false;
    gunit_assert("parent import can be retried", gmod_Modules_import(context.alloc, context.modules, parent));
    gunit_return();
})

GUNIT_TEST(configuration_copies_owned_strings, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_configure(&context));
    context.config_key[0] = 'X';
    context.config_text[0] = 'X';
    const gmod_ModuleConfig* config = gmod_Modules_config(context.modules, "configured");
    gunit_assert("config copied", config != NULL && config->count == 5);
    gunit_assert(
        "config strings owned independently",
        strcmp(gmod_ModuleConfig_get(config, "text")->value.string, "hello") == 0
    );
    gunit_return();
})

GUNIT_TEST(configuration_preserves_value_types, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_configure(&context));
    const gmod_ModuleConfig* config = gmod_Modules_config(context.modules, "configured");
    gunit_assert("null", gmod_ModuleConfig_get(config, "null")->kind == GMOD_NULL);
    gunit_assert("boolean", gmod_ModuleConfig_get(config, "bool")->value.boolean);
    gunit_assert("integer", gmod_ModuleConfig_get(config, "int")->value.integer == -42);
    gunit_assert("double", gmod_ModuleConfig_get(config, "double")->value.real == 1.5);
    gunit_return();
})

GUNIT_TEST(configuration_missing_values_return_null, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_configure(&context));
    const gmod_ModuleConfig* config = gmod_Modules_config(context.modules, "configured");
    gunit_assert("missing key", gmod_ModuleConfig_get(config, "missing") == NULL);
    gunit_assert("missing module", gmod_Modules_config(context.modules, "missing") == NULL);
    gunit_return();
})

GUNIT_TEST(configuration_duplicate_keys_rejected, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gmod_ModuleConfigEntry values[] = {{.key = "x"}, {.key = "x"}};
    gmod_Module module = {GMOD_INIT, .name = "invalid", .config = {.entries = values, .count = 2}};
    gunit_assert("duplicate keys rejected", !gmod_Modules_import(context.alloc, context.modules, module));
    gunit_assert("failed import not retained", gmod_Modules_config(context.modules, "invalid") == NULL);
    gunit_return();
})

GUNIT_TEST(configuration_null_string_rejected, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gmod_ModuleConfigEntry value = {.key = "text", .kind = GMOD_STRING};
    gmod_Module module = {GMOD_INIT, .name = "invalid", .config = {.entries = &value, .count = 1}};
    gunit_assert("null string rejected", !gmod_Modules_import(context.alloc, context.modules, module));
    gunit_assert("failed import not retained", gmod_Modules_config(context.modules, "invalid") == NULL);
    gunit_return();
})

GUNIT_TEST(repeated_import_preserves_configuration, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_configure(&context));
    context.config_key[0] = 'X';
    context.config_text[0] = 'X';
    gunit_assert("idempotent import succeeds", gmod_Modules_import(context.alloc, context.modules, context.configured));
    const gmod_ModuleConfig* config = gmod_Modules_config(context.modules, "configured");
    gunit_assert("original config remains", gmod_ModuleConfig_get(config, "text") != NULL);
    gunit_return();
})

GUNIT_TEST(module_unknown_abi_rejected, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gmod_Module module = {GMOD_INIT, .name = "contract"};
    module.abi_version += 1;
    gunit_assert("invalid descriptor rejected", !gmod_Modules_import(context.alloc, context.modules, module));
    gunit_return();
})

GUNIT_TEST(module_short_descriptor_rejected, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gmod_Module module = {GMOD_INIT, .name = "contract"};
    module.struct_size -= 1;
    gunit_assert("invalid descriptor rejected", !gmod_Modules_import(context.alloc, context.modules, module));
    gunit_return();
})

GUNIT_TEST(module_descriptor_is_copied, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gmod_Module module = {GMOD_INIT, .name = "contract", .user = &context.data, .load = gmod__test_load_a};
    gunit_assert("valid descriptor imported", gmod_Modules_import(context.alloc, context.modules, module));
    module.load = NULL;
    module.user = NULL;
    gunit_assert("descriptor copied", gmod_Modules_load(context.alloc, context.modules));
    gunit_assert("original callback and user retained", strcmp(context.data.events, "A") == 0);
    gunit_return();
})

GUNIT_TEST(module_reentrant_dispatch_is_ignored, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gmod__TestReentrantData data = {.modules = context.modules};
    gmod_Module module = {GMOD_INIT, .name = "reentrant", .user = &data, .update = gmod__test_reentrant_update};
    gunit_assert("import", gmod_Modules_import(context.alloc, context.modules, module));
    gunit_assert("load", gmod_Modules_load(context.alloc, context.modules));
    gunit_assert("first frame", gmod_Modules_frame(context.alloc, context.modules));
    gunit_assert("second frame", gmod_Modules_frame(context.alloc, context.modules));
    gunit_assert("recursive dispatch and destruction ignored", data.calls == 2);
    gunit_return();
})

GUNIT_TEST(configuration_allocation_denial_preserves_import_contract, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, true));
    size_t attempts = 0;
    gunit_assert(
        "discover successful import allocations",
        gmod__test_config_allocation_failure(&context, 0, &attempts)
    );
    gmod__test_free(&context);
    gunit_assert("successful import releases owned allocations on teardown", context.allocations.live == 0);
    for (size_t failure = 1; failure <= attempts; failure++) {
        context = (gmod__TestContext) {0};
        gunit_assert("create test context", gmod__test_create(&context, true));
        gunit_assert("test helper succeeds", gmod__test_config_allocation_failure(&context, failure, NULL));
        gmod__test_free(&context);
        gunit_assert("allocation denial leaves no owned resources after teardown", context.allocations.live == 0);
    }
    gunit_return();
})

GUNIT_TEST(types_module_imports_binding_dependencies, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_binding(&context, gtypes_module(), "assert(Vec2 ~= nil)"));
    gunit_return();
})

GUNIT_TEST(time_module_imports_binding_dependencies, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_binding(&context, gtime_module(), "assert(Timer ~= nil)"));
    gunit_return();
})

GUNIT_TEST(core_module_installs_services, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_core(&context));
    gecs_World* world = context.world;
    gtypes_LuaService* lua = gecs_context_get(world, GECS_CONTEXT(world, gtypes_LuaService));
    grender_Service* render = gecs_context_get(world, GECS_CONTEXT(world, grender_Service));
    gunit_assert("Lua service installed", lua != NULL && lua->lua != NULL);
    gunit_assert(
        "render service installed",
        render != NULL && render->draw_texture != NULL && render->clear_background != NULL
    );
    gunit_assert("time service installed", gecs_context_get(world, GECS_CONTEXT(world, gtime_Host)) != NULL);
    gunit_assert("script service installed", gecs_context_get(world, GECS_CONTEXT(world, gscript_Service)) != NULL);
    gunit_return();
})

GUNIT_TEST(core_module_installs_lua_bindings, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_core(&context));
    gtypes_LuaService* lua = gecs_context_get(context.world, GECS_CONTEXT(context.world, gtypes_LuaService));
    gunit_assert("Lua service available", lua != NULL && lua->lua != NULL);
    gunit_assert(
        "window and type Lua bindings",
        luaL_dostring(
            lua->lua,
            "assert(window.dt == 0.25); local v = Vec2.new(2, 3); assert(v.x == 2 and v.y == 3); assert(Timer ~= nil)"
        ) == 0
    );
    gunit_return();
})

GUNIT_TEST(core_unload_removes_owned_contexts, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_core(&context));
    gecs_World* world = context.world;
    gecs_Component lua = GECS_CONTEXT(world, gtypes_LuaService);
    gecs_Component time = GECS_CONTEXT(world, gtime_Host);
    gecs_Component render = GECS_CONTEXT(world, grender_Service);
    gecs_Component script = GECS_CONTEXT(world, gscript_Service);
    gunit_assert("unload core", gmod_Modules_unload(context.alloc, context.modules));
    gunit_assert(
        "owned contexts removed",
        gecs_context_get(world, lua) == NULL && gecs_context_get(world, time) == NULL
            && gecs_context_get(world, render) == NULL && gecs_context_get(world, script) == NULL
    );
    gunit_return();
})

GUNIT_TEST(core_unload_preserves_host_services_and_names, {
    gmod__TestContext context = {0};
    gunit_defer(gmod__test_free, &context);
    gunit_assert("create test context", gmod__test_create(&context, false));
    gunit_assert("test helper succeeds", gmod__test_core(&context));
    gecs_Entity entity = gecs_spawn(context.world);
    gunit_assert(
        "hierarchy names work",
        entity.id != 0 && gname_set(entity, "hero") && gname_find(context.world, "hero").id == entity.id
    );
    gunit_assert("unload core", gmod_Modules_unload(context.alloc, context.modules));
    gunit_assert("host clock survives", gecs_context_get(context.world, context.host_type) != NULL);
    gunit_assert("entity names survive", gname_find(context.world, "hero").id == entity.id);
    gunit_return();
})

gunit_main();

static bool gmod__test_create(gmod__TestContext* context, bool fail_allocator) {
    static const galloc_FunctionTable functions = {
        .alloc = gmod__test_config_alloc,
        .calloc = gmod__test_config_calloc,
        .realloc = gmod__test_config_realloc,
        .free = gmod__test_config_free
    };
    context->allocations.backing = galloc_malloc_allocator();
    context->alloc = fail_allocator
        ? (galloc_Allocator) {.data = &context->allocations, .ftable = &functions}
        : context->allocations.backing;
    context->world = gecs_World_new(context->alloc);
    if (!gunit_check("create world", context->world != NULL)) {
        return false;
    }
    context->modules = gmod_Modules_new(context->world);
    if (!gunit_check("create module manager", context->modules != NULL)) {
        return false;
    }
    return true;
}

static void gmod__test_free(void* user) {
    gmod__TestContext* context = user;
    context->allocations.fail_at = 0;
    context->data.veto_unload = false;
    if (context->modules != NULL) {
        gmod_Modules_free(context->alloc, context->modules);
        context->modules = NULL;
    }
    if (context->world != NULL) {
        gecs_World_free(context->world);
        context->world = NULL;
    }
}

static bool gmod__test_dependencies(gmod__TestContext* context) {
    context->a = (gmod_Module) {
        GMOD_INIT,
        .name = "a",
        .user = &context->data,
        .load = gmod__test_load_a,
        .unload = gmod__test_unload_a
    };
    context->b = (gmod_Module) {
        GMOD_INIT,
        .name = "b",
        .user = &context->data,
        .import = gmod__test_import_dependency,
        .load = gmod__test_load_b,
        .unload = gmod__test_unload_b,
        .update = gmod__test_update,
        .render = gmod__test_render
    };
    context->data.dependency = &context->a;
    if (!gunit_check("import dependency module", gmod_Modules_import(context->alloc, context->modules, context->b))) {
        return false;
    }
    return true;
}

static bool gmod__test_stages(gmod__TestContext* context) {
    context->late = (gmod__StageProbe) {.data = &context->data, .event = 'L'};
    context->normal = (gmod__StageProbe) {.data = &context->data, .event = 'N'};
    context->early = (gmod__StageProbe) {.data = &context->data, .event = 'E'};
    gmod_Module late =
        {GMOD_INIT, .name = "late", .user = &context->late, .update = gmod__test_stage, .update_order = 1};
    gmod_Module normal =
        {GMOD_INIT, .name = "normal", .user = &context->normal, .update = gmod__test_stage, .render = gmod__test_stage};
    gmod_Module early =
        {GMOD_INIT, .name = "early", .user = &context->early, .update = gmod__test_stage, .update_order = -1};
    if (!gunit_check(
            "load modules",
            gmod_Modules_import(context->alloc, context->modules, late)
                && gmod_Modules_import(context->alloc, context->modules, normal)
                && gmod_Modules_import(context->alloc, context->modules, early)
                && gmod_Modules_load(context->alloc, context->modules)
        )) {
        return false;
    }
    return true;
}

static bool gmod__test_configure(gmod__TestContext* context) {
    memcpy(context->config_key, "text", 5);
    memcpy(context->config_text, "hello", 6);
    context->config_values[0] = (gmod_ModuleConfigEntry) {
        .key = context->config_key,
        .kind = GMOD_STRING,
        .value.string = context->config_text
    };
    context->config_values[1] = (gmod_ModuleConfigEntry) {.key = "null", .kind = GMOD_NULL};
    context->config_values[2] = (gmod_ModuleConfigEntry) {.key = "bool", .kind = GMOD_BOOL, .value.boolean = true};
    context->config_values[3] = (gmod_ModuleConfigEntry) {.key = "int", .kind = GMOD_INT, .value.integer = -42};
    context->config_values[4] = (gmod_ModuleConfigEntry) {.key = "double", .kind = GMOD_DOUBLE, .value.real = 1.5};
    context->configured = (gmod_Module) {
        GMOD_INIT,
        .name = "configured",
        .import = gmod__test_config_import,
        .config = {.entries = context->config_values, .count = 5}
    };
    if (!gunit_check(
            "import with config",
            gmod_Modules_import(context->alloc, context->modules, context->configured)
        )) {
        return false;
    }
    return true;
}

static bool gmod__test_config_allocation_failure(gmod__TestContext* context, size_t failure, size_t* out_attempts) {
    gmod_ModuleConfigEntry entries[] = {
        {.key = "one", .kind = GMOD_STRING, .value.string = "value"},
        {.key = "two", .kind = GMOD_INT, .value.integer = 2}
    };
    gmod_Module module = {GMOD_INIT, .name = "test", .config = {.entries = entries, .count = 2}};
    size_t before = context->allocations.calls;
    context->allocations.fail_at = failure != 0 ? before + failure : 0;
    bool imported = gmod_Modules_import(context->alloc, context->modules, module);
    size_t attempts = context->allocations.calls - before;
    context->allocations.fail_at = 0;
    if (failure == 0) {
        if (!gunit_check("healthy allocator imports configuration", imported)) {
            return false;
        }
    }
    if (!imported) {
        if (!gunit_check(
                "failed import does not expose partial configuration",
                gmod_Modules_config(context->modules, "test") == NULL
            )) {
            return false;
        }
        if (!gunit_check(
                "failed import can be retried",
                gmod_Modules_import(context->alloc, context->modules, module)
            )) {
            return false;
        }
    }

    const gmod_ModuleConfig* config = gmod_Modules_config(context->modules, "test");
    if (!gunit_check("configuration exists after successful or retried import", config != NULL)) {
        return false;
    }
    const gmod_ModuleConfigEntry* one = gmod_ModuleConfig_get(config, "one");
    const gmod_ModuleConfigEntry* two = gmod_ModuleConfig_get(config, "two");
    if (!gunit_check(
            "successful or retried import retains string value",
            one != NULL && one->kind == GMOD_STRING && strcmp(one->value.string, "value") == 0
        )) {
        return false;
    }
    if (!gunit_check(
            "successful or retried import retains integer value",
            two != NULL && two->kind == GMOD_INT && two->value.integer == 2
        )) {
        return false;
    }
    if (out_attempts != NULL) {
        *out_attempts = attempts;
    }
    return true;
}

static bool gmod__test_binding(gmod__TestContext* context, gmod_Module module, const char* source) {
    if (!gunit_check(
            "load binding with dependencies",
            gmod_Modules_import(context->alloc, context->modules, module)
                && gmod_Modules_load(context->alloc, context->modules)
        )) {
        return false;
    }
    gtypes_LuaService* service = gecs_context_get(context->world, GECS_CONTEXT(context->world, gtypes_LuaService));
    if (!gunit_check("Lua service available", service != NULL && service->lua != NULL)) {
        return false;
    }
    if (!gunit_check("binding available", luaL_dostring(service->lua, source) == 0)) {
        return false;
    }
    return true;
}

static bool gmod__test_core(gmod__TestContext* context) {
    context->delta = 0.25f;
    gmod_Host policy = {.headless = true};
    if (!gunit_check(
            "provide headless policy",
            gecs_context_set(context->world, GECS_CONTEXT(context->world, gmod_Host), &policy) != NULL
        )) {
        return false;
    }
    gwindow_Host host = {.frame_time = gmod__test_core_delta, .user = &context->delta};
    context->host_type = GECS_CONTEXT(context->world, gwindow_Host);
    if (!gunit_check("provide clock", gecs_context_set(context->world, context->host_type, &host) != NULL)) {
        return false;
    }
    glint_Config config = {.headless = true};
    if (!gunit_check(
            "provide module configuration",
            gecs_context_set(context->world, GECS_CONTEXT(context->world, glint_Config), &config) != NULL
        )) {
        return false;
    }
    if (!gunit_check("import core", gmod_Modules_import(context->alloc, context->modules, gcore_module()))) {
        return false;
    }
    if (!gunit_check("load core dependencies", gmod_Modules_load(context->alloc, context->modules))) {
        return false;
    }
    return true;
}

static void gmod__test_event(gmod__TestData* data, char c) {
    data->events[data->count] = c;
    data->count += 1;
    data->events[data->count] = '\0';
}

static bool gmod__test_import_dependency(gmod_Modules* modules, gecs_World* world, void* user) {
    galloc_Allocator alloc = glint_allocator(world);
    gmod__TestData* data = user;
    (void)world;
    return gmod_Modules_import(alloc, modules, *data->dependency);
}

static bool gmod__test_load_a(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)world;
    gmod__test_event(user, 'A');
    return true;
}

static bool gmod__test_load_b(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)world;
    gmod__TestData* data = user;
    gmod__test_event(data, 'B');
    return !data->fail_load;
}

static bool gmod__test_start_b(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)world;
    gmod__TestData* data = user;
    gmod__test_event(data, 'S');
    if (data->fail_load) {
        snprintf(error, error_size, "start failed");
    }

    return !data->fail_load;
}

static void gmod__test_unload_a(gecs_World* world, void* user) {
    (void)world;
    gmod__test_event(user, 'a');
}

static void gmod__test_unload_b(gecs_World* world, void* user) {
    (void)world;
    gmod__test_event(user, 'b');
}

static bool gmod__test_can_unload(gecs_World* world, void* user) {
    (void)world;
    const gmod__TestData* data = user;
    return !data->veto_unload;
}

static bool gmod__test_update(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    gmod__TestData* data = user;
    data->scratch = galloc_alloc(glint_temp_allocator(world), 64);
    if (!gunit_check("allocate update scratch", data->scratch != NULL)) {
        return false;
    }
    memset(data->scratch, 0x5a, 64);
    gmod__test_event(data, 'u');
    return true;
}

static bool gmod__test_render(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    gmod__TestData* data = user;
    (void)world;
    if (!gunit_check("scratch survives through render", data->scratch != NULL)) {
        return false;
    }
    const unsigned char* bytes = data->scratch;
    for (size_t i = 0; i < 64; i++) {
        if (!gunit_check("temporary contents survive from update through render", bytes[i] == 0x5a)) {
            return false;
        }
    }
    data->scratch = NULL;
    data->rendered_frames += 1;
    gmod__test_event(data, 'r');
    return true;
}

static bool gmod__test_stage(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)world;
    gmod__StageProbe* probe = user;
    gmod__test_event(probe->data, probe->event);
    if (probe->fail) {
        snprintf(error, error_size, "stage failed");
        return false;
    }

    return true;
}

static bool gmod__test_config_import(gmod_Modules* modules, gecs_World* world, void* user) {
    (void)world;
    (void)user;
    const gmod_ModuleConfig* config = gmod_Modules_config(modules, "configured");
    const gmod_ModuleConfigEntry* value = gmod_ModuleConfig_get(config, "text");
    return value != NULL && value->kind == GMOD_STRING && strcmp(value->value.string, "hello") == 0;
}

static bool gmod__test_config_dependency_import(gmod_Modules* modules, gecs_World* world, void* user) {
    galloc_Allocator alloc = glint_allocator(world);
    (void)world;
    bool fail = *(bool*)user;
    const gmod_ModuleConfig* config = gmod_Modules_config(modules, "parent");
    const gmod_ModuleConfigEntry* entry = gmod_ModuleConfig_get(config, "parent-value");
    if (entry == NULL || entry->kind != GMOD_INT || entry->value.integer != 123) {
        return false;
    }

    for (size_t i = 0; i < 12; i++) {
        char name[32];
        snprintf(name, sizeof(name), "dependency-%zu", i);
        gmod_ModuleConfigEntry value = {.key = "child-value", .kind = GMOD_INT, .value.integer = i};
        gmod_Module dependency = {GMOD_INIT, .name = name, .config = {.entries = &value, .count = 1}};
        if (!gmod_Modules_import(alloc, modules, dependency)) {
            return false;
        }
    }

    config = gmod_Modules_config(modules, "parent");
    const gmod_ModuleConfigEntry* again = gmod_ModuleConfig_get(config, "parent-value");
    return !fail && again != NULL && again->kind == GMOD_INT && strcmp(again->key, "parent-value") == 0
        && again->value.integer == 123;
}

static bool gmod__test_reentrant_update(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    galloc_Allocator alloc = glint_allocator(world);
    (void)world;
    gmod__TestReentrantData* data = user;
    data->calls += 1;
    gmod_Modules_free(alloc, data->modules);
    gmod_Modules_unload(alloc, data->modules);
    gmod_Modules_update(alloc, data->modules);
    gmod_Modules_frame(alloc, data->modules);
    return true;
}

static void* gmod__test_config_alloc(void* data, size_t size) {
    gmod__TestConfigFailAlloc* state = data;
    state->calls += 1;
    if (state->calls == state->fail_at) {
        return NULL;
    }

    void* ptr = galloc_alloc(state->backing, size);
    if (ptr != NULL) {
        state->live += 1;
    }

    return ptr;
}

static void* gmod__test_config_calloc(void* data, size_t count, size_t size) {
    gmod__TestConfigFailAlloc* state = data;
    state->calls += 1;
    if (state->calls == state->fail_at) {
        return NULL;
    }

    void* ptr = galloc_calloc(state->backing, count, size);
    if (ptr != NULL) {
        state->live += 1;
    }

    return ptr;
}

static void* gmod__test_config_realloc(void* data, void* ptr, size_t old_size, size_t size) {
    gmod__TestConfigFailAlloc* state = data;
    state->calls += 1;
    if (state->calls == state->fail_at) {
        return NULL;
    }

    void* result = galloc_realloc(state->backing, ptr, old_size, size);
    if (result != NULL && ptr == NULL) {
        state->live += 1;
    }

    return result;
}

static void gmod__test_config_free(void* data, void* ptr) {
    gmod__TestConfigFailAlloc* state = data;
    if (ptr != NULL) {
        state->live -= 1;
    }

    galloc_free(state->backing, ptr);
}

static float gmod__test_core_delta(void* user) {
    return *(float*)user;
}
