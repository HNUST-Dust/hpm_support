# SPDX-License-Identifier: Apache-2.0
zephyr_library()
zephyr_library_sources_ifdef(CONFIG_HPM_ESC esc_hpmicro.c)
zephyr_library_sources_ifdef(CONFIG_HPM_ESC ${HPM_SDK_DIR}/drivers/src/hpm_esc_drv.c)
