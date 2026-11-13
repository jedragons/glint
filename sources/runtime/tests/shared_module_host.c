/* POSIX integration fixture. Example (macOS: use -dynamiclib
 * -undefined dynamic_lookup instead of -shared, and -Wl,-export_dynamic
 * instead of -rdynamic; omit -ldl):
 * cc -std=c11 -fPIC -shared -Isources/core/include -Isources/ecs/include \
 *   -Isources/runtime/include sources/runtime/tests/shared_module.c -o plugin.so
 * cc -std=c11 -rdynamic -Isources/core/include -Isources/ecs/include \
 *   -Isources/runtime/include sources/runtime/tests/shared_module_host.c \
 *   sources/runtime/src/modules.c sources/core/src/{alloc,arena}.c \
 *   sources/ecs/src/ecs.c -ldl -o plugin-test
 * ./plugin-test ./plugin.so
 * The host exports ECS symbols; the plugin does not link another engine copy. */
#include <glint/modules.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>

int main(int argc, char **argv) {
    assert(argc == 2);
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    assert(library);
    void *symbol = dlsym(library, "gmod_descriptor");
    assert(symbol);
    gmod_DescriptorFn descriptor;
    _Static_assert(sizeof(descriptor) == sizeof(symbol), "POSIX function pointer size");
    memcpy(&descriptor, &symbol, sizeof(descriptor));
    galloc_Allocator alloc = galloc_malloc_allocator();
    gecs_World *world = gecs_World_new(alloc);
    gmod_Modules *modules = gmod_Modules_new(alloc, world);
    int destroyed = 0;
    gmod_Module module = *descriptor();
    module.user = &destroyed;
    assert(gmod_Modules_import(alloc, modules, &module));
    gecs_Component id = gecs_Component_find(world, "test.dll.counter.v1");
    assert(id && gecs_Component_count(world, id) == 1);
    assert(!gecs_Component_register(alloc, world, "test.dll.counter.v1", sizeof(int *), NULL));
    assert(gmod_Modules_load(alloc, modules));
    gmod_Modules_free(alloc, modules);
    assert(destroyed == 0);
    gecs_World_free(alloc, world);
    assert(destroyed == 1);
    assert(dlclose(library) == 0);
    return 0;
}
