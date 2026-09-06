# Copyright (c) 2024-2025 HPMicro
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(common)
add_subdirectory(${SOC_SERIES})

zephyr_linker_sources_ifdef(CONFIG_HPM_BOOT_HEADER ROM_START SORT_KEY $ common/boot_header.ld)
zephyr_linker_sources_ifdef(CONFIG_HPM_BOOT_HEADER ROM_START SORT_KEY + common/app_load_offset.ld)
zephyr_linker_sources_ifdef(CONFIG_HPM_BOOT_HEADER ROM_START SORT_KEY - common/entry.ld)

zephyr_linker_sources_ifdef(CONFIG_BOOTLOADER_MCUBOOT ROM_START SORT_KEY ! $ENV{ZEPHYR_BASE}/arch/common/rom_start_address.ld)
zephyr_linker_sources_ifdef(CONFIG_BOOTLOADER_MCUBOOT ROM_START SORT_KEY $ $ENV{ZEPHYR_BASE}/arch/common/fill_with_zeros.ld)
zephyr_linker_sources_ifdef(CONFIG_BOOTLOADER_MCUBOOT ROM_START SORT_KEY + $ENV{ZEPHYR_BASE}/arch/common/rom_start_offset.ld)
zephyr_linker_sources_ifdef(CONFIG_BOOTLOADER_MCUBOOT ROM_START SORT_KEY - common/entry.ld)
zephyr_linker_sources(ITCM_SECTION SORT_KEY 0 common/ISR.ld)
# This snippet pads and aligns the completed nocache region for PMA NAPOT.
# Keep it after every content-producing NOCACHE_SECTION snippet.
zephyr_linker_sources_ifdef(CONFIG_NOCACHE_MEMORY NOCACHE_SECTION SORT_KEY zzz common/nocache_pma_align.ld)
