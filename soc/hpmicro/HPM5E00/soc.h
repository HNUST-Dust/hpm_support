/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 */

#ifndef ZEPHYR_SOC_RISCV_HPMICRO_HPM5E00_SOC_H_
#define ZEPHYR_SOC_RISCV_HPMICRO_HPM5E00_SOC_H_

#include <soc_common.h>
#include <zephyr/devicetree.h>

#define RISCV_MTIME_BASE DT_REG_ADDR_BY_IDX(DT_NODELABEL(mtimer), 0)
#define RISCV_MTIMECMP_BASE DT_REG_ADDR_BY_IDX(DT_NODELABEL(mtimer), 1)

#endif
