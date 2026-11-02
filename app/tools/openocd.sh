#!/usr/bin/env bash
# CIAA Software 1.1: evita mezclar su librt antigua con la glibc de Ubuntu actual.
set -euo pipefail
ciaa_sdk_dir="${CIAA_SDK_DIR:-$HOME/CIAA_Software_1.1-linux-x64}"
ciaa_openocd_dir="$ciaa_sdk_dir/tools/openocd/bin"
ciaa_loader=/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2

if [[ ! -x "$ciaa_loader" || ! -x "$ciaa_openocd_dir/openocd" ]]; then
    printf 'No se encontro el cargador Linux x86-64 o el OpenOCD del SDK en %s\n' "$ciaa_openocd_dir" >&2
    exit 1
fi

# El ejecutable tiene RPATH=$ORIGIN. Se ignora para priorizar las bibliotecas
# del sistema; el SDK queda al final para dependencias antiguas como libusb-0.1.
exec "$ciaa_loader" --inhibit-rpath '' \
    --library-path "/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu:$ciaa_openocd_dir" \
    "$ciaa_openocd_dir/openocd" "$@"
