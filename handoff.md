# Glint C rewrite handoff

## Commit checkpoint: 2026-11-16

The user has completed the production source review. Manual test review remains
open; keep the test checklist unchecked until that review is complete. The notes
below retain earlier results and decisions; this checkpoint supersedes conflicting
status reports.

Tests now use focused cases with per-test contexts and reusable setup/cleanup
helpers. `GUNIT_TEST` owns a stack-local defer array, configurable through
`GUNIT_DEFER_SIZE`. Assertions use `gunit_return`, which runs deferred callbacks
in reverse order and returns directly. Helpers propagate status explicitly.
Test registration lives in `tests/CMakeLists.txt`; build logic lives in the
`cmake/Glint*Tests.cmake` modules and related helpers.

Latest verification on this Mac:

- Native Debug and ASan/UBSan with stack lifetime checks: 36/36 tests passed.
- Apple Clang 17 (`macos-clang`): 36/36 passed; warm suite time 0.40 seconds.
- MinGW Windows x86_64 with Wine 11.21 Staging: 35/35 passed; warm suite time
  3.31 seconds. Native macOS additionally runs the shared-module test.
- Keep Wine's server alive with `wineserver -p`, then warm it with
  `wine cmd /c exit` with output redirected before running CTest through `just`.
  Restart the old server when upgrading Wine to avoid client/server version
  mismatches.
- Native GCC 16.2 cannot complete the current macOS dependency build. RGFW also
  encountered Apple Blocks declarations in SDK headers. SDL3 compiled the window
  backend, but miniaudio still failed in CoreAudio headers. No shared preset or
  production source changes were made for these experiments; the local
  `macos-gcc` cache currently selects SDL.
- Real graphical/audio sessions and the current tree on Linux remain unverified.
  Linux/TCC work is deferred until the user switches to Linux.

## Latest build layout and macOS verification

Worlds retain their backing allocator and own a temporary arena. Use `glint_allocator(world)` and `glint_temp_allocator(world)`; world-based APIs and module callbacks no longer take a separate allocator. The temporary arena resets at the end of a frame and is destroyed with the world.

Frame delta is shared through `gtime_FrameTime` in the time API. Module load/start/update/render hooks return `bool` and receive an error buffer; the manager owns the diagnostic exposed by `gmod_Modules_error`. There is no frame-status context or scripting pause flag. Failed updates skip rendering, and frame temporary allocations still reset on failure. The module ABI is now version 5.

The test directory is now `tests/`. The engine unity translation unit is `sources/glint/lib.c`. The platform compiles from `sources/glint/platform/lib.c` (Raylib) and `sources/glint/platform/native.c` (filesystem and watcher), and `sources/glint/main.c` is the optional entry-point translation unit. Tests use a mock native platform or a separately compiled web adapter; the platform integration test uses the production library. Debug ASan/UBSan and Release builds passed all 34 CTest tests on macOS. Both graphical examples build; graphics and audio tests use stubs, so real window and audio behavior was not exercised. The engine and examples also cross-build with MinGW for Windows. The frozen-world registration checks permit resolving existing types while rejecting new types. The notes below include older historical results where they differ from this update.

## File review checklist

VFS delegates native file operations to `platform/filesystem.h`. The watcher lives under `platform/watcher.h`; UTF-16, POSIX, and metadata handling stay inside platform code. Native filesystem code compiles separately to isolate Windows headers from Raylib.

Math types now live under `sources/glint/math`. Platform audio, rendering, windowing, and logging each have matching headers and implementations; Glint owns log levels, filtering, callbacks, and severity helpers. Raylib forwards logs to Glint through a callback. Vec2 Lua operations call the math API directly and include `length`/`normalize`.

Lua glue now lives in `bindings.c`/`bindings.h` under dim2, script, time, types, and window. Time, types, and window module declarations now live in `module.h`; script runtime and behavior execution remain in their own files. Binding symbols use distinct `g<module>_bindings_` prefixes. Lua logging is installed once during script runtime initialization, independently of window and world bindings.

The second style pass changes only files left unchecked below. It separates
validation from resource operations, names complex guards, fixes implicit pointer
and numeric conditions, and moves private implementations after public functions.
Test failures now have descriptive messages. `++` and `--` are allowed only in
`for` loop updates; other updates use `+= 1` or `-= 1`.

All 33 previously checked files remain byte-for-byte unchanged. The 109 scoped C
files pass formatting and increment checks. Debug ASan/UBSan and Release each pass
all 32 CTest tests on macOS. Native and Windows syntax checks pass; strict GCC
warnings still flag the existing comparison in reviewed `core/arena.c`. The
Windows capacity guard moved out of VFS and now uses checked allocation.

Added or modified files compared with the merge base of `main` and `c-rewrite` (`3d3740b`), including untracked files. Deleted files are excluded. Existing checkmarks are preserved; check off each file after reviewing it.

- **.github/**
  - **workflows/**
    - [x] `ci.yml`
- **cmake/**
  - [ ] `README.md`
  - **toolchains/**
    - [ ] `linux.cmake`
    - [ ] `macos.cmake`
    - [ ] `mingw.cmake`
    - [ ] `msvc.cmake`
  - [x] `GlintConfig.cmake.in`
  - [x] `GlintExamples.cmake`
  - [x] `GlintExampleTests.cmake`
  - [x] `GlintPackageTest.cmake`
  - [x] `GlintSharedModuleTest.cmake`
  - [x] `GlintTests.cmake`
- **definitions/**
  - **core/**
    - **lua/**
      - [x] `entity.lua`
      - [ ] `logging.lua`
    - **time/**
      - [x] `bindings.lua`
    - **types/**
      - [x] `bindings.lua`
    - **windowing/**
      - [x] `bindings.lua`
  - **module-dim2/**
    - **physics/**
      - **bindings/**
        - [x] `transform.lua`
    - **rendering/**
      - **bindings/**
        - [x] `animated_sprite.lua`
        - [x] `sprite.lua`
  - [x] `README.md`
- **examples/**
  - **2d/**
    - **atlas/**
      - [x] `main.c`
  - **basic/**
    - [x] `main.c`
  - [x] `CMakeLists.txt`
- **sources/**
  - **lua-defgen/**
    - **src/**
      - [x] `main.rs`
    - [x] `Cargo.toml`
  - **glint/**
    - **core/**
      - [x] `alloc.c`
      - [x] `alloc.h`
      - [x] `arena.c`
      - [x] `arena.h`
      - [x] `strings.c`
      - [x] `strings.h`
    - **ecs/**
      - [x] `ecs.c`
      - [x] `ecs.h`
    - **json/**
      - [x] `json.c`
      - [x] `json.h`
    - **math/**
      - [x] `color.h`
      - [x] `rect.h`
      - [x] `vec2.h`
    - **modules/**
      - **assets/**
        - [x] `cache.c`
        - [x] `cache.h`
        - [x] `lib.c`
        - [x] `module.c`
        - [x] `module.h`
      - **audio/**
        - [x] `audio.h`
        - [x] `audio.c`
        - [x] `dsp.c`
        - [x] `dsp.h`
        - [x] `lib.c`
        - [x] `module.c`
        - [x] `module.h`
      - **core/**
        - [x] `core_modules.c`
        - [x] `core_modules.h`
        - [x] `lib.c`
      - **dim2/**
        - [x] `asset.c`
        - [x] `asset.h`
        - [x] `atlas.c`
        - [x] `atlas.h`
        - [x] `bindings.c`
        - [x] `bindings.h`
        - [x] `dim2_internal.h`
        - [x] `dim2.c`
        - [x] `dim2.h`
        - [x] `lib.c`
        - [x] `module.c`
        - [x] `module.h`
      - **names/**
        - [x] `lib.c`
        - [x] `module.c`
        - [x] `module.h`
        - [x] `names.c`
        - [x] `names.h`
        - [x] `names_internal.h`
      - **render/**
        - [x] `lib.c`
        - [x] `module.c`
        - [x] `module.h`
        - [x] `render.c`
        - [x] `render.h`
      - **script/**
        - [x] `behaviors.c`
        - [x] `behaviors.h`
        - [x] `lib.c`
        - [x] `module.c`
        - [x] `script.c`
        - [x] `script.h`
        - [x] `bindings.c`
        - [x] `bindings.h`
        - [x] `script_internal.h`
        - [x] `module.h`
      - **texture/**
        - [x] `lib.c`
        - [x] `module.c`
        - [x] `module.h`
        - [x] `texture_asset.c`
        - [x] `texture_asset.h`
      - **time/**
        - [x] `lib.c`
        - [x] `module.c`
        - [x] `time.c`
        - [x] `time.h`
        - [x] `bindings.c`
        - [x] `bindings.h`
        - [x] `module.h`
      - **types/**
        - [x] `lib.c`
        - [x] `module.c`
        - [x] `bindings.c`
        - [x] `bindings.h`
        - [x] `module.h`
      - **vfs/**
        - [x] `lib.c`
        - [x] `module.c`
        - [x] `module.h`
      - **window/**
        - [x] `lib.c`
        - [x] `module.c`
        - [x] `bindings.c`
        - [x] `bindings.h`
        - [x] `module.h`
    - **platform/**
      - [x] `audio.c`
      - [x] `audio.h`
      - [x] `filesystem.c`
      - [x] `filesystem.h`
      - [x] `lib.c`
      - [x] `logging.c`
      - [x] `logging.h`
      - [x] `native.c`
      - [x] `raylib.c`
      - [x] `raylib_internal.h`
      - [x] `rendering.c`
      - [x] `rendering.h`
      - [x] `watcher.c`
      - [x] `watcher.h`
      - [x] `win32.h`
      - [x] `windowing.c`
      - [x] `windowing.h`
    - **runtime/**
      - [x] `config.h`
      - [x] `modules.c`
      - [x] `modules.h`
    - **unit/**
      - [x] `unit.h`
    - **vfs/**
      - [x] `lib.c`
      - [x] `vfs.c`
      - [x] `vfs.h`
    - [x] `config.h`
    - [x] `game.c`
    - [x] `glint.h`
    - [x] `lib.c`
    - [x] `main.c`
  - [x] `CMakeLists.txt`
- **tests/**
  - **math/**
    - [ ] `math_test.c`
  - **core/**
    - [ ] `allocator_test.c`
    - [ ] `strings_test.c`
  - **ecs/**
    - [ ] `ecs_test.c`
  - **examples/**
    - **stubs/**
      - **glint/**
        - [ ] `glint.h`
      - [ ] `emscripten.h`
      - [ ] `raylib.h`
    - [ ] `entry_test.c`
  - **json/**
    - [ ] `json_test.c`
  - **modules/**
    - **assets/**
      - [ ] `cache_test.c`
    - **audio/**
      - [ ] `audio_test.c`
    - **dim2/**
      - [ ] `atlas_test.c`
      - [ ] `dim2_test.c`
    - **names/**
      - [ ] `names_test.c`
    - **render/**
      - [ ] `render_test.c`
    - **script/**
      - [ ] `behaviors_test.c`
      - [ ] `script_api_test.c`
      - [ ] `script_test.c`
    - **time/**
      - [ ] `bindings_test.c`
      - [ ] `time_test.c`
    - **types/**
      - [ ] `bindings_test.c`
    - **window/**
      - [ ] `bindings_test.c`
  - **packaging/**
    - [ ] `CMakeLists.txt`
    - [ ] `entry.c`
    - [ ] `main.c`
    - [ ] `test_install.cmake`
  - **platform/**
    - **web_stub/**
      - [ ] `emscripten.h`
    - [ ] `filesystem_test.c`
    - [ ] `audio_test.c`
    - [ ] `logging_test.c`
    - [ ] `mock.c`
    - [ ] `rendering_test.c`
    - [ ] `watcher_test.c`
  - **runtime/**
    - [ ] `modules_test.c`
    - [ ] `shared_module.c`
    - [ ] `shared_module_host.c`
  - **unit/**
    - [ ] `check_runner.cmake`
    - [ ] `fatal_test.c`
    - [ ] `runner_test.c`
  - **vfs/**
    - [ ] `vfs_test.c`
  - [ ] `CMakeLists.txt`
  - [ ] `game_test.c`
  - [ ] `game_window_test.c`
- **vendor/**
  - **lua/**
    - [ ] `CMakeLists.txt`
  - **raylib/**
    - [ ] `CMakeLists.txt`
- [ ] `CMakeLists.txt`
- [ ] `CMakePresets.json`
- [ ] `.clangd`
- [ ] `handoff.md`
- [ ] `justfile`
- [ ] `STYLEGUIDE.md`
- [ ] `xmake.lua`

## Current state

- The engine compiles as one translation unit. Platform Raylib and native filesystem code compile separately, with an optional separate entry point. Headers and implementations are organized under `sources/glint`; tests mirror that structure under `tests/`.
- Module imports define load and reverse-unload order. Frame stages are separate from dependency order, and modules report errors through hook results and manager diagnostics. The script module starts the initial behavior after module loading.
- The platform owns the native and browser loop implementations. Game and entry-point code use the same completion callback on both platforms.
- Game shutdown clears entities, freezes the world, and unloads modules in reverse dependency order. Lua finalizers and graphics-device lifetime still deserve review.
- Debug ASan/UBSan and Release builds pass all 34 CTest tests on macOS. Graphics and browser behavior use test doubles; a real window, audio device and browser have not been exercised.
- The broad allocator API migration, render-service decision, and asset reload contract changes from the review remain open. Type and time modules currently import the window module when their Lua/clock contexts are not supplied by the host.

## Historical notes

The notes below describe earlier stages of the rewrite; their paths, API names and verification results may no longer be current.

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

Native build, with tests (defaults to the host compiler):

```sh
just configure
just build
just test
```

Presets use `build/<preset>/`, including `compile_commands.json`; clangd and
`just tidy` explicitly use `build/default/`. Unsuffixed names and `-debug` use
Debug; `-release` uses RelWithDebInfo (MinSizeRel for Emscripten). `debug` and
`release` select the host compiler. See `cmake/README.md` for toolchain
requirements. The default and Homebrew Clang presets each pass 34 tests on this
Mac. Apple Clang builds and macOS Emscripten configures; macOS GCC configures but
cannot build the current Raylib/Cocoa dependencies because of Apple Blocks syntax.
Linux and native Windows presets have not been tested.
The host `release` variant (RelWithDebInfo) builds and passes all 34 tests;
both macOS Emscripten variants configure with their requested build types.
Cross-target presets now use `windows-mingw-*` and `emscripten`, without a host
prefix. Windows MinGW tests are enabled; x86_64 cross builds discover Wine through
`CMAKE_CROSSCOMPILING_EMULATOR`, including the scripted unit-runner check.
The default Wine runner disables its graphical crash debugger per invocation.
Windows x86_64 passes all 33 tests through Wine on this Mac; native macOS still
passes all 34. This exposed and fixed incomplete failing allocators in the native
filesystem and VFS tests, plus test-order dependence in the asset-cache tests.

`GLINT_C_ONLY` no longer exists; there is only the C build. The example executables `glint_basic` and `glint_atlas` are built without the former `GLINT_C_ONLY` guard. Never *run* those example binaries directly; they open real windows (see Notes).

Windows cross-build used on this Mac:

```sh
cmake -S . -B build-c-windows -G Ninja \
  -DGLINT_BUILD_TESTS=ON \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc
cmake --build build-c-windows
```

The Windows cross-build and 23 engine test executables passed under Wine during this session (the installed-package test was not part of that run). Pass plain drive paths (`Z:\Users\...\examples\basic`), because the `\\?\` prefix breaks the VFS's initial-script resolution. The windowed-game test uses full in-test raylib stubs (window, audio, textures) so it never opens a real window; keep it that way. **Never run the `glint_basic`/`glint_atlas` example binaries directly** — they open a real window and block until it closes; only build them (or run them deliberately with a display). The dim2 test uses raylib stubs and exercises the real atlas example Lua/assets. The basic game test is headless and runs the original basic Lua script.

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
4. **Window/config parity:** implemented this session (`fps`, `resizable`, `vsync`, `log_level`, multiple mounts; `glint_log_set_level`). Window lifecycle and error handling are covered by the stubbed `glint_game_window_test` (flags, target FPS, draw begin/end/clear counts, close ordering, headless skips, extra mounts); a real graphical-session check on macOS and Windows is still outstanding.
5. **Lifetimes and allocators:** audit all operations that can indirectly allocate, callbacks and allocator identity. The ECS destructor callback interface has no explicit allocator parameter, so owned values must carry the allocator they need. World teardown, Lua callbacks/finalizers, behavior unloading, module unload, VFS and raylib resource lifetime require careful integration tests. The frame arena is intentionally invalidated at frame end.
6. **Testing and platforms:** run complete sanitizer and Release/NDEBUG builds after changes (last done for all 20 tests this session); test the real graphical examples and audio on macOS and Windows, and verify Windows runtime behavior natively if possible. Wine now covers the windowed-game, game, dim2, window-Lua, and atlas tests, but that is still not a native Windows check, and the windowed tests use raylib stubs rather than a real graphics session. Check CMake's unity build on both platforms when adding C files; file-local symbols can collide, raylib-stub tests require some files outside the unity batch, and any unity-batched file that references raylib breaks those stub links (see the note in Build and test).
7. **Build and packaging:** the C++ engine and C++-only vendors are deleted, and the build is C-only. `.github/workflows/ci.yml` defines native Linux x86-64/ARM64, Windows x86-64/ARM64 and macOS ARM64 jobs; GitHub-hosted execution remains to be verified. `sources/lua-defgen` supports explicit C annotations but the rewritten sources do not contain them; the LuaLS definitions are currently maintained manually as described in `definitions/README.md`.
8. **Performance:** measure the final clean engine build after Lua/raylib are built; optimize toward ~100 ms without sacrificing separate testability or feature parity. Also evaluate watcher cost and ECS iteration before making performance claims.

## Notes for the next agent

- `README.md` documents the C-only build. `handoff.md` is this handoff; the work and this file are uncommitted. Do not commit without the user's direction.
- **Do not run the `glint_basic` or `glint_atlas` example binaries** (or any non-headless game) directly: they open a real window and block the session until someone closes it. Build-only, or use the stubbed/headless tests.
- Scratch build dirs from this session (all safe to delete): `build-c-only-check{,-win,-notests}`, `build-c-feature{,-asan,-windows}`, `build-c-final`; older `build`, `build-c`, `build-windows`, `build-c-windows` predate it.
- Re-run Wine on the Windows test binaries after the removal + feature changes (use plain `Z:\...` paths; `\\?\` breaks initial-script resolution), plus Release/ASan re-runs.
- Do not interpret an unfinished old API as complete merely because it compiles. Several tests use stubs or only exercise the supplied examples.
- The last user asked for this handoff so they can continue with GLM. Continue from the uncommitted `c-rewrite` working tree; do not restart or discard the C work.
