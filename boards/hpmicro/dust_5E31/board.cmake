# SPDX-License-Identifier: Apache-2.0

get_filename_component(OPENOCD_CONFIG_DIR
    "${ZEPHYR_BASE}/../sdk_env/hpm_sdk/boards/openocd" ABSOLUTE)
get_filename_component(HPM_TOOLS_DIR
    "${ZEPHYR_BASE}/../sdk_env/tools" ABSOLUTE)

set(HPM5E31_OPENOCD_PROBE "cmsis_dap" CACHE STRING
    "HPM5E31 JTAG probe script basename")
set(_hpm_probe_cfg "${OPENOCD_CONFIG_DIR}/probes/${HPM5E31_OPENOCD_PROBE}.cfg")
if(NOT EXISTS "${_hpm_probe_cfg}")
    message(FATAL_ERROR "OpenOCD probe script not found: ${_hpm_probe_cfg}")
endif()

board_runner_args(openocd
    "--config=${_hpm_probe_cfg}"
    "--config=${OPENOCD_CONFIG_DIR}/soc/hpm5e00.cfg"
    "--config=${CMAKE_CURRENT_LIST_DIR}/support/openocd.cfg"
    "--openocd-search=${OPENOCD_CONFIG_DIR}"
    "--target-handle=hpm5e00.cpu0")

if(CONFIG_XIP)
    if(CMAKE_HOST_WIN32)
        # The generic OpenOCD runner resets before a separate verify_image,
        # which leaves this board's XPI mapping unreadable. Use the proven
        # single-command ELF program/verify/reset sequence for west flash.
        board_set_flasher_ifnset(misc-flasher)
    endif()
else()
    board_runner_args(openocd "--use-elf")
    set(OPENOCD_USE_LOAD_IMAGE YES)
endif()
if(CMAKE_HOST_WIN32)
    set(OPENOCD "${HPM_TOOLS_DIR}/openocd/openocd.exe" CACHE FILEPATH
        "HPMicro OpenOCD executable" FORCE)
    set(OPENOCD_DEFAULT_PATH "${HPM_TOOLS_DIR}/openocd/tcl")
endif()

include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)

if(CONFIG_XIP AND CMAKE_HOST_WIN32)
    board_finalize_runner_args(misc-flasher
        "${CMAKE_CURRENT_LIST_DIR}/support/flash.cmd")
endif()
