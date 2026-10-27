#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""原生测试直接编译驱动函数体；仅替换 RISC-V MMIO fence 为宿主内存屏障。"""
from pathlib import Path
import re
import sys
root = Path(__file__).resolve().parents[2]
source = (root / "drivers/serial/uart_hpmicro.c").read_text()
names = ["uart_hpm_native_idle_stop", "async_user_callback", "async_evt_rx_rdy",
         "uart_hpm_async_rx_flush", "uart_hpm_rx_disable", "uart_hpm_rx_line_idle_flush",
         "uart_hpm_native_idle_failed", "uart_hpm_isr"]
parts = []
for name in names:
    match = re.search(r"^(?:__attribute__\(\(section\(\"\.isr\"\)\)\)\s*)?(?:static\s+)?(?:void|int)\s+" + name + r"\([^;]*?\)\s*\{", source, re.M)
    if not match:
        raise RuntimeError(f"Driver function not found: {name}")
    start = source.index("{", match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    body = source[match.start():end]
    body = body.replace('__asm__ volatile ("fence iorw, iorw" ::: "memory");', 'atomic_thread_fence(memory_order_seq_cst);')
    body = body.replace('__asm__ volatile("fence iorw, iorw" ::: "memory");', 'atomic_thread_fence(memory_order_seq_cst);')
    parts.append(body)
Path(sys.argv[1]).write_text("\n\n".join(parts) + "\n")
