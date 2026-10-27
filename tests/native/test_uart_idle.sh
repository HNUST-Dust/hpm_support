#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer "$root/tests/native/uart_idle_policy.c" -o "$build/uart_idle"
"$build/uart_idle"
# 使用当前驱动的真实函数体；只模拟寄存器、DMA 和内核 IRQ 边界。
python3 "$root/tests/native/extract_uart_idle.py" "$build/uart_idle_functions.inc"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I "$build" "$root/tests/native/uart_idle_driver.c" -o "$build/uart_driver"
"$build/uart_driver"
