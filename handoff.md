# Glint C rewrite handoff

## Current state

- Naming update: the engine C API now uses the subsystem-prefixed scheme below;
  older symbol names in the historical notes are not current. Applications use
  `ggame_configure`, shared modules export `gmod_descriptor`, and the rename
  breaks the prior C source/ABI. Release and ASan each passed 30/30 tests,
  including the shared-module fixture; Windows and WebAssembly cross-builds
  succeeded. Lua-visible names and asset keys remain unchanged.

- Branch: `c-rewrite`, based on commit `abc0fe0` (`Fix lua log bindings collapsing under mingw-g++`).
- All rewrite work is **uncommitted**, including many untracked source and test files. Run `git status --short --branch` before changing anything; do not lose the untracked files.
- The C++ implementation and its C++-only vendors have been removed. The C rewrite is the only engine build; remaining parity and verification work is described below.
- The user wants the original engine rewritten in C, not a separate product or a partial facade. Preserve the existing Lua/game behavior where possible. Split former `core` responsibilities into separate modules, like dim2 and audio, rather than recreating a monolithic core.
- Progress on 2026-11-14 (all still uncommitted): **the old C++ engine and its C++-only vendors were deleted** (461 tracked paths show as unstaged ` D`/`M` — recoverable via git while nothing is committed). The root CMakeLists.txt is now a single C-only project (`LANGUAGES C`, no `GLINT_C_ONLY` option — that flag is gone); `README.md` and `xmake.lua` were rewritten for the C-only build; the `justfile` was updated for C globs. Earlier in the session: window/config parity (`fps`, `resizable`, `vsync`, `log_level`, multiple mounts) landed on `GlintGameConfig`; a C log module landed in `sources/module-window`; the dim2 Lua API regained `Atlas.load/:sprite/:animation`, `Sprite.load/get`, `AnimatedSprite.load/get`, value-taking `set` overloads, and `Transform2D.new`, all with tests.
- A parallel feature agent (session `ses_e2930d5c2ffeA4Wgd9Na5VpLfl`) completed and reported: VFS `glint_vfs_read_str`/`glint_vfs_exists`/`glint_vfs_points`; `sources/module-types/include/glint/types.h` (header-only `GlintRect`/`GlintColor` + 25 `GLINT_COLOR_*` constants + inline Vec2 math, no raylib includes); `sources/module-render` (`glint/render.h`: `GlintTexture` from memory or VFS, `glint_draw_texture`/`_params`, `glint_clear_background`; only `render.c` includes raylib and is in `SKIP_UNITY_BUILD_INCLUSION`); a generic asset cache in `module-assets` (`glint/assets/cache.h`: named types, per-callback explicit allocators, cache-hit identity, retryable load failures, reload with stale-on-failure, reverse-order destroy, built-in watcher/poll); names `glint_name_exists`/`_rename`/`_remove`. Behaviors were deliberately NOT migrated onto the cache (they are per-entity, not shared per-path). Its verification: 23/23 ctest on macOS, 23/23 under ASan/UBSan, Windows cross-build of library + all tests + examples.
- Subsequent work: renamed the target to `glint` / `Glint::glint`, added CMake installation and consumer tests, added a five-platform native GitHub Actions matrix, restored the Emscripten asynchronous loop, added working-directory snapshotting, simplified low-value tests, and updated Lua definitions. A Linux ARM64 Ubuntu 24.04 container passed 25/25 tests. The old ZIP backend was never implemented.
- Sol's independent review found a time-binding rebind heap overflow, asset-cache callback reentrancy use-after-free, browser example loop ownership and missing asset preloading, native ECS behavior-destroy hooks not firing, Transform2D vector operation regressions, and package testing/Web dependency issues. These were addressed with regression tests; the renamed tree passed 30/30 tests in both macOS Release and ASan/UBSan. Review reproducers are under the approved temporary directory `opencode/glint-review-bzyihtff/`.
- The user requested a Linux x86 TinyCC build and asked the TCC agent also to try raylib's RGFW backend; results are summarized below. The user approved a repository-wide naming change: types `gprefix_TypeName`, methods `gprefix_TypeName_func` (omit type when it equals the prefix, e.g. `gvec2_add`), free functions `gprefix_func`, private/statics `gprefix__item_name`, constants/macros `GPREFIX_ITEM_NAME`. Agreed prefixes: `galloc`, `garena`, `gstr`, `gecs`, `gmod`, `ggame`, `gvfs`, `gjson`, `gasset`, `gwatch`, `gaudio`, `gdim2`, `gatlas`, `gname`, `grender`, `gscript`, `gbeh`, `gtime`, `gwindow`, `glog`, `gtypes`, `gvec2`, `grect`, `gcolor`. The intentionally ABI-breaking rename and private test/fixture audit are complete; Lua/raylib and Lua-visible identifiers remain unchanged.
- The tree has been reorganized without `sources/c` and with unprefixed utility directories. `Glint::main` is an opt-in engine entry point: applications define `ggame_configure` returning their configuration; `Glint::glint` remains usable without an engine-owned main.
- TinyCC investigation (Ubuntu 24.04 `tcc 0.9.27+git20200814`, Linux x86-64 emulated on Apple ARM): stock TCC needs a CMake `c_std_11` feature workaround; GLFW hits a reproducible glibc `<regex.h>` parse issue, whereas vendored raylib supports `-DPLATFORM=RGFW`. RGFW built engine and examples in a disposable copy after temporary Xrandr linkage and `sinfl.h` `__builtin_clz` fallback changes. The test harness's constructor attributes do not execute under this TCC, so tests segfault and results do not validate engine behavior; a game test also failed compilation with `index too large`. These were diagnostics only, no live source changes; rerun against the settled tree if TCC support is pursued.
- The user clarified that they like importable modules and want the module system restored, with an entirely self-contained `gmod_Module` that can come from a DLL. Runtime has a versioned/sized descriptor initialized by `GMOD_INIT`; a separately compiled macOS shared-library fixture registered an ECS component against the host. The DLL must stay loaded while its callbacks or registered destructors can execute. Audio, 2D, Lua types and time have importable module descriptors; the Lua state, behavior manager, window bindings and default game audio service remain game-managed. Dynamic DLL loading was not specifically requested.

## User requirements and preferences

1. Prefer in-house code. Lua and raylib are allowed third-party dependencies; stb and miniaudio are also acceptable where useful. Raylib already bundles media/backend code. C++ dependencies are acceptable only if they have a complete C interface, but the user prefers the minimal set above.
2. Replace EnTT with a C sparse-set ECS and component/context registration in the spirit of flecs. Replace wtr watcher with an in-house solution.
3. Every engine function that allocates should receive an `Allocator` explicitly, including functions on existing objects. Use the same allocator for an object's lifetime. Lua and raylib have fixed callback/APIs and internally allocate: Lua uses the allocator passed at state creation; raylib controls its own internal allocations. Document such boundaries accurately.
4. A per-frame arena is stored in the ECS world context; temporary allocations expire after the frame. Long-lived objects and Lua state must use a longer-lived allocator.
5. Follow the user's style as exemplified by `/Users/albert/Projects/Repos/Personal/mewlib`: C11, 4 spaces, `PascalCase` types, `snake_case` functions, explicit init/destroy, `Allocator`, `StringView`, and `StringBuilder` patterns. Review source instead of copying bugs blindly.
6. Use the ported mewunit-style test framework in `tests/support/mewunit.h`, with named `TEST` cases and `mewassert`/`mewrequire`, not plain C assertions. `mewrequire` is appropriate in callbacks where longjmp across a library frame would skip cleanup. Checks remain active under `NDEBUG`.
7. Verify macOS and Windows builds. With dependencies already built, the requested clean engine build target is around 100 ms; current CMake/Ninja measurement is above that.
8. Future option only: the user is interested in hot-reloadable native game modules and possibly wasm3 mods with direct ECS access. They explicitly said **do not implement dynamic-library modules immediately**. They later discussed wasm3 and WABT wasm2c as possibilities, not authorization to add them now. A Wasm runtime would add an optional dependency; expose ECS via handles and carefully designed host calls rather than treating Wasm offsets as host pointers.

## Build and test

macOS C build, with tests:

```sh
cmake -S . -B build-c -G Ninja -DGLINT_BUILD_C_TESTS=ON
cmake --build build-c
ctest --test-dir build-c --output-on-failure
```

`GLINT_C_ONLY` no longer exists; there is only the C build. The example executables `glint_c_basic` and `glint_c_atlas` are built without the former `GLINT_C_ONLY` guard. Never *run* those example binaries directly; they open real windows (see Notes).

Windows cross-build used on this Mac:

```sh
cmake -S . -B build-c-windows -G Ninja \
  -DGLINT_BUILD_C_TESTS=ON \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc
cmake --build build-c-windows
```

The Windows cross-build and 23 engine test executables passed under Wine during this session (the installed-package test was not part of that run). Pass plain drive paths (`Z:\Users\...\examples\basic`), because the `\\?\` prefix breaks the VFS's initial-script resolution. The windowed-game test uses full in-test raylib stubs (window, audio, textures) so it never opens a real window; keep it that way. **Never run the `glint_c_basic`/`glint_c_atlas` example binaries directly** — they open a real window and block until it closes; only build them (or run them deliberately with a display). The dim2 test uses raylib stubs and exercises the real atlas example Lua/assets. The basic game test is headless and runs the original basic Lua script.

The updated packaging and tests passed 25/25 on macOS Release, including an installed-package consumer and a stubbed web loop. The Windows cross-build and 23 earlier engine tests passed under Wine; the new web test also passed under Wine separately. ASan/UBSan passed 24 engine tests but the packaging test failed because a single-config build without `CMAKE_BUILD_TYPE` passes an empty `--config`; this needs a fix and re-test. The Emscripten engine target cross-built, but its browser runtime has not been exercised. Re-run these after subsequent changes. A lesson from this session: any file added to the `glint` unity batch must not reference raylib. The unity object is pulled into almost every test, so a raylib reference drags `rcore.c.o` out of `libraylib.a` and collides with the stub tests' stand-ins. `sources/module-window/src/log.c` and `sources/module-render/src/render.c` are therefore listed in `SKIP_UNITY_BUILD_INCLUSION` alongside `game.c`, `c_dim2.c`, `window_lua.c`, and `c_audio.c`; keep new raylib-touching files out of the unity batch the same way.

A warm, engine-only clean rebuild of the former `glint_c_foundation` target through CMake/Ninja, removing only its generated objects/archive and keeping Lua/raylib built, took about **0.355 s** on this Mac, before the latest additions. It excludes dependency builds, examples, and tests; it is not the final engine measurement. A one-translation-unit experimental compile/archive measured 77–108 ms, excluding build-tool startup, but the current CMake build keeps raylib-backed objects separate so stub tests link correctly. Do not claim the 100 ms target is met.

## C implementation map

- `sources/c/`: allocator, reusable arena, strings, and the experimental CMake target. `sources/c/include/glint/alloc.h` follows the mewlib function-table design.
- `sources/ecs/`: generational entities, sparse-set components, named idempotent registration, singleton contexts, cleanup callbacks, and tests. Value storage is byte-relocatable; check header lifetime/iteration restrictions.
- `sources/module-runtime/`: module imports, dependency ordering, load/unload/update/render hooks, and frame arena context.
- `sources/module-script/`: Lua C API wrapper, entity userdata and Lua fields, behaviors, `spawn(path)`, attach/destroy/update, and file reload.
- `sources/module-time/`: C timers and stopwatches, plus Lua/ECS bindings.
- `sources/module-json/`: in-house parser for atlas data, with explicit allocation and failure tests.
- `sources/module-vfs/`: C mounts, file reads, native path resolution, and directory enumeration; Windows paths use UTF-16 OS APIs.
- `sources/module-assets/`: in-house polling watcher for individual asset paths. Polling detects metadata changes, deletion, and reappearance; it can miss changes that leave all observed metadata unchanged.
- `sources/module-dim2/`: C atlas parsing, sprite/animation and Transform2D Lua/ECS path, raylib rendering, and staged JSON/PNG reload preserving live component references when possible. Lua API restored to old shape: `Atlas.load(path)` with `:sprite(name)`/`:animation(name)`, `Sprite.load/get/set/has/remove`, `AnimatedSprite.load/get/set/has/remove` (set accepts path+name or a standalone value), and `Transform2D.new()` standalone values (no arguments or position/scale/origin/rotation). Standalone values store the frame/animation name and re-resolve it on use, so they survive reloads that keep the name; every use re-validates the asset against the live module list, and cross-module or destroyed-module uses raise Lua errors.
- `sources/module-names/`: named ECS entity support used by Lua `Entity.find`, `try_find`, and `name`.
- `sources/module-types/`: Lua Vec2, Rect, Color plus C value types in `glint/types.h` (`GlintRect`, `GlintColor` + named constants, inline Vec2 math); `sources/module-window/`: `GlintLogLevel` + `glint_log_set_level` (`log.h`/`log.c`, forwarding to raylib's TraceLogLevel; the old C++ `set_log_level` was a self-assignment bug that never applied anything) and Lua window properties plus `log_*` globals.
- `sources/module-render/`: C equivalent of the old `platform::Renderer`/`Texture`: `glint/render.h` with `GlintTexture` (memory/VFS load), `GlintTextureDrawParams`, `glint_draw_texture`/`glint_draw_texture_params`/`glint_clear_background`; raylib-backed, `render.c` must stay out of the unity batch.
- `sources/module-assets/`: polling watcher plus a generic registered-type asset cache (`glint/assets/cache.h`) with load/reload/unload callbacks taking explicit allocators.
- `sources/module-names/`: named entities incl. `glint_name_exists`/`glint_name_rename`/`glint_name_remove` (old `NameMap` semantics).
- `sources/module-audio/`: raylib audio-device wrapper and one-pole filter.
- `sources/module-game/`: C game orchestration, lifecycle, Lua, VFS, dim2, timer and window binding setup. `GlintGameConfig` covers `asset_root`, additional `mounts` (later mounts shadow earlier), `initial_script`, `modules`, `width/height/title`, `fps` (0 = uncapped), `resizable`, `vsync`, `log_level`, and `headless`; window flags and `SetTargetFPS` apply only when not headless. C examples are in `examples/basic/main.c` and `examples/2d/atlas/main.c`.

## Important remaining work

1. **Migration and feature parity:** compare deleted C++ sources through Git history with the C implementation; examples passing headlessly do not prove full parity. The old ZIP mount was itself marked TODO, not an implemented backend. Window configuration, dim2 Lua bindings, standalone C textures, renderer, and C value types have been ported.
2. **Assets:** a generic C asset cache and standalone texture API exist. Atlas and behavior loading retain their own loading and reload paths; behavior instances are per entity rather than shared by path.
3. **2D/types compatibility:** Lua types are table-based and permit arbitrary fields; constructor validation and constant mutation semantics are not identical to sol2. The legacy Lua definition generator scans C++ sources and requires reassessment for the C rewrite.
4. **Window/config parity:** implemented this session (`fps`, `resizable`, `vsync`, `log_level`, multiple mounts; `glint_log_set_level`). Window lifecycle and error handling are covered by the stubbed `glint_c_game_window_test` (flags, target FPS, draw begin/end/clear counts, close ordering, headless skips, extra mounts); a real graphical-session check on macOS and Windows is still outstanding.
5. **Lifetimes and allocators:** audit all operations that can indirectly allocate, callbacks and allocator identity. The ECS destructor callback interface has no explicit allocator parameter, so owned values must carry the allocator they need. World teardown, Lua callbacks/finalizers, behavior unloading, module unload, VFS and raylib resource lifetime require careful integration tests. The frame arena is intentionally invalidated at frame end.
6. **Testing and platforms:** run complete sanitizer and Release/NDEBUG builds after changes (last done for all 20 tests this session); test the real graphical examples and audio on macOS and Windows, and verify Windows runtime behavior natively if possible. Wine now covers the windowed-game, game, dim2, window-Lua, and atlas tests, but that is still not a native Windows check, and the windowed tests use raylib stubs rather than a real graphics session. Check CMake's unity build on both platforms when adding C files; file-local symbols can collide, raylib-stub tests require some files outside the unity batch, and any unity-batched file that references raylib breaks those stub links (see the note in Build and test).
7. **Build and packaging:** the C++ engine and C++-only vendors are deleted, and the build is C-only. `.github/workflows/ci.yml` defines native Linux x86-64/ARM64, Windows x86-64/ARM64 and macOS ARM64 jobs; GitHub-hosted execution remains to be verified. `crates/lua-defgen` supports explicit C annotations but the rewritten sources do not contain them; the LuaLS definitions are currently maintained manually as described in `definitions/README.md`.
8. **Performance:** measure the final clean engine build after Lua/raylib are built; optimize toward ~100 ms without sacrificing separate testability or feature parity. Also evaluate watcher cost and ECS iteration before making performance claims.

## Notes for the next agent

- `README.md` documents the C-only build. `handoff.md` is this handoff; the work and this file are uncommitted. Do not commit without the user's direction.
- **Do not run the `glint_c_basic` or `glint_c_atlas` example binaries** (or any non-headless game) directly: they open a real window and block the session until someone closes it. Build-only, or use the stubbed/headless tests.
- Scratch build dirs from this session (all safe to delete): `build-c-only-check{,-win,-notests}`, `build-c-feature{,-asan,-windows}`, `build-c-final`; older `build`, `build-c`, `build-windows`, `build-c-windows` predate it.
- Re-run Wine on the Windows test binaries after the removal + feature changes (use plain `Z:\...` paths; `\\?\` breaks initial-script resolution), plus Release/ASan re-runs.
- Do not interpret an unfinished old API as complete merely because it compiles. Several tests use stubs or only exercise the supplied examples.
- The last user asked for this handoff so they can continue with GLM. Continue from the uncommitted `c-rewrite` working tree; do not restart or discard the C work.
