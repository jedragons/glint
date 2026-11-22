#include "./module.h"
#include "./names_internal.h"

static bool gname__load(gecs_World* world, void* user, char* error, size_t error_size);

gmod_Module gname_module(void) {
    return (gmod_Module) {GMOD_INIT, .name = "glint.hierarchy", .load = gname__load};
}

static bool gname__load(gecs_World* world, void* user, char* error, size_t error_size) {
    (void)error;
    (void)error_size;
    (void)user;

    return gname__register(world) != 0;
}
