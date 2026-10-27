/* SPDX-License-Identifier: BSD-3-Clause */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/ethercat/hpm_esc.h>
#include "ecat_board_cfg.h"
#include "hpm5e31_phy.h"
#include "hpm_esc_drv.h"

#define PHY_BMSR_REG       1U
#define PHY_ID1_REG        2U
#define PHY_ID2_REG        3U
#define PHY_LINK_STATUS    BIT(2)
#define PHY_BMCR_REG       0U
#define PHY_ANAR_REG       4U
#define PHY_PAGE_REG       31U
#define PHY_BMCR_RESET     BIT(15)
#define PHY_100_FULL_AN    (BIT(13) | BIT(12) | BIT(8))
#define PHY_AN_RESTART     BIT(9)
#define RTL8201F_ID        0x001cc810U /* revision nibble masked */

static uint8_t phy_addr;
static bool phy_configured;

/* SHOW bypasses the ESC logical-port address translation during discovery. */
static uint8_t physical_addr(uint8_t addr)
{
	return ESC_PHY_ADDR_SHOW_MASK | addr;
}

static int phy_read(const struct device *dev, uint8_t reg, uint16_t *value)
{
	return hpm_esc_mdio_read(dev, physical_addr(phy_addr), reg, value);
}

static int phy_write(const struct device *dev, uint8_t reg, uint16_t value)
{
	return hpm_esc_mdio_write(dev, physical_addr(phy_addr), reg, value);
}

static int phy_modify(const struct device *dev, uint8_t reg,
		      uint16_t clear, uint16_t set)
{
	uint16_t value;
	int ret = phy_read(dev, reg, &value);

	if (ret != 0 || value == 0xffffU) {
		return -EIO;
	}
	return phy_write(dev, reg, (value & ~clear) | set);
}

/* The optional phy-reset-gpios must match this board's schematic.
 * HPM5E3Y's PV12/PW17 PHY wiring must not be assumed on HPM5E31.
 * REFCK is enabled during EEPROM setup before releasing reset.
 */
void ecat_phy_reset(const struct device *esc_dev)
{
	if (!DT_NODE_HAS_PROP(DT_NODELABEL(esc0), phy_reset_gpios)) {
		printk("ESC Port0: no PHY reset GPIO configured; using hardware reset\n");
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

hpm_stat_t ecat_phy_config(const struct device *esc_dev)
{
	uint16_t value = 0U;
	unsigned int found = 0U;
	int ret;
	const char *why = "unknown";

	phy_configured = false;
	/* RTL8201F QFN32 has two address straps, so physical addresses are 0..3.
	 * Bind only when exactly one RTL8201F responds: multiple PHYs require
	 * an explicit schematic mapping rather than guessing which is Port0.
	 */
	for (uint8_t addr = 0U; addr < 4U; ++addr) {
		uint16_t id1, id2;
		if (hpm_esc_mdio_read(esc_dev, physical_addr(addr), PHY_ID1_REG, &id1) == 0 &&
		    hpm_esc_mdio_read(esc_dev, physical_addr(addr), PHY_ID2_REG, &id2) == 0) {
			uint32_t id = ((uint32_t)id1 << 16) | id2;
			printk("ESC MDIO %u: PHY ID=0x%08x\n", addr, id);
			if ((id & 0xfffffff0U) == RTL8201F_ID) {
				phy_addr = addr;
				found++;
			}
		}
	}
	if (found != 1U) {
		printk("ESC Port0: expected one RTL8201F, found %u; check straps/reset/25MHz/MDIO\n",
		       found);
		return status_fail;
	}
	/* Keep ESC hardware management and the master's logical Port0 aligned. */
	esc_set_phy_offset((ESC_Type *)hpm_esc_base(esc_dev), phy_addr);
	printk("ESC Port0: RTL8201F at physical MDIO %u (DT initial offset %u)\n",
	       phy_addr, BOARD_ECAT_PHY_ADDR_OFFSET);

	/* Diagnostic: toggle BMCR loopback to prove MDIO writes reach the PHY. */
	{
		uint16_t probe0, probe1, probe_wr;
		if (phy_read(esc_dev, PHY_BMCR_REG, &probe0) == 0) {
			probe_wr = (uint16_t)(probe0 | 0x4000U); /* loopback bit14 */
			if (phy_write(esc_dev, PHY_BMCR_REG, probe_wr) == 0 &&
			    phy_read(esc_dev, PHY_BMCR_REG, &probe1) == 0) {
				printk("ESC Port0: BMCR write probe wrote 0x%04x read 0x%04x => %s\n",
				       probe_wr, probe1,
				       (probe1 & 0x4000U) ? "write OK" : "WRITE NOT STUCK");
				(void)phy_write(esc_dev, PHY_BMCR_REG, probe0);
			} else {
				printk("ESC Port0: BMCR write probe failed\n");
			}
		}
	}

	ret = phy_write(esc_dev, PHY_PAGE_REG, 0U);
	if (ret != 0 || phy_write(esc_dev, PHY_BMCR_REG, PHY_BMCR_RESET) != 0) {
		why = "software-reset write (page0/BMCR)";
		goto fail;
	}
	/* Bounded software-reset completion wait. */
	for (unsigned int retry = 0U; retry < 100U; ++retry) {
		k_msleep(1);
		if (phy_read(esc_dev, PHY_BMCR_REG, &value) != 0 || value == 0xffffU) {
			why = "BMCR read during reset wait";
			goto fail;
		}
		if ((value & PHY_BMCR_RESET) == 0U) {
			break;
		}
	}
	if ((value & PHY_BMCR_RESET) != 0U) {
		why = "software-reset timeout (bit15 stuck)";
		goto fail;
	}

	/* R60 straps RXDV low for MII. Enforce MII without copying the
	 * datasheet's RMII external-clock example 0x7ffb (which sets bit3).
	 */
	if (phy_write(esc_dev, PHY_PAGE_REG, 7U) != 0 ||
	    phy_modify(esc_dev, 16U, BIT(3), 0U) != 0 ||
	    phy_write(esc_dev, PHY_PAGE_REG, 4U) != 0 ||
	    /* Disable EEE capabilities for a continuous EtherCAT MII clock. */
	    phy_modify(esc_dev, 16U, BIT(13) | BIT(12), 0U) != 0 ||
	    phy_write(esc_dev, PHY_PAGE_REG, 0U) != 0 ||
	    /* Disable link-down power saving; retain reserved bits. */
	    phy_modify(esc_dev, 24U, BIT(15), 0U) != 0 ||
	    /* Advertise only 100BASE-TX full duplex, with auto-negotiation. */
	    phy_write(esc_dev, PHY_ANAR_REG, BIT(8) | BIT(0)) != 0) {
		why = "MII/EEE/AN register sequence";
		goto fail;
	}
	if (phy_read(esc_dev, PHY_ANAR_REG, &value) != 0 || value != 0x0101U) {
		printk("RTL8201F: ANAR readback 0x%04x (expected 0x0101), continuing\n",
		       value);
	}
	if (phy_write(esc_dev, PHY_BMCR_REG, PHY_100_FULL_AN | PHY_AN_RESTART) != 0) {
		why = "BMCR AN-restart write";
		goto fail;
	}
	phy_configured = true;
	printk("RTL8201F: MII, advertise 100/full only, auto-negotiation enabled\n");
	hpm_esc_set_link_state(esc_dev, 0, false);
	return status_success;

fail:
	(void)phy_write(esc_dev, PHY_PAGE_REG, 0U);
	hpm_esc_set_link_state(esc_dev, 0, false);
	printk("RTL8201F register configuration/reset failed: %s (last read=0x%04x)\n",
	       why, value);
	return status_fail;
}

void ecat_phy_update_link(const struct device *esc_dev)
{
	static int previous = -1;
	static unsigned int dbg_cnt;
	uint16_t status = 0U;
	uint16_t control = 0U;
	int up = 0;

	/* BMSR LINK_STATUS is latch-low; the second read gives current state.
	 * Do NOT require BMSR bit5 (AN complete): on a direct/simple EtherCAT
	 * link the RTL8201F may establish 100M-FD via parallel detection with
	 * auto-negotiation never completing, leaving bit5 clear while the link
	 * is fully usable. The SDK's rtl8201_get_phy_status() also checks only
	 * bit2 for link.
	 */
	if (phy_configured &&
	    phy_read(esc_dev, PHY_BMSR_REG, &status) == 0 &&
	    phy_read(esc_dev, PHY_BMSR_REG, &status) == 0 && status != 0xffffU &&
	    phy_read(esc_dev, PHY_BMCR_REG, &control) == 0 && control != 0xffffU) {
		up = (status & PHY_LINK_STATUS) &&
		     (control & (BIT(13) | BIT(8))) == (BIT(13) | BIT(8));
		if ((status & PHY_LINK_STATUS) && !up) {
			printk("ESC Port0 link mismatch: BMSR=0x%04x BMCR=0x%04x\n",
			       status, control);
		}
	}

	/* Debug: periodic raw dump (every ~2s at the 100ms poll rate). */
	if (++dbg_cnt >= 20U) {
		dbg_cnt = 0U;
		printk("ESC Port0 phy dump: BMSR=0x%04x BMCR=0x%04x\n",
		       status, control);
	}

	hpm_esc_set_link_state(esc_dev, 0, up != 0);
	if (up != previous) {
		printk("ESC Port0 link %s\n", up ? "up" : "down");
		previous = up;
	}
}
