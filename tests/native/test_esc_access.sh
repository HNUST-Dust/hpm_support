#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
python3 "$root/tests/native/extract_esc_access.py" "$build/esc_access_functions.inc"
for mode in byte word; do
    flags=()
    if [[ $mode == word ]]; then flags+=(-DCONFIG_HPM_ESC_PDRAM_WORD_ACCESS=1); fi
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
      -fno-omit-frame-pointer "${flags[@]}" -I "$build" "$root/tests/native/esc_access.c" -o "$build/$mode"
    "$build/$mode"
done
