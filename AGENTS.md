Follow [STYLEGUIDE.md](STYLEGUIDE.md) when writing C code.

## Target platforms

Consider these targets when making platform and ABI decisions.

### Tier 1: Working toward support now

- Linux: x86_64 and arm64
- macOS: arm64
- Windows: x86_64 and arm64
- Android: arm64
- Emscripten: wasm32

### Tier 2: Planned in the near future

- iOS/iPadOS: arm64

### Tier 3: Planned in the distant future

- PS4/PS5: x86_64
- Xbox Series S/X: x86_64
- Nintendo Switch: arm64

## Build and test commands

Use `just` from the repository root for configuring, building and testing.
Use these recipes instead of invoking CMake or CTest directly for these tasks.
Configure first, then build and test with the same preset name:

```sh
just configure
just build
just test

just configure release
just build release
just test release
```

All three recipes accept an optional preset, defaulting to `default`.
Unsuffixed names and `-debug` use Debug. `-release` uses RelWithDebInfo,
except Emscripten, which uses MinSizeRel. `debug` and `release` use the host compiler.
Each preset writes to `build/<preset>/`, including `compile_commands.json`.
Clangd and `just tidy` use `build/default/`.

Preset names describe the target, not the host. Conditions gate compatible hosts.
Linux and macOS native presets require that host OS. Windows MSVC/GCC presets
require Windows. Windows MinGW and Emscripten presets work on Linux, macOS and
Windows with the matching toolchain installed. These lists cover Tier 1 except Android.
See [cmake/README.md](cmake/README.md) for SDK requirements and known limitations.

### Host defaults

| Configure | Build |
| --- | --- |
| `just configure default` | `just build default` |
| `just configure default-debug` | `just build default-debug` |
| `just configure default-release` | `just build default-release` |
| `just configure debug` | `just build debug` |
| `just configure release` | `just build release` |

### Linux targets

| Configure | Build |
| --- | --- |
| `just configure linux-gcc-x86_64` | `just build linux-gcc-x86_64` |
| `just configure linux-gcc-x86_64-debug` | `just build linux-gcc-x86_64-debug` |
| `just configure linux-gcc-x86_64-release` | `just build linux-gcc-x86_64-release` |
| `just configure linux-gcc-arm64` | `just build linux-gcc-arm64` |
| `just configure linux-gcc-arm64-debug` | `just build linux-gcc-arm64-debug` |
| `just configure linux-gcc-arm64-release` | `just build linux-gcc-arm64-release` |
| `just configure linux-tcc-x86_64` | `just build linux-tcc-x86_64` |
| `just configure linux-tcc-x86_64-debug` | `just build linux-tcc-x86_64-debug` |
| `just configure linux-tcc-x86_64-release` | `just build linux-tcc-x86_64-release` |
| `just configure linux-tcc-arm64` | `just build linux-tcc-arm64` |
| `just configure linux-tcc-arm64-debug` | `just build linux-tcc-arm64-debug` |
| `just configure linux-tcc-arm64-release` | `just build linux-tcc-arm64-release` |
| `just configure linux-clang-x86_64` | `just build linux-clang-x86_64` |
| `just configure linux-clang-x86_64-debug` | `just build linux-clang-x86_64-debug` |
| `just configure linux-clang-x86_64-release` | `just build linux-clang-x86_64-release` |
| `just configure linux-clang-arm64` | `just build linux-clang-arm64` |
| `just configure linux-clang-arm64-debug` | `just build linux-clang-arm64-debug` |
| `just configure linux-clang-arm64-release` | `just build linux-clang-arm64-release` |

### macOS targets

| Configure | Build |
| --- | --- |
| `just configure macos-gcc` | `just build macos-gcc` |
| `just configure macos-gcc-debug` | `just build macos-gcc-debug` |
| `just configure macos-gcc-release` | `just build macos-gcc-release` |
| `just configure macos-homebrew-clang` | `just build macos-homebrew-clang` |
| `just configure macos-homebrew-clang-debug` | `just build macos-homebrew-clang-debug` |
| `just configure macos-homebrew-clang-release` | `just build macos-homebrew-clang-release` |
| `just configure macos-clang` | `just build macos-clang` |
| `just configure macos-clang-debug` | `just build macos-clang-debug` |
| `just configure macos-clang-release` | `just build macos-clang-release` |

### Windows targets

MSVC requires the matching Visual Studio developer shell. MinGW presets select
the Windows target on any supported desktop host; x86_64 cross tests use Wine.

| Configure | Build |
| --- | --- |
| `just configure windows-mingw-x86_64` | `just build windows-mingw-x86_64` |
| `just configure windows-mingw-x86_64-debug` | `just build windows-mingw-x86_64-debug` |
| `just configure windows-mingw-x86_64-release` | `just build windows-mingw-x86_64-release` |
| `just configure windows-mingw-arm64` | `just build windows-mingw-arm64` |
| `just configure windows-mingw-arm64-debug` | `just build windows-mingw-arm64-debug` |
| `just configure windows-mingw-arm64-release` | `just build windows-mingw-arm64-release` |
| `just configure windows-msvc-x86_64` | `just build windows-msvc-x86_64` |
| `just configure windows-msvc-x86_64-debug` | `just build windows-msvc-x86_64-debug` |
| `just configure windows-msvc-x86_64-release` | `just build windows-msvc-x86_64-release` |
| `just configure windows-gcc-x86_64` | `just build windows-gcc-x86_64` |
| `just configure windows-gcc-x86_64-debug` | `just build windows-gcc-x86_64-debug` |
| `just configure windows-gcc-x86_64-release` | `just build windows-gcc-x86_64-release` |
| `just configure windows-msvc-arm64` | `just build windows-msvc-arm64` |
| `just configure windows-msvc-arm64-debug` | `just build windows-msvc-arm64-debug` |
| `just configure windows-msvc-arm64-release` | `just build windows-msvc-arm64-release` |
| `just configure windows-gcc-arm64` | `just build windows-gcc-arm64` |
| `just configure windows-gcc-arm64-debug` | `just build windows-gcc-arm64-debug` |
| `just configure windows-gcc-arm64-release` | `just build windows-gcc-arm64-release` |

### Emscripten wasm32

The configure recipe invokes `emcmake` automatically. Native tests are disabled.

| Configure | Build |
| --- | --- |
| `just configure emscripten` | `just build emscripten` |
| `just configure emscripten-debug` | `just build emscripten-debug` |
| `just configure emscripten-release` | `just build emscripten-release` |

### Tests and other checks

Use `just test <preset>` after building. Every non-Emscripten preset above has
a matching test preset, including `-debug` and `-release` variants. MinGW
x86_64 cross builds discover Wine automatically on Linux and macOS, with its
graphical crash debugger disabled. Start a persistent server once with
`wineserver -p` in the same `WINEPREFIX` used by the tests. If a persistent
server is already running for that prefix, run `just test` for the preset
directly. On the development Mac, the x86_64 suite takes about 4 seconds
with a persistent server and has taken about 140 seconds with repeated Wine
startup between CTest processes.

ARM64 cross tests require an explicitly configured compatible runner.
Native Windows tests run directly.

```sh
just test default
just test debug
just test release
just test macos-clang-debug
just test linux-gcc-arm64-release
just test windows-msvc-x86_64-debug

just configure windows-mingw-x86_64
just build windows-mingw-x86_64
wineserver -p
just test windows-mingw-x86_64
```

Use `just fmt` for formatting and `just check` for static checks.
Do not run graphical example recipes during automated checks; they open real windows.
