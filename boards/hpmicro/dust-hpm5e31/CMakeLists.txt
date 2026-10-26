# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources(board.c)

# OpenOCD accesses RTT through RISC-V System Bus Access while the CPU runs.
# Keep the control block and channel buffers in the SoC's non-cacheable RAM.
zephyr_linker_sources_ifdef(
  CONFIG_USE_SEGGER_RTT
  NOCACHE_SECTION
  rtt_nocache.ld
)
