set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-Command"]

# Each preset writes compile_commands.json to build/<preset>/.
configure preset='default':
    {{ if preset =~ '^emscripten(-(debug|release))?$' { 'emcmake cmake -G Ninja' } else { 'cmake' } }} --preset "{{ preset }}"

build preset='default':
    cmake --build --preset "{{ preset }}"

test preset='default': (build preset)
    ctest --preset "{{ preset }}"

check: tidy luals-check cargo-check

check-fix: tidy-fix luals-check cargo-check

tidy *args: configure build
    run-clang-tidy -quiet -p build/default 'sources/.*\.c$' 'sources/.*\.h$' {{ args }}

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
    cmake --build --preset default --target {{ name }}
    {{ "." / "build" / "default" / "examples" / name }}

[unix]
test-example name: configure
    cmake --build --preset default --target {{ name }}
    timeout 2 {{ "." / "build" / "default" / "examples" / name }}

[windows]
test-example name: configure
    cmake --build --preset default --target {{ name }}
    $proc = Start-Process -FilePath "{{ "." / "build" / "default" / "examples" / name }}.exe" -PassThru
    Start-Sleep -Seconds 2
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

test-all-examples:
    -just test-example glint_basic
    -just test-example glint_atlas
