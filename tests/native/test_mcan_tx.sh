#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer "$root/tests/native/mcan_tx_state.c" -o "$build/mcan_tx"
"$build/mcan_tx"
python3 "$root/tests/native/extract_mcan_submit.py" "$root/drivers/can/mcan_hpmicro.c" "$build/mcan_submit.inc"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I "$build" "$root/tests/native/mcan_admission.c" -o "$build/mcan_admission"
"$build/mcan_admission"
