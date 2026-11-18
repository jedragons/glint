# glint

> /ɡlɪnt/ \[*noun*] &mdash; a small, bright flash of light

Simple Game Engine with ECS and Lua Scripting

## Building

Glint builds with CMake and Ninja. Lua and raylib are included under `vendor/`.

```bash
just configure
just build
just test
```

These commands default to `build/default/`. To debug one engine suite directly, run
for example `build/default/tests/glint_suites_test allocator`.

- On Linux, the bundled GLFW builds both Wayland and X11 backends. Install `wayland-scanner`,
  `pkg-config`, and the development files for Wayland, xkbcommon and X11.
- On macOS, use Clang. The vendored Cocoa code uses Apple Blocks, which Homebrew GCC cannot compile.
- Cross builds need the target SDK and libraries. The Windows ARM64 MinGW preset uses LLVM-MinGW.
  Set `CMAKE_CROSSCOMPILING_EMULATOR` to run cross-built tests with a custom runner.

When changing a Lua binding, update its LuaLS stub under `definitions/` by hand for editor
completion and type checking.

## Examples

Building the project also builds the examples under `examples/` (`basic`, `2d/atlas`).
Run one directly from the build output:

```bash
./build/default/examples/glint_basic
```

or through just, which also makes sure it is built first:

```bash
just run-example glint_basic
```

## Using glint in your own project

### CMake

Add glint as a subdirectory of your project and link against the `glint` target.

```cmake
add_subdirectory(path/to/glint)
target_link_libraries(your-app PRIVATE glint)
```

### Xmake

The root `xmake.lua` exposes a package that wraps this repository's own CMake build, so xmake based
projects can depend on it directly, without going through xmake-repo.

```lua
includes("path/to/glint")
add_requires("glint")

target("your-app")
    add_packages("glint")
```
