// Shared-module fixture: the host exports ECS symbols; the plugin shares its runtime.
#include <glint/runtime/modules.h>
#include <glint/unit/unit.h>

#include <dlfcn.h>

#include <string.h>

typedef struct gmod__SharedContext {
    galloc_Allocator alloc;
    void* library;
    gecs_World* world;
    gmod_Modules* modules;
    gecs_Component component;
    int destroyed;
} gmod__SharedContext;

static bool gmod__test_shared_create(gmod__SharedContext* context);
static void gmod__test_shared_free(void* user);

GUNIT_TEST(shared_module_uses_host_component_storage,
    gmod__SharedContext context = {0};
    gunit_defer(gmod__test_shared_free, &context);
    gunit_assert("open and import shared module", gmod__test_shared_create(&context));

    gunit_assert("plugin registers host component", context.component != 0);
    gunit_assert("plugin stores component in host world", gecs_Component_count(context.world, context.component) == 1);
    gunit_assert("plugin module loads", gmod_Modules_load(context.alloc, context.modules));
)

GUNIT_TEST(shared_module_rejects_incompatible_registration,
    gmod__SharedContext context = {0};
    gunit_defer(gmod__test_shared_free, &context);
    gunit_assert("open and import shared module", gmod__test_shared_create(&context));

    gunit_assert(
        "host cannot replace plugin destructor",
        gecs_Component_register(context.world, "test.dll.counter.v1", sizeof(int*), NULL) == 0
    );
)

GUNIT_TEST(shared_module_destructor_outlives_module_manager,
    gmod__SharedContext context = {0};
    gunit_defer(gmod__test_shared_free, &context);
    gunit_assert("open and import shared module", gmod__test_shared_create(&context));
    gunit_assert("plugin module loads", gmod_Modules_load(context.alloc, context.modules));

    gmod_Modules_free(context.alloc, context.modules);
    context.modules = NULL;
    gunit_assert("module teardown leaves world values alive", context.destroyed == 0);

    gecs_World_free(context.world);
    context.world = NULL;
    gunit_assert("world teardown invokes plugin destructor once", context.destroyed == 1);
)

gunit_main(if (gunit_argc != 2) {
    fprintf(stderr, "usage: %s SHARED_MODULE\n", gunit_argv[0]);
    return EXIT_FAILURE;
});

static bool gmod__test_shared_create(gmod__SharedContext* context) {
    context->alloc = galloc_malloc_allocator();
    context->library = dlopen(gunit_argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!gunit_check("shared module opens", context->library != NULL)) {
        return false;
    }
    void* symbol = dlsym(context->library, "gmod_descriptor");
    if (!gunit_check("shared module exports descriptor", symbol != NULL)) {
        return false;
    }
    gmod_DescriptorFn get_descriptor;
    _Static_assert(sizeof(get_descriptor) == sizeof(symbol), "POSIX function pointer size");
    memcpy(&get_descriptor, &symbol, sizeof(get_descriptor));

    context->world = gecs_World_new(context->alloc);
    if (!gunit_check("host world created", context->world != NULL)) {
        return false;
    }
    context->modules = gmod_Modules_new(context->world);
    if (!gunit_check("host module manager created", context->modules != NULL)) {
        return false;
    }

    const gmod_Module* descriptor = get_descriptor();
    if (!gunit_check("shared module provides a descriptor", descriptor != NULL)) {
        return false;
    }
    gmod_Module module = *descriptor;
    module.user = &context->destroyed;
    if (!gunit_check("plugin imports into host", gmod_Modules_import(context->alloc, context->modules, module))) {
        return false;
    }
    context->component = gecs_Component_find(context->world, "test.dll.counter.v1");

    return true;
}

static void gmod__test_shared_free(void* user) {
    gmod__SharedContext* context = user;
    if (context->modules != NULL) {
        gmod_Modules_free(context->alloc, context->modules);
        context->modules = NULL;
    }
    if (context->world != NULL) {
        gecs_World_free(context->world);
        context->world = NULL;
    }
    if (context->library != NULL) {
        int closed = dlclose(context->library);
        context->library = NULL;
        if (!gunit_check("shared module closes after world teardown", closed == 0)) {
            return;
        }
    }
}
