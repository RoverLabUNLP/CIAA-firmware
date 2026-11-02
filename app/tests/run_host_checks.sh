#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_build_dir=$(mktemp -d)
trap 'rm -rf "$test_build_dir"' EXIT
cc -std=c99 -Wall -Wextra -Werror \
  -Iapp/tests/stubs -Iapp/inc -Idrivers/inc \
  app/tests/integration_checks.c app/src/app_communication.c \
  app/src/app_imu.c app/src/app_hardware.c app/src/vehicle_state.c \
  -lm -o "$test_build_dir/integration_checks"
"$test_build_dir/integration_checks"
cc -std=c99 -Wall -Wextra -Werror \
  -Iapp/tests/stubs -Iapp/inc -Idrivers/inc \
  app/tests/startup_checks.c -o "$test_build_dir/startup_checks"
"$test_build_dir/startup_checks"
cc -std=c99 -Wall -Wextra -Werror \
  -Iapp/tests/stubs -Iapp/inc -Idrivers/inc \
  app/tests/debug_checks.c app/src/app_debug.c -o "$test_build_dir/debug_checks"
"$test_build_dir/debug_checks"
