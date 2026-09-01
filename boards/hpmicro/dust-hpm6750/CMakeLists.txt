# Copyright (c) 2023 HPMicro
# SPDX-License-Identifier: BSD-3-Clause

zephyr_library()
zephyr_library_sources(board.c)

# OpenOCD's RISC-V System Bus Access reads SRAM behind the CPU cache. Keep the
# SEGGER RTT control block and its default buffers in the nocache region so the
# running core and debugger observe the same offsets and payload bytes.
zephyr_linker_sources_ifdef(
  CONFIG_WBR_CONTROL_RTT_DIAGNOSTICS
  NOCACHE_SECTION
  rtt_nocache.ld
)
