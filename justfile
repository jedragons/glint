set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-Command"]

configure:
    cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

check: configure
    cmake --build build
    cargo check --workspace

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
