# Copyright (c) 2026
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_ETHERCAT_HPMICRO ethercat_hpmicro.c)
