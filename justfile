set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-Command"]

configure:
    cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build:
    cmake --build build

check: tidy luals-check cargo-check

check-fix: tidy-fix luals-check cargo-check

tidy *args: configure build
    run-clang-tidy -quiet -p build 'sources/.*\.cpp$' 'sources/.*\.hpp$' {{ args }}

tidy-fix: (tidy "-fix")

luals-check:
    lua-language-server --check=. --configpath=.luarc.json --checklevel=Warning --logpath=build/lua-check

cargo-check:
    cargo check --workspace

fmt:
    #!/usr/bin/env sh
    set -euio pipefail

    find sources -iname *.hpp -o -iname *.cpp | xargs clang-format -i
    stylua examples

precommit: fmt check-fix

prepare:
    cargo run -p lua-defgen -- sources definitions

run-example name: configure
    cmake --build build --target {{ name }}
    {{ "." / "build" / "examples" / name }}

[unix]
test-example name: configure
    cmake --build build --target {{ name }}
    timeout 2 {{ "." / "build" / "examples" / name }}

[windows]
test-example name: configure
    cmake --build build --target {{ name }}
    $proc = Start-Process -FilePath "{{ "." / "build" / "examples" / name }}.exe" -PassThru
    Start-Sleep -Seconds 2
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

test-all-examples:
    -just test-example basic
    -just test-example 2d-atlas
    -just test-example ui-basic
