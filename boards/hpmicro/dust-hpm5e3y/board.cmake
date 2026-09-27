# SPDX-License-Identifier: Apache-2.0

set(OPENOCD_CONFIG_RELATIVE ${ZEPHYR_BASE}/../../sdk_env/hpm_sdk/boards/openocd)
get_filename_component(OPENOCD_CONFIG_ABSOLUTE ${OPENOCD_CONFIG_RELATIVE} ABSOLUTE)
set(OPENOCD_CONFIG_DIR ${OPENOCD_CONFIG_ABSOLUTE} CACHE PATH "HPMicro OpenOCD scripts")

board_runner_args(openocd "--config=${OPENOCD_CONFIG_DIR}/probes/cmsis_dap.cfg"
	"--config=${OPENOCD_CONFIG_DIR}/soc/hpm5e00.cfg"
	"--config=${OPENOCD_CONFIG_DIR}/boards/hpm5e00evk.cfg"
	"--openocd-search=${OPENOCD_CONFIG_DIR}" --target-handle=hpm5e00.cpu0)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
