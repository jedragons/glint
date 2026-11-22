#pragma once
#include <glint/modules/dim2/atlas.h>
#include <glint/modules/render/render.h>
#include <glint/vfs/vfs.h>

#ifdef __cplusplus
extern "C" {
#endif

char* gdim2__asset_texture_path(galloc_Allocator alloc, const char* path);

bool gdim2__asset_read(galloc_Allocator alloc, gvfs_Vfs* vfs, const char* path, gdim2_atlas_Atlas* atlas);

#ifdef __cplusplus
}
#endif
