#pragma once

#include <glint/core/alloc.h>
#include <glint/core/arena.h>
#include <glint/core/strings.h>
#include <glint/ecs/ecs.h>
#include <glint/json/json.h>
#include <glint/math/color.h>
#include <glint/math/rect.h>
#include <glint/math/vec2.h>
#include <glint/modules/assets/cache.h>
#include <glint/modules/assets/module.h>
#include <glint/modules/audio/audio.h>
#include <glint/modules/audio/dsp.h>
#include <glint/modules/audio/module.h>
#include <glint/modules/core/core_modules.h>
#include <glint/modules/dim2/atlas.h>
#include <glint/modules/dim2/bindings.h>
#include <glint/modules/dim2/dim2.h>
#include <glint/modules/dim2/module.h>
#include <glint/modules/names/module.h>
#include <glint/modules/names/names.h>
#include <glint/modules/render/module.h>
#include <glint/modules/render/render.h>
#include <glint/modules/script/behaviors.h>
#include <glint/modules/script/bindings.h>
#include <glint/modules/script/script.h>
#include <glint/modules/script/module.h>
#include <glint/modules/texture/module.h>
#include <glint/modules/texture/texture_asset.h>
#include <glint/modules/time/bindings.h>
#include <glint/modules/time/module.h>
#include <glint/modules/time/time.h>
#include <glint/modules/types/bindings.h>
#include <glint/modules/types/module.h>
#include <glint/modules/vfs/module.h>
#include <glint/modules/window/bindings.h>
#include <glint/modules/window/module.h>
#include <glint/platform/audio.h>
#include <glint/platform/filesystem.h>
#include <glint/platform/logging.h>
#include <glint/platform/rendering.h>
#include <glint/platform/watcher.h>
#include <glint/platform/windowing.h>
#include <glint/runtime/config.h>
#include <glint/runtime/modules.h>
#include <glint/unit/unit.h>
#include <glint/vfs/vfs.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct glint_Game glint_Game;

/// Application entry point when linking Glint::main. Requires a graphical configuration.
///
/// # Ownership
///
/// Configuration pointers must remain valid through game initialization.
glint_Config glint_main(int argc, char** argv);

/// Create a game. Use the same allocator for all game operations.
glint_Game* glint_new(galloc_Allocator alloc, const glint_Config* config);

/// Destroy a game.
///
/// # Errors
///
/// Returns false without teardown if a module vetoes unload or the asset cache is busy.
/// Retry after the active operation completes.
bool glint_destroy(galloc_Allocator alloc, glint_Game* game);

/// Run one frame using `delta_time` in seconds.
///
/// # Errors
///
/// Returns false on a module failure. `glint_error` retains the message until the next frame.
bool glint_frame(galloc_Allocator alloc, glint_Game* game, float delta_time);

const char* glint_error(const glint_Game* game);

gecs_World* glint_world(glint_Game* game);

gscript_lua_Runtime* glint_script(glint_Game* game);

gvfs_Vfs* glint_vfs(glint_Game* game);

/// Loop completion callback; may destroy the game.
///
/// # Ownership
///
/// The caller keeps the game alive until completion. Destroying it cancels the loop
/// without calling completion. Start and stop outside frame callbacks, on the same thread.
typedef void (*glint_RunEndFn)(galloc_Allocator alloc, glint_Game* game, bool ok, void* user);

bool glint_run(galloc_Allocator alloc, glint_Game* game);

bool glint_run_with_completion(galloc_Allocator alloc, glint_Game* game, glint_RunEndFn end, void* user);

#ifdef __cplusplus
}
#endif
