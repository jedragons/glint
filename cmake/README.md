# Build presets

`just configure [preset]`, `just build [preset]` and `just test [preset]` default
to `default`. Configure first, then build and test with the same preset name.
List available presets with `cmake --list-presets`. All use Ninja. Unsuffixed
names and `-debug` variants use Debug; `-release` variants use RelWithDebInfo,
or MinSizeRel for Emscripten. `debug` and `release` select the host compiler.

```sh
just configure release
just build release
just test release
just configure macos-clang-release
just configure emscripten-release
```

Each build lives in `build/<preset>/`, including its `compile_commands.json`.
Clangd and `just tidy` use `build/default/compile_commands.json`. To index another
build, change `.clangd`'s `CompilationDatabase` to `build/<preset>`.

Names describe targets; host OS conditions control availability. For web builds,
`just configure emscripten` invokes `emcmake` on any supported desktop host.
Native tests are disabled for Emscripten.

`windows-mingw-x86_64` and `windows-mingw-arm64` work on Linux, macOS and Windows.
Their tests are enabled. On Linux/macOS, the x86_64 toolchain discovers Wine and
sets `CMAKE_CROSSCOMPILING_EMULATOR`, so `just test windows-mingw-x86_64` uses it.
The default runner disables Wine's graphical crash debugger; failures stay in CTest.
This also handles the scripted unit-runner check. Native Windows tests run directly.
ARM64 cross tests need an explicitly configured compatible runner; the toolchain
does not assume the installed Wine supports ARM64.

Architecture names describe the target; `x86_64` means 64-bit x86. Cross builds
require target SDKs/libraries; pass `-DCMAKE_SYSROOT=...` or
`-DCMAKE_FIND_ROOT_PATH=...` as needed. Compiler paths can be overridden with
`-DCMAKE_C_COMPILER=...`. Set `CMAKE_CROSSCOMPILING_EMULATOR` to override the runner
(a semicolon-separated command and arguments). ARM64 MinGW presets use LLVM-MinGW; Windows GCC ARM64
requires an ARM64-targeting GCC installation. MSVC presets require a Visual
Studio developer shell targeting the requested architecture.

macOS GCC configures, but current vendored Raylib/Cocoa sources use Apple Blocks
syntax unsupported by Homebrew GCC. Use either Clang preset for a complete build.
