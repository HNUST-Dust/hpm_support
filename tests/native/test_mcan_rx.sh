#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
python3 "$root/tests/native/extract_mcan_rx.py" "$root/drivers/can/mcan_hpmicro.c" "$build/mcan_rx.inc"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I "$build" "$root/tests/native/mcan_rx.c" -o "$build/test"
"$build/test"
echo 'Actual MCAN masks/matching/FIFO drain/arrival-after-ack/read-failure contracts PASS'
