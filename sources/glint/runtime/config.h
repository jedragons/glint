#pragma once

#include <glint/modules/vfs/module.h>
#include <glint/platform/logging.h>
#include <glint/runtime/modules.h>

typedef gvfsmod_Mount glint_Mount;

// Available in world context during module import and load. Its pointer fields
// are borrowed and must not be retained beyond game initialization.
typedef struct glint_Config {
    const char* asset_root; // NULL uses the working directory at initialization.
    const glint_Mount* mounts;
    size_t mount_count;
    const char* initial_script;
    const gmod_Module* modules;
    size_t module_count;
    int width;
    int height;
    const char* title;
    int fps; // Zero leaves the frame rate uncapped.
    bool resizable;
    bool vsync;
    glog_Level log_level;
    bool headless;
} glint_Config;
