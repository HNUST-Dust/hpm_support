# Copyright (c) 2026 HPMicro
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_HPM_PLIC intc_hpm_plic.c)
zephyr_library_include_directories(${ZEPHYR_BASE}/arch/common/include)
