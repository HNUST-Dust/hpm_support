/* SPDX-License-Identifier: BSD-3-Clause */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/ethercat/hpm_esc.h>
#include "ecat_board_cfg.h"
#include "hpm5e31_phy.h"
#include "hpm_esc_drv.h"

/* Standard MDIO (Clause 22) register addresses. */
#define PHY_BMCR_REG       0U
#define PHY_BMSR_REG       1U
#define PHY_ID1_REG        2U
#define PHY_ID2_REG        3U
#define PHY_ANAR_REG       4U
#define PHY_PAGE_REG       31U

#define PHY_BMCR_RESET     BIT(15)
#define PHY_LINK_STATUS    BIT(2)
#define PHY_AN_RESTART     BIT(9)
#define PHY_100_FULL_AN    (BIT(13) | BIT(12) | BIT(8))

/* RTL8201F (Port0) identifier, revision nibble masked. */
#define RTL8201F_ID        0x001cc810U
/* NXP 100BASE-T1 PHY family OUI MSB (TJA1101/TJA1101B). */
#define TJA1101_ID1        0x001BU

static bool port0_configured;
static bool port1_configured;

/* SHOW bypasses the ESC logical-port address translation during PDI access,
 * so the raw MDIO address below is exactly the physical strap address.
 */
static uint8_t physical_addr(uint8_t addr)
{
	return (uint8_t)(ESC_PHY_ADDR_SHOW_MASK | addr);
}

static int phy_read(const struct device *dev, uint8_t addr, uint8_t reg, uint16_t *value)
{
	return hpm_esc_mdio_read(dev, physical_addr(addr), reg, value);
}

static int phy_write(const struct device *dev, uint8_t addr, uint8_t reg, uint16_t value)
{
	return hpm_esc_mdio_write(dev, physical_addr(addr), reg, value);
}

static int phy_modify(const struct device *dev, uint8_t addr, uint8_t reg,
		      uint16_t clear, uint16_t set)
{
	uint16_t value;
	int ret = phy_read(dev, addr, reg, &value);

	if (ret != 0 || value == 0xffffU) {
		return -EIO;
	}
	return phy_write(dev, addr, reg, (uint16_t)((value & ~clear) | set));
}

static int phy_soft_reset(const struct device *dev, uint8_t addr, const char *name)
{
	uint16_t value = 0U;

	if (phy_write(dev, addr, PHY_BMCR_REG, PHY_BMCR_RESET) != 0) {
		return -EIO;
	}
	for (unsigned int retry = 0U; retry < 100U; ++retry) {
		k_msleep(1);
		if (phy_read(dev, addr, PHY_BMCR_REG, &value) != 0 || value == 0xffffU) {
			return -EIO;
		}
		if ((value & PHY_BMCR_RESET) == 0U) {
			return 0;
		}
	}
	printk("%s: software reset timeout (BMCR=0x%04x)\n", name, value);
	return -ETIMEDOUT;
}

/* The optional phy-reset-gpios (PF28) drives TJA1101B RST_N only; the RTL8201F
 * PHYRSTB pin is strapped with a pull-up and has no MCU reset control, so Port0
 * is brought up via BMCR software reset instead.
 */
void ecat_phy_reset(const struct device *esc_dev)
{
	if (!DT_NODE_HAS_PROP(DT_NODELABEL(esc0), phy_reset_gpios)) {
		printk("ESC: no PHY reset GPIO configured; PHYs rely on hardware/software reset\n");
	}
	hpm_esc_phy_reset(esc_dev, true);
	k_msleep(10);
	hpm_esc_phy_reset(esc_dev, false);
	/* RTL8201F datasheet rev1.4 sections 8.7 / 9.1.3: MDIO after 150ms. */
	k_msleep(150);
}

void ecat_phy_keep_reset(const struct device *esc_dev)
{
	hpm_esc_phy_reset(esc_dev, true);
}

static hpm_stat_t rtl8201f_config(const struct device *esc_dev)
{
	uint8_t addr = BOARD_ECAT_PORT0_PHY_ADDR;
	uint16_t id1 = 0U, id2 = 0U, value = 0U;

	if (phy_read(esc_dev, addr, PHY_ID1_REG, &id1) != 0 ||
	    phy_read(esc_dev, addr, PHY_ID2_REG, &id2) != 0) {
		printk("ESC Port0: no PHY at MDIO %u\n", addr);
		return status_fail;
	}
	uint32_t id = ((uint32_t)id1 << 16) | id2;

	printk("ESC Port0: PHY ID=0x%08x at MDIO %u\n", id, addr);
	if ((id & 0xfffffff0U) != RTL8201F_ID) {
		printk("ESC Port0: unexpected PHY ID 0x%08x (expected RTL8201F)\n", id);
		return status_fail;
	}

	if (phy_write(esc_dev, addr, PHY_PAGE_REG, 0U) != 0 ||
	    phy_soft_reset(esc_dev, addr, "Port0") != 0) {
		printk("ESC Port0: software reset failed\n");
		(void)phy_write(esc_dev, addr, PHY_PAGE_REG, 0U);
		return status_fail;
	}

	/* R60 straps RXDV low for MII; enforce MII without copying the datasheet's
	 * RMII external-clock example 0x7ffb (which sets bit3).
	 */
	if (phy_write(esc_dev, addr, PHY_PAGE_REG, 7U) != 0 ||
	    phy_modify(esc_dev, addr, 16U, BIT(3), 0U) != 0 ||
	    phy_write(esc_dev, addr, PHY_PAGE_REG, 4U) != 0 ||
	    /* Disable EEE capabilities for a continuous EtherCAT MII clock. */
	    phy_modify(esc_dev, addr, 16U, BIT(13) | BIT(12), 0U) != 0 ||
	    phy_write(esc_dev, addr, PHY_PAGE_REG, 0U) != 0 ||
	    /* Disable link-down power saving; retain reserved bits. */
	    phy_modify(esc_dev, addr, 24U, BIT(15), 0U) != 0 ||
	    /* Advertise only 100BASE-TX full duplex, with auto-negotiation. */
	    phy_write(esc_dev, addr, PHY_ANAR_REG, BIT(8) | BIT(0)) != 0) {
		printk("ESC Port0: RTL8201F MII/EEE/AN register sequence failed\n");
		(void)phy_write(esc_dev, addr, PHY_PAGE_REG, 0U);
		return status_fail;
	}
	if (phy_read(esc_dev, addr, PHY_ANAR_REG, &value) != 0 || value != 0x0101U) {
		printk("ESC Port0: ANAR readback 0x%04x (expected 0x0101), continuing\n",
		       value);
	}
	if (phy_write(esc_dev, addr, PHY_BMCR_REG, PHY_100_FULL_AN | PHY_AN_RESTART) != 0) {
		printk("ESC Port0: BMCR AN-restart write failed\n");
		return status_fail;
	}
	port0_configured = true;
	hpm_esc_set_link_state(esc_dev, 0, false);
	printk("ESC Port0: RTL8201F MII, advertise 100/full only, auto-negotiation enabled\n");
	return status_success;
}

static hpm_stat_t tja1101b_config(const struct device *esc_dev)
{
	uint8_t addr = BOARD_ECAT_PORT1_PHY_ADDR;
	uint16_t id1 = 0U, id2 = 0U;

	if (phy_read(esc_dev, addr, PHY_ID1_REG, &id1) != 0 ||
	    phy_read(esc_dev, addr, PHY_ID2_REG, &id2) != 0 ||
	    id1 == 0xffffU) {
		printk("ESC Port1: no PHY at MDIO %u\n", addr);
		return status_fail;
	}
	printk("ESC Port1: PHY ID=0x%04x%04x at MDIO %u\n", id1, id2, addr);
	if (id1 != TJA1101_ID1) {
		printk("ESC Port1: unexpected PHY ID1 0x%04x (expected 0x%04x)\n",
		       id1, TJA1101_ID1);
		return status_fail;
	}

	if (phy_soft_reset(esc_dev, addr, "Port1") != 0) {
		printk("ESC Port1: TJA1101B software reset failed\n");
		return status_fail;
	}

	/* TJA1101B is 100BASE-T1 (IEEE 802.3bw). After reset it auto-negotiates
	 * master/slave with the link partner, and its host interface defaults to
	 * MII, matching the ESC MII port. Extended-register configuration (MII/RMII
	 * mode, master/slave override, cable test, ...) is reached indirectly via
	 * REGCR (0x0D) / ADDAR (0x0E). Those values are left at reset defaults here
	 * and must be verified against NXP UM11897 before production; add any
	 * required overrides below for the final board.
	 */
	port1_configured = true;
	hpm_esc_set_link_state(esc_dev, 1, false);
	printk("ESC Port1: TJA1101B reset, default MII/AN (extended-register config pending)\n");
	return status_success;
}

hpm_stat_t ecat_phy_config(const struct device *esc_dev)
{
	hpm_stat_t s0, s1;

	port0_configured = false;
	port1_configured = false;

	/* Diagnostic MDIO scan so the boot log verifies board strapping before the
	 * address-specific configuration below.
	 */
	for (uint8_t addr = 0U; addr < 4U; ++addr) {
		uint16_t a1, a2;
		if (phy_read(esc_dev, addr, PHY_ID1_REG, &a1) == 0 &&
		    phy_read(esc_dev, addr, PHY_ID2_REG, &a2) == 0) {
			printk("ESC MDIO %u: PHY ID=0x%04x%04x\n", addr, a1, a2);
		}
	}

	s0 = rtl8201f_config(esc_dev);
	s1 = BOARD_ECAT_SUPPORT_PORT1 ? tja1101b_config(esc_dev) : status_success;

	if (s0 != status_success) {
		printk("ESC Port0 (RTL8201F) configuration failed\n");
		return status_fail;
	}
	if (s1 != status_success) {
		printk("ESC Port1 (TJA1101B) configuration failed; continuing with Port0 only\n");
	}
	return status_success;
}

void ecat_phy_update_link(const struct device *esc_dev)
{
	static int prev0 = -1, prev1 = -1;
	static unsigned int dbg_cnt;
	uint16_t status = 0U, control = 0U;
	int up;

	/* Port0: RTL8201F. BMSR LINK_STATUS is latch-low, so the second read gives
	 * the current state. Do NOT require BMSR bit5 (AN complete): on a direct
	 * EtherCAT link the RTL8201F may establish 100M-FD via parallel detection
	 * with auto-negotiation never completing. The SDK's rtl8201_get_phy_status()
	 * likewise checks only bit2 for link.
	 */
	up = 0;
	if (port0_configured &&
	    phy_read(esc_dev, BOARD_ECAT_PORT0_PHY_ADDR, PHY_BMSR_REG, &status) == 0 &&
	    phy_read(esc_dev, BOARD_ECAT_PORT0_PHY_ADDR, PHY_BMSR_REG, &status) == 0 &&
	    status != 0xffffU &&
	    phy_read(esc_dev, BOARD_ECAT_PORT0_PHY_ADDR, PHY_BMCR_REG, &control) == 0 &&
	    control != 0xffffU) {
		up = (status & PHY_LINK_STATUS) &&
		     (control & (BIT(13) | BIT(8))) == (BIT(13) | BIT(8));
	}
	hpm_esc_set_link_state(esc_dev, 0, up != 0);
	if (up != prev0) {
		printk("ESC Port0 link %s\n", up ? "up" : "down");
		prev0 = up;
	}

	/* Port1: TJA1101B 100BASE-T1. The link is always 100M full duplex, so
	 * BMSR bit2 is sufficient; there is no speed/duplex register to cross-check.
	 */
	up = 0;
	if (port1_configured &&
	    phy_read(esc_dev, BOARD_ECAT_PORT1_PHY_ADDR, PHY_BMSR_REG, &status) == 0 &&
	    phy_read(esc_dev, BOARD_ECAT_PORT1_PHY_ADDR, PHY_BMSR_REG, &status) == 0 &&
	    status != 0xffffU) {
		up = (status & PHY_LINK_STATUS) != 0;
	}
	hpm_esc_set_link_state(esc_dev, 1, up != 0);
	if (up != prev1) {
		printk("ESC Port1 link %s\n", up ? "up" : "down");
		prev1 = up;
	}

	/* Debug: periodic raw dump (every ~2s at the 100ms poll rate). */
	if (++dbg_cnt >= 20U) {
		dbg_cnt = 0U;
		uint16_t b0, b1;
		int r0 = phy_read(esc_dev, BOARD_ECAT_PORT0_PHY_ADDR, PHY_BMSR_REG, &b0);
		int r1 = phy_read(esc_dev, BOARD_ECAT_PORT1_PHY_ADDR, PHY_BMSR_REG, &b1);
		printk("ESC phy dump: Port0 BMSR=0x%04x(%d) Port1 BMSR=0x%04x(%d)\n",
		       b0, r0, b1, r1);
	}
}
