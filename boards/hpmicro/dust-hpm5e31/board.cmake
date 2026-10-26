# SPDX-License-Identifier: Apache-2.0

set(OPENOCD_CONFIG_RELATIVE ${ZEPHYR_BASE}/../../sdk_env/hpm_sdk/boards/openocd)
set(HPM_MACOS_OPENOCD_RELATIVE ${ZEPHYR_BASE}/../../usr/local)
set(HPM_RTT_OPENOCD_RELATIVE ${ZEPHYR_BASE}/../../openocd-hpm-rtt-v0.1.0/local-rtt)
get_filename_component(OPENOCD_CONFIG_ABSOLUTE ${OPENOCD_CONFIG_RELATIVE} ABSOLUTE)
get_filename_component(HPM_MACOS_OPENOCD_ABSOLUTE ${HPM_MACOS_OPENOCD_RELATIVE} ABSOLUTE)
get_filename_component(HPM_RTT_OPENOCD_ABSOLUTE ${HPM_RTT_OPENOCD_RELATIVE} ABSOLUTE)
set(OPENOCD_CONFIG_DIR ${OPENOCD_CONFIG_ABSOLUTE} CACHE PATH "HPMicro OpenOCD scripts")

# The upstream OpenOCD build does not contain HPMicro's hpm_xpi flash driver.
# Prefer the HPMicro builds on macOS, matching the other custom HPM boards.
if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
    if(EXISTS "${HPM_RTT_OPENOCD_ABSOLUTE}/bin/openocd")
        set(OPENOCD "${HPM_RTT_OPENOCD_ABSOLUTE}/bin/openocd" CACHE FILEPATH "" FORCE)
        set(OPENOCD_DEFAULT_PATH "${HPM_RTT_OPENOCD_ABSOLUTE}/share/openocd/scripts")
    elseif(EXISTS "${HPM_MACOS_OPENOCD_ABSOLUTE}/bin/openocd")
        set(OPENOCD "${HPM_MACOS_OPENOCD_ABSOLUTE}/bin/openocd" CACHE FILEPATH "" FORCE)
        set(OPENOCD_DEFAULT_PATH "${HPM_MACOS_OPENOCD_ABSOLUTE}/share/openocd/scripts")
    else()
        message(WARNING "HPMicro OpenOCD not found; the system OpenOCD may not support the hpm_xpi flash driver")
    endif()
endif()

board_runner_args(openocd "--config=${OPENOCD_CONFIG_DIR}/probes/cmsis_dap.cfg"
	"--config=${OPENOCD_CONFIG_DIR}/soc/hpm5e00.cfg"
	"--config=${OPENOCD_CONFIG_DIR}/boards/hpm5e00evk.cfg"
	"--openocd-search=${OPENOCD_CONFIG_DIR}" --target-handle=hpm5e00.cpu0)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
