set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-Command"]

configure:
    cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DGLINT_BUILD_C_TESTS=ON

build:
    cmake --build build

check: tidy luals-check cargo-check

check-fix: tidy-fix luals-check cargo-check

tidy *args: configure build
    run-clang-tidy -quiet -p build 'sources/.*\.c$' 'sources/.*\.h$' {{ args }}

tidy-fix: (tidy "-fix")

luals-check:
    lua-language-server --check=. --configpath=.luarc.json --checklevel=Warning --logpath=build/lua-check

cargo-check:
    cargo check --workspace

fmt:
    #!/usr/bin/env sh
    set -euio pipefail

    find sources \( -iname '*.c' -o -iname '*.h' \) -exec clang-format -i {} +
    stylua examples

precommit: fmt check-fix

run-example name: configure
    cmake --build build --target {{ name }}
    {{ "." / "build" / "sources" / name }}

[unix]
test-example name: configure
    cmake --build build --target {{ name }}
    timeout 2 {{ "." / "build" / "sources" / name }}

[windows]
test-example name: configure
    cmake --build build --target {{ name }}
    $proc = Start-Process -FilePath "{{ "." / "build" / "sources" / name }}.exe" -PassThru
    Start-Sleep -Seconds 2
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

test-all-examples:
    -just test-example glint_c_basic
    -just test-example glint_c_atlas
