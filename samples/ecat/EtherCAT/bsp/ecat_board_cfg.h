/*
 * Copyright (c) 2025 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Dust HPM5E31 board EtherCAT configuration.
 */

#ifndef ECAT_BOARD_CFG_H
#define ECAT_BOARD_CFG_H

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include "hpm_soc.h"
#include "hpm_clock_drv.h"

#define BOARD_FLASH_BASE_ADDRESS (0x80000000UL)

/*
 * PHY population of the HPM5E31 board being built. Select it here, or with
 * build_ethercat.ps1 -PhyLayout, which sets the CMake cache variable ECAT_PHY_LAYOUT.
 */
#define BOARD_ECAT_PHY_LAYOUT_RTL8201F_TJA1101 1 /* Port0 RTL8201F, Port1 TJA1101B */
#define BOARD_ECAT_PHY_LAYOUT_DUAL_TJA1101     2 /* Port0 and Port1 both TJA1101 */

#ifndef BOARD_ECAT_PHY_LAYOUT
#define BOARD_ECAT_PHY_LAYOUT BOARD_ECAT_PHY_LAYOUT_RTL8201F_TJA1101
#endif

/* PHY driven by each ESC port; a new board only needs a new layout block below. */
#define BOARD_ECAT_PHY_NONE     0 /* Port not configured; its software link stays down */
#define BOARD_ECAT_PHY_RTL8201F 1
#define BOARD_ECAT_PHY_TJA1101  2

/* 100BASE-T1 role of a TJA1101 port; the two ends of a link must differ. */
#define BOARD_ECAT_T1_ROLE_STRAP  0 /* Keep the board strap */
#define BOARD_ECAT_T1_ROLE_MASTER 1
#define BOARD_ECAT_T1_ROLE_SLAVE  2

/* TJA1101 clock source. Both boards feed CLK_IN_OUT from ESC0_REFCK (PA09, 25 MHz). */
#define BOARD_ECAT_T1_CLK_XTAL    0 /* 25 MHz crystal on XI/XO */
#define BOARD_ECAT_T1_CLK_EXT_25M 1 /* External 25 MHz clock on CLK_IN_OUT */
#ifndef BOARD_ECAT_T1_CLK
#define BOARD_ECAT_T1_CLK BOARD_ECAT_T1_CLK_EXT_25M
#endif

#if BOARD_ECAT_PHY_LAYOUT == BOARD_ECAT_PHY_LAYOUT_RTL8201F_TJA1101
/* Dust board: Port0 RTL8201F at MDIO 1, Port1 TJA1101B strapped to MDIO 2. MDIO 0 is
 * a broadcast address both PHYs answer, so no PHY on this bus may use it.
 */
/*根据板子的实际PHY布局 上板配置:
#define BOARD_ECAT_PORT0_PHY      BOARD_ECAT_PHY_RTL8201F
#define BOARD_ECAT_PORT0_PHY_ADDR (1U)
#define BOARD_ECAT_PORT1_PHY      BOARD_ECAT_PHY_TJA1101
#define BOARD_ECAT_PORT1_PHY_ADDR (2U)
下板配置:
#define BOARD_ECAT_PORT0_PHY      BOARD_ECAT_PHY_TJA1101
#define BOARD_ECAT_PORT0_PHY_ADDR (2U)
#define BOARD_ECAT_PORT1_PHY      BOARD_ECAT_PHY_NONE
#define BOARD_ECAT_PORT1_PHY_ADDR (0U)*/
#define BOARD_ECAT_PORT0_PHY      BOARD_ECAT_PHY_TJA1101
#define BOARD_ECAT_PORT0_PHY_ADDR (2U)
#define BOARD_ECAT_PORT1_PHY      BOARD_ECAT_PHY_NONE
#define BOARD_ECAT_PORT1_PHY_ADDR (0U)
#elif BOARD_ECAT_PHY_LAYOUT == BOARD_ECAT_PHY_LAYOUT_DUAL_TJA1101
/* Unconfirmed straps. MDIO 0 is answered as broadcast (TJA1101 replied there on the Dust
 * board), so two TJA1101 need nonzero addresses; check the board before using these.
 */
#define BOARD_ECAT_PORT0_PHY      BOARD_ECAT_PHY_TJA1101
#define BOARD_ECAT_PORT0_PHY_ADDR (1U)
#define BOARD_ECAT_PORT1_PHY      BOARD_ECAT_PHY_TJA1101
#define BOARD_ECAT_PORT1_PHY_ADDR (0U)
#else
#error "BOARD_ECAT_PHY_LAYOUT must be BOARD_ECAT_PHY_LAYOUT_RTL8201F_TJA1101 or _DUAL_TJA1101"
#endif
#ifndef BOARD_ECAT_PORT0_T1_ROLE
#define BOARD_ECAT_PORT0_T1_ROLE BOARD_ECAT_T1_ROLE_STRAP
#endif
#ifndef BOARD_ECAT_PORT1_T1_ROLE
#define BOARD_ECAT_PORT1_T1_ROLE BOARD_ECAT_T1_ROLE_STRAP
#endif
#define BOARD_ECAT_SUPPORT_PORT1 (BOARD_ECAT_PORT1_PHY != BOARD_ECAT_PHY_NONE)
#define BOARD_ECAT_SUPPORT_PORT2 (0)

/* Share the driver's address offset instead of duplicating it in C. */
#define BOARD_ECAT_PHY_ADDR_OFFSET DT_PROP(DT_NODELABEL(esc0), phy_addr_offset)

/* Use the board's reserved EtherCAT EEPROM partition and flash offset. */
#define BOARD_ECAT_EEPROM_PARTITION ethercat_eeprom_partition
#define BOARD_ECAT_FLASH_EMULATE_EEPROM_ADDR \
    DT_REG_ADDR(DT_NODELABEL(BOARD_ECAT_EEPROM_PARTITION))

/* busy-wait delay, usable in ISR context */
static inline void board_delay_ms(uint32_t ms)
{
	k_busy_wait(ms * 1000);
}

#endif /* ECAT_BOARD_CFG_H */
