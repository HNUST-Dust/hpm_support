# SPDX-License-Identifier: Apache-2.0

set(OPENOCD_CONFIG_RELATIVE ${ZEPHYR_BASE}/../sdk_env/hpm_sdk/boards/openocd)
set(HPM_TOOLS_RELATIVE ${ZEPHYR_BASE}/../sdk_env/tools)
set(HPM_MACOS_OPENOCD_RELATIVE ${ZEPHYR_BASE}/../../tools/usr/local)
get_filename_component(OPENOCD_CONFIG_ABSOLUTE ${OPENOCD_CONFIG_RELATIVE} ABSOLUTE)
get_filename_component(HPM_TOOLS_ABSOLUTE ${HPM_TOOLS_RELATIVE} ABSOLUTE)
get_filename_component(HPM_MACOS_OPENOCD_ABSOLUTE ${HPM_MACOS_OPENOCD_RELATIVE} ABSOLUTE)
set(OPENOCD_CONFIG_DIR ${OPENOCD_CONFIG_ABSOLUTE} CACHE PATH "hpmicro openocd cfg root directory")
set(HPM_TOOLS_DIR ${HPM_TOOLS_ABSOLUTE} CACHE PATH "hpmicro win tools root directory")
# 使用 CMSIS-DAP 调试器
set(HPM_OPENOCD_PROBE "cmsis_dap" CACHE STRING "OpenOCD probe cfg name, e.g. cmsis_dap/ft2232/jlink")
# 1MHz速度
set(HPM_OPENOCD_ADAPTER_SPEED_KHZ "1000" CACHE STRING "OpenOCD adapter speed in kHz")

if(NOT CONFIG_XIP)
    board_runner_args(openocd "--use-elf")
endif()

if(${BOARD} STREQUAL "hpm6e00evk_v2")
    board_runner_args(openocd "--config=${OPENOCD_CONFIG_DIR}/probes/${HPM_OPENOCD_PROBE}.cfg"
                                    "--config=${OPENOCD_CONFIG_DIR}/soc/hpm6e80-single-core.cfg"
                                "--config=${OPENOCD_CONFIG_DIR}/boards/hpm6e00evk.cfg"
                                "--openocd-search=${OPENOCD_CONFIG_DIR}")

    board_runner_args(openocd "--cmd-pre-init=adapter speed ${HPM_OPENOCD_ADAPTER_SPEED_KHZ}")

    board_runner_args(openocd --target-handle=_CHIPNAME.cpu0)
else()
    message(FATAL_ERROR "${BOARD} is not supported now")
endif()

set(ENV_PATH $ENV{PATH})
unset(OPENOCD CACHE)
if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
    set(SEPARATOR ":")
    string(REPLACE ${SEPARATOR} ";" ZPATH ${ENV_PATH})
    find_program(OPENOCD openocd PATHS ${ZPATH} NO_DEFAULT_PATH)
elseif("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
    set(OPENOCD "${HPM_TOOLS_DIR}/openocd/openocd.exe" CACHE FILEPATH "" FORCE)
    set(OPENOCD_DEFAULT_PATH ${HPM_TOOLS_DIR}/openocd/tcl)
elseif("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
    if(EXISTS "${HPM_MACOS_OPENOCD_ABSOLUTE}/bin/openocd")
        set(OPENOCD "${HPM_MACOS_OPENOCD_ABSOLUTE}/bin/openocd" CACHE FILEPATH "" FORCE)
        set(OPENOCD_DEFAULT_PATH "${HPM_MACOS_OPENOCD_ABSOLUTE}/share/openocd/scripts")
    elseif(EXISTS "${HPM_TOOLS_DIR}/openocd/bin/openocd")
        set(OPENOCD "${HPM_TOOLS_DIR}/openocd/bin/openocd" CACHE FILEPATH "" FORCE)
        set(OPENOCD_DEFAULT_PATH "${HPM_TOOLS_DIR}/openocd/tcl")
    else()
        message(WARNING "Darwin openocd not found. Expected ${HPM_MACOS_OPENOCD_ABSOLUTE}/bin/openocd or ${HPM_TOOLS_DIR}/openocd/bin/openocd")
    endif()
else()
    message(WARNING "${CMAKE_HOST_SYSTEM_NAME} openocd is not support")
endif()
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
