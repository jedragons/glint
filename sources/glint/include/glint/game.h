#ifndef GGAME_GAME_H
#define GGAME_GAME_H

#include <glint/alloc.h>
#include <glint/ecs.h>
#include <glint/log.h>
#include <glint/modules.h>
#include <glint/script.h>
#include <glint/vfs.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ggame_Game ggame_Game;

/* Registered in the world before user modules are imported. */
typedef struct ggame_Services {
    gvfs_Vfs *vfs;
    gscript_Script *script;
} ggame_Services;

/* Mounts a native directory at a virtual VFS point. Later mounts shadow
 * earlier ones for files under the same point. */
typedef struct ggame_Mount {
    const char *point;
    const char *root;
} ggame_Mount;

typedef struct ggame_Config {
    const char *asset_root; /* NULL snapshots the current working directory at initialization. */
    const ggame_Mount *mounts;
    size_t mount_count;
    const char *initial_script;
    const gmod_Module *modules;
    size_t module_count;
    int width;
    int height;
    const char *title;
    int fps; /* Frames per second cap; 0 leaves the frame rate uncapped. */
    bool resizable;
    bool vsync;
    glog_Level log_level;
    bool headless;
} ggame_Config;

/* Applications linking Glint::main implement this instead of main. The default
 * entry point requires a graphical (non-headless) configuration. Pointers in
 * the returned configuration must remain valid through game initialization;
 * use static storage for arrays, strings, and module descriptors. */
ggame_Config ggame_configure(int argc, char **argv);

/* All game operations use the same allocator and allocator state must outlive
 * the game. A non-headless game owns raylib's window and audio device. */
ggame_Game *ggame_new(galloc_Allocator alloc, const ggame_Config *config);
void ggame_destroy(galloc_Allocator alloc, ggame_Game *game);
/* Call once per frame. A false result indicates an error in a Lua behavior;
 * ggame_error describes it until the next frame or destruction. */
bool ggame_frame(galloc_Allocator alloc, ggame_Game *game, float delta_time);
const char *ggame_error(const ggame_Game *game);
gecs_World *ggame_world(ggame_Game *game);
gscript_Script *ggame_script(ggame_Game *game);
gvfs_Vfs *ggame_vfs(ggame_Game *game);
/* Runs an already-created graphical game; headless games should be stepped
 * with ggame_frame instead. Native builds block until the window closes
 * (true) or a frame fails (false).
 *
 * Emscripten schedules the browser main loop and returns immediately: true
 * means scheduled, not completed. The caller retains ownership and must keep
 * the game and allocator state alive. Closing the window or a frame error
 * cancels the loop but does not destroy the game. Frame errors are logged and
 * remain available via ggame_error; destroy the game when appropriate.
 * Destroying a scheduled game cancels its loop first. Only one game loop may
 * be scheduled at a time; a second run fails. All operations must be on the
 * registering thread, and the game must not be destroyed or manually stepped
 * from a frame callback. Do not replace the Emscripten main loop externally. */
bool ggame_run(galloc_Allocator alloc, ggame_Game *game);

#ifdef __cplusplus
}
#endif
#endif
