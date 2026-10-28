#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
python3 "$root/tests/native/extract_mcan_timing.py" "$root/drivers/can/mcan_hpmicro.c" "$build/mcan_timing.inc"
for enabled in 0 1; do
    defines=()
    if [[ "$enabled" == 1 ]]; then defines+=(-DCONFIG_HPM_MCAN_RX_TIMING=1); fi
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        "${defines[@]}" -I "$root/include" -I "$build" "$root/tests/native/mcan_timing.c" -o "$build/test"
    "$build/test"
done
echo 'Actual MCAN RX timing config/counter sampling/callback contracts PASS'
