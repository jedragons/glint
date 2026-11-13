# glint

> /ɡlɪnt/ \[*noun*] &mdash; a small, bright flash of light

Simple game engine with ECS and Lua scripting, written in C11.

## Building

glint uses CMake (3.25+) and vendors all of its dependencies under `vendor/` —
only Lua and raylib, nothing else — so no package manager setup is needed.

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

The engine library target is `glint` (also available as `Glint::glint`).

The C API uses subsystem prefixes, for example `gecs_World`,
`gecs_World_new`, `gvec2_add`, and `ggame_run`. This naming migration breaks
the previous C ABI and source API; rebuild the engine, applications, and
plugins together. No compatibility aliases are provided. Lua API names and
asset formats are unchanged.

The C sources are organized by responsibility under `sources/`: `core` holds
allocation and strings, `glint` owns game orchestration, `ecs` and `runtime`
provide the world and module lifecycle, and directories such as `vfs`,
`assets`, `render`, and `window` contain their respective subsystems.
The `module-` prefix is reserved for world-importable modules. `module-audio`
provides an importable audio device module, and `module-dim2` provides an
importable 2D module. Lua types and time bindings also have importable
descriptors. The game still initializes its default audio service directly.

With tests:

```sh
cmake -S . -B build -G Ninja -DGLINT_BUILD_C_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Examples

Two examples live under `examples/`: `basic` (headless-friendly Lua game loop)
and `2d/atlas` (sprite atlas rendering). Both use the Lua scripting API backed
by the C engine.

They are exercised by the test suite rather than opened directly: the dim2
test drives the real atlas example's Lua and assets through raylib stubs, and
the game tests run the basic example's script headlessly. Running the example
executables themselves opens a real window and requires a graphical session;
real-window and real-audio behavior on macOS and Windows has not been verified
yet.

## Using glint in your own project

### CMake

Add glint as a subdirectory of your project:

```cmake
add_subdirectory(path/to/glint)
target_link_libraries(your-app PRIVATE Glint::main)
```

Alternatively, install with `cmake --install build --prefix /your/prefix`, then
set `CMAKE_PREFIX_PATH` to that prefix in the consuming project:

```cmake
find_package(Glint CONFIG REQUIRED)
target_link_libraries(your-app PRIVATE Glint::main)
```

The installation includes the vendored Lua and raylib libraries and headers.
Static raylib still requires the platform's graphics and window-system libraries.

Implement `ggame_configure` instead of `main` when linking `Glint::main`:

```c
#include <glint/game.h>

ggame_Config ggame_configure(int argc, char **argv) {
    (void)argc; (void)argv;
    return (ggame_Config){.asset_root = "assets", .initial_script = "/game.lua"};
}
```

Configuration pointers must remain valid through initialization (use static
storage for arrays and strings). The engine owns the game and its malloc
allocator, including until the browser loop stops on Emscripten. This entry
point requires a graphical configuration. For headless stepping or embedding,
supply your own `main`, link `Glint::glint` and manage the game yourself;
`ggame_run` retains its documented caller-owned semantics.

### Audio module

`gaudio_module()` returns a `gmod_Module` descriptor suitable for
`gmod_Modules_import`, dependency imports, or a game's module list. A
standalone manager opens the audio device on load, exposes it through the
`gaudio_Service` world context, and closes it on unload. Device initialization
failure fails the load. When a game already supplies that context, the module
borrows it instead: it does not reinitialize or close the game-owned device.
In a headless game the borrowed service is unavailable, and no device is opened.
Check `gaudio_Device_ready` before use and do not close the borrowed handle.
Imports are idempotent by module name. No dynamic library loader is included.
Hosts that load a shared module may look up its `gmod_descriptor` export;
the module ABI and lifetime requirements are documented in `glint/modules.h`.

### 2D and core bindings

`gdim2_module()` imports the Lua types descriptor as a dependency and
installs 2D bindings, updates animation, and renders sprites. Standalone hosts
provide `gtypes_LuaService` and `gdim2_Host` world contexts before loading;
the host must make a graphics context available for textures and bracket render
dispatch with drawing calls. A disabled 2D host supports headless operation.
The game supplies these services and imports 2D by default; importing the same
descriptor again does not initialize or update it twice.

`gtypes_module()` installs Lua value types using `gtypes_LuaService`;
`gtime_module()` installs timers and stopwatches using that service and
`gtime_Host`. Lua bindings and their host data must remain valid for the
documented lifetime of Lua values even after module unload. The Lua state,
behavior manager, window bindings, and game audio service are still managed
directly by the game rather than through importable modules.

### Xmake

The root `xmake.lua` exposes a package that wraps this repository's own CMake
build, so xmake based projects can depend on it directly, without going
through xmake-repo.

```lua
includes("path/to/glint")
add_requires("glint")

target("your-app")
    add_packages("glint")
```

## Notes

- C11, no C++. Windows is supported via MinGW cross-compilation and Wine runs
  of the test suite; a native Windows check is still outstanding.
- The engine APIs take an explicit `galloc_Allocator` for operations that allocate.
  A per-frame arena is exposed through the ECS world context; its allocations
  are valid only until the end of the frame. Lua and raylib allocate
  internally through their own mechanisms (Lua's state allocator, raylib's
  internal allocators) and are documented boundaries.
- The asset watcher is an in-house polling watcher: it reliably notices
  deletion and reappearance but can miss changes that leave observed metadata
  unchanged.
