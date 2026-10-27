/*
 * Copyright (c) 2025 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * HPM5E31 custom board EtherCAT configuration.
 */

#ifndef ECAT_BOARD_CFG_H
#define ECAT_BOARD_CFG_H

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include "hpm_soc.h"
#include "hpm_clock_drv.h"

#define BOARD_FLASH_BASE_ADDRESS (0x80000000UL)

/* image.png specifies Port0 only. */
#define BOARD_ECAT_SUPPORT_PORT1 (0)
#define BOARD_ECAT_SUPPORT_PORT2 (0)

/* Share the driver's address offset instead of duplicating it in C. */
#define BOARD_ECAT_PHY_ADDR_OFFSET DT_PROP(DT_NODELABEL(esc0), phy_addr_offset)
#define BOARD_ECAT_PORT0_PHY_ADDR  (0U)

/* the address of ecat flash emulate eeprom component in flash */
#define BOARD_ECAT_FLASH_EMULATE_EEPROM_ADDR (0xF0000U)

/* busy-wait delay, usable in ISR context */
static inline void board_delay_ms(uint32_t ms)
{
	k_busy_wait(ms * 1000);
}

#endif /* ECAT_BOARD_CFG_H */
