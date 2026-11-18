#include <glint/modules/audio/module.h>
#include <glint/modules/core/core_modules.h>
#include <glint/modules/dim2/module.h>
#include <glint/modules/names/module.h>
#include <glint/modules/render/module.h>
#include <glint/modules/script/module.h>
#include <glint/modules/texture/module.h>
#include <glint/modules/time/module.h>
#include <glint/modules/types/module.h>
#include <glint/modules/window/module.h>

static bool gcore__core_import(gmod_Modules* modules, gecs_World* world, void* user);

gmod_Module gcore_module(void) {
    return (gmod_Module) {
        GMOD_INIT,
        .name = "glint.core",
        .import = gcore__core_import,
    };
}

static bool gcore__core_import(gmod_Modules* modules, gecs_World* world, void* user) {
    (void)user;
    galloc_Allocator alloc = glint_allocator(world);

    if (!gmod_Modules_import(alloc, modules, gwindow_module())) return false;
    if (!gmod_Modules_import(alloc, modules, gname_module())) return false;
    if (!gmod_Modules_import(alloc, modules, grender_module())) return false;
    if (!gmod_Modules_import(alloc, modules, gtypes_module())) return false;
    if (!gmod_Modules_import(alloc, modules, gtime_module())) return false;
    if (!gmod_Modules_import(alloc, modules, gtexture_module())) return false;
    if (!gmod_Modules_import(alloc, modules, gaudio_module())) return false;
    if (!gmod_Modules_import(alloc, modules, gdim2_module())) return false;

    return true;
}
