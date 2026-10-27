# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ecat)

# _nocache_ram_size is an absolute linker symbol and may legitimately be 0
# when no DMA buffers use __nocache. GCC otherwise assumes its address is
# non-null and removes soc_init_pma()'s zero-size guard. Preserve that guard
# for this BSP source; a zero-size NAPOT region would cover AXI SRAM and
# change the cacheability of the live stack without cleaning dirty data.
if(CONFIG_SOC_HPM5E31 AND CONFIG_NOCACHE_MEMORY)
    set_property(SOURCE
        ${ZEPHYR_SDK_GLUE_MODULE_DIR}/soc/hpmicro/HPM5E00/soc.c
        TARGET_DIRECTORY zephyr APPEND PROPERTY COMPILE_OPTIONS
        -fno-delete-null-pointer-checks)
endif()

target_sources(app PRIVATE src/main.c)

add_subdirectory(Middleware/ESC)
add_subdirectory(EtherCAT)
