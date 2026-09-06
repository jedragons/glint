# glint

> /ɡlɪnt/ \[*noun*] &mdash; a small, bright flash of light

Simple Game Engine with ECS and Lua Scripting

## Building

glint uses CMake and vendors all of its dependencies under `vendor/`, so no package manager setup is
needed.

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

A `justfile` wraps the same steps: `just configure` runs the CMake configure step, `just check`
configures, builds everything and runs `cargo check` on the workspace crates.

## Examples

Building the project also builds the examples under `examples/` (`basic`, `2d/atlas`, `ui/basic`).
Run one directly from the build output:

```bash
./build/examples/basic
```

or through just, which also makes sure it is built first:

```bash
just run-example basic
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
