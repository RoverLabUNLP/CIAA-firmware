#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
ciaa_sdk_dir="${1:-$HOME/CIAA_Software_1.1-linux-x64}"
ciaa_firmware_dir="$ciaa_sdk_dir/ide4plc/shared/firmware_v3"
if [[ ! -f "$ciaa_firmware_dir/Makefile" || ! -x "$ciaa_sdk_dir/tools/gcc-arm-embedded/bin/arm-none-eabi-gcc" ]]; then
    printf 'No se encontró CIAA Software completo en %s. Indicá CIAA_SDK_DIR al ejecutar make.\n' "$ciaa_sdk_dir" >&2
    exit 1
fi
for ciaa_resource in libs scripts; do
    ciaa_target="$ciaa_firmware_dir/$ciaa_resource"
    if [[ -L "$ciaa_resource" && "$(readlink "$ciaa_resource")" == "$ciaa_target" ]]; then
        continue
    fi
    if [[ -e "$ciaa_resource" || -L "$ciaa_resource" ]]; then
        printf 'El enlace %s ya existe con otro destino. Revisalo antes de cambiar CIAA_SDK_DIR.\n' "$ciaa_resource" >&2
        exit 1
    fi
    ln -s "$ciaa_target" "$ciaa_resource"
done
