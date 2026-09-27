# Copyright (c) 2026
# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(.)
zephyr_sources(start.S soc.c)
set(SOC_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld CACHE INTERNAL "")
