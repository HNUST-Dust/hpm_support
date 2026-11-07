/* SPDX-License-Identifier: BSD-3-Clause */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/ethercat/hpm_esc.h>
#include "ecat_board_cfg.h"
#include "hpm5e31_phy.h"
#include "hpm_esc_drv.h"
/*目前5E31有两套PHY布局分别是 P0:RTL8201F P1:TJA1101 |
P0:TJA1101 P1:TJA1101 根据布局前往EtherCAT\bsp\ecat_board_cfg.h文件修改
*/
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

/* PHY identifier = ID1 << 16 | ID2; the low nibble of ID2 is the silicon revision. */
#define PHY_ID_REV_MASK    0xfffffff0U
#define RTL8201F_ID        0x001cc810U /* Boot log: 0x001cc816 */
#define TJA1101_ID         0x0180dd00U /* Boot log: 0x0180dd02 (NXP OUI) */

/*
 * TJA1101 extended registers, as the Linux nxp-tja11xx driver uses them; check them
 * against the NXP datasheet. Configuration writes need ECTRL.CONFIG_EN first.
 */
#define TJA1101_ECTRL_REG               17U
#define TJA1101_ECTRL_LINK_CONTROL      BIT(15)
#define TJA1101_ECTRL_POWER_MODE_MASK   (0xFU << 11)
#define TJA1101_ECTRL_POWER_MODE_NORMAL (0x3U << 11)
#define TJA1101_ECTRL_POWER_MODE_STANDBY (0xCU << 11)
#define TJA1101_ECTRL_CONFIG_EN         BIT(2)
#define TJA1101_CFG1_REG                18U
#define TJA1101_CFG1_MASTER             BIT(15)
#define TJA1101_CFG1_MII_MODE_MASK      (0x3U << 8) /* 00 = MII */
#define TJA1101_COMMCFG_REG             27U
#define TJA1101_COMMCFG_AUTO_OP         BIT(15)
/* CLK_MODE per the TJA1101 datasheet as recalled: 00 crystal, 10 external 25 MHz. */
#define TJA1101_COMMCFG_CLK_MODE_MASK   (0x3U << 12)
#define TJA1101_COMMCFG_CLK_MODE_XTAL   (0x0U << 12)
#define TJA1101_COMMCFG_CLK_MODE_EXT25M (0x2U << 12)
#define TJA1101_PLL_LOCK_TIMEOUT_MS     20U
#define TJA1101_GENSTAT_REG             24U
#define TJA1101_GENSTAT_PLL_LOCKED      BIT(14)

static bool port0_configured;
static bool port1_configured;

/*---------------------*/
/* Public: ecat_phy_reset, ecat_phy_keep_reset, ecat_phy_config and
 * ecat_phy_update_link are declared in hpm5e31_phy.h.
 */

/* MDIO access */
static uint8_t physical_addr(uint8_t addr);
static int phy_read(const struct device *dev, uint8_t addr, uint8_t reg, uint16_t *value);
static int phy_write(const struct device *dev, uint8_t addr, uint8_t reg, uint16_t value);
static int phy_modify(const struct device *dev, uint8_t addr, uint8_t reg,
		      uint16_t clear, uint16_t set);
static int phy_soft_reset(const struct device *dev, uint8_t addr, const char *name);

/* PHY drivers */
static hpm_stat_t rtl8201f_config(const struct device *esc_dev, uint8_t port, uint8_t addr);
static bool rtl8201f_link_up(const struct device *esc_dev, uint8_t addr);
static hpm_stat_t tja1101_config(const struct device *esc_dev, uint8_t port, uint8_t addr);
static bool tja1101_link_up(const struct device *esc_dev, uint8_t addr);

/* Dispatch by the board's PHY type */
static const char *phy_name(int phy);
static hpm_stat_t phy_config(const struct device *esc_dev, uint8_t port, uint8_t addr, int phy);
static bool phy_link_up(const struct device *esc_dev, uint8_t addr, int phy);

/* Ports */
static hpm_stat_t port0_config(const struct device *esc_dev);
static hpm_stat_t port1_config(const struct device *esc_dev);
static bool port0_link_up(const struct device *esc_dev);
static bool port1_link_up(const struct device *esc_dev);
/*--------------------*/

/*-------------------------P0,P1根据不同的PHY修改-----------------------*/
static hpm_stat_t port0_config(const struct device *esc_dev)
{
	return phy_config(esc_dev, 0U, BOARD_ECAT_PORT0_PHY_ADDR, BOARD_ECAT_PORT0_PHY);
}

static hpm_stat_t port1_config(const struct device *esc_dev)
{
	return phy_config(esc_dev, 1U, BOARD_ECAT_PORT1_PHY_ADDR, BOARD_ECAT_PORT1_PHY);
}

static bool port0_link_up(const struct device *esc_dev)
{
	return phy_link_up(esc_dev, BOARD_ECAT_PORT0_PHY_ADDR, BOARD_ECAT_PORT0_PHY);
}

static bool port1_link_up(const struct device *esc_dev)
{
	return phy_link_up(esc_dev, BOARD_ECAT_PORT1_PHY_ADDR, BOARD_ECAT_PORT1_PHY);
}
/*----------------------------------------------------------------------*/

/*------------------------------统一定义及Link判定-----------------------*/
static hpm_stat_t phy_config(const struct device *esc_dev, uint8_t port, uint8_t addr, int phy)
{
	switch (phy) {
	case BOARD_ECAT_PHY_RTL8201F:
		return rtl8201f_config(esc_dev, port, addr);
	case BOARD_ECAT_PHY_TJA1101:
		return tja1101_config(esc_dev, port, addr);
	case BOARD_ECAT_PHY_NONE:
		return status_success;
	default:
		printk("ESC Port%u: unknown PHY type %d\n", port, phy);
		return status_fail;
	}
}

static bool phy_link_up(const struct device *esc_dev, uint8_t addr, int phy)
{
	switch (phy) {
	case BOARD_ECAT_PHY_RTL8201F:
		return rtl8201f_link_up(esc_dev, addr);
	case BOARD_ECAT_PHY_TJA1101:
		return tja1101_link_up(esc_dev, addr);
	default:
		return false;
	}
}
/*--------------------------------------------------------------------*/

/*---------------------Config(仅有两个PHY)----------------------------*/
static hpm_stat_t rtl8201f_config(const struct device *esc_dev, uint8_t port, uint8_t addr)
{
	char name[8];
	uint16_t id1 = 0U, id2 = 0U, value = 0U;

	snprintk(name, sizeof(name), "Port%u", port);
	if (phy_read(esc_dev, addr, PHY_ID1_REG, &id1) != 0 ||
	    phy_read(esc_dev, addr, PHY_ID2_REG, &id2) != 0) {
		printk("ESC %s: no PHY at MDIO %u\n", name, addr);
		return status_fail;
	}
	uint32_t id = ((uint32_t)id1 << 16) | id2;

	printk("ESC %s: PHY ID=0x%08x at MDIO %u\n", name, id, addr);
	if ((id & PHY_ID_REV_MASK) != RTL8201F_ID) {
		printk("ESC %s: unexpected PHY ID 0x%08x (expected RTL8201F 0x%08x)\n",
		       name, id, RTL8201F_ID);
		return status_fail;
	}

	if (phy_write(esc_dev, addr, PHY_PAGE_REG, 0U) != 0 ||
	    phy_soft_reset(esc_dev, addr, name) != 0) {
		printk("ESC %s: software reset failed\n", name);
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
		printk("ESC %s: RTL8201F MII/EEE/AN register sequence failed\n", name);
		(void)phy_write(esc_dev, addr, PHY_PAGE_REG, 0U);
		return status_fail;
	}
	if (phy_read(esc_dev, addr, PHY_ANAR_REG, &value) != 0 || value != 0x0101U) {
		printk("ESC %s: ANAR readback 0x%04x (expected 0x0101), continuing\n",
		       name, value);
	}
	if (phy_write(esc_dev, addr, PHY_BMCR_REG, PHY_100_FULL_AN | PHY_AN_RESTART) != 0) {
		printk("ESC %s: BMCR AN-restart write failed\n", name);
		return status_fail;
	}
	hpm_esc_set_link_state(esc_dev, port, false);
	printk("ESC %s: RTL8201F MII, advertise 100/full only, auto-negotiation enabled\n", name);
	return status_success;
}
static hpm_stat_t tja1101_config(const struct device *esc_dev, uint8_t port, uint8_t addr)
{
	char name[8];
	uint16_t id1 = 0U, id2 = 0U;
	snprintk(name, sizeof(name), "Port%u", port);
	if (phy_read(esc_dev, addr, PHY_ID1_REG, &id1) != 0 ||
	    phy_read(esc_dev, addr, PHY_ID2_REG, &id2) != 0 ||
	    id1 == 0xffffU) {
		printk("ESC %s: no PHY at MDIO %u\n", name, addr);
		return status_fail;
	}
	uint32_t id = ((uint32_t)id1 << 16) | id2;

	printk("ESC %s: PHY ID=0x%08x at MDIO %u\n", name, id, addr);
	if ((id & PHY_ID_REV_MASK) != TJA1101_ID) {
		printk("ESC %s: unexpected PHY ID 0x%08x (expected TJA1101 0x%08x)\n",
		       name, id, TJA1101_ID);
		return status_fail;
	}

	if (phy_soft_reset(esc_dev, addr, name) != 0) {
		printk("ESC %s: TJA1101 software reset failed\n", name);
		return status_fail;
	}

	/*
	 * A managed-mode strap leaves the PHY in standby with link control off, so the
	 * link never comes up. Force autonomous operation, the MII host interface, normal
	 * power mode and link control. The master/slave role follows the board macro.
	 */
	int role = port == 0U ? BOARD_ECAT_PORT0_T1_ROLE : BOARD_ECAT_PORT1_T1_ROLE;
	uint16_t role_clear = role == BOARD_ECAT_T1_ROLE_SLAVE ? TJA1101_CFG1_MASTER : 0U;
	uint16_t role_set = role == BOARD_ECAT_T1_ROLE_MASTER ? TJA1101_CFG1_MASTER : 0U;
	const uint16_t clk_mode = BOARD_ECAT_T1_CLK == BOARD_ECAT_T1_CLK_EXT_25M ?
		TJA1101_COMMCFG_CLK_MODE_EXT25M : TJA1101_COMMCFG_CLK_MODE_XTAL;
	uint16_t ectrl = 0U, cfg1 = 0U, commcfg = 0U, genstat = 0U;

	/*
	 * Configure in standby, then enter normal mode so the PLL starts from the selected
	 * clock source. With CLK_MODE set to crystal while the clock comes in on CLK_IN_OUT
	 * the PLL never locks and neither does the link.
	 */
	if (phy_modify(esc_dev, addr, TJA1101_ECTRL_REG, TJA1101_ECTRL_LINK_CONTROL,
		       TJA1101_ECTRL_CONFIG_EN) != 0 ||
	    phy_modify(esc_dev, addr, TJA1101_ECTRL_REG, TJA1101_ECTRL_POWER_MODE_MASK,
		       TJA1101_ECTRL_POWER_MODE_STANDBY) != 0 ||
	    phy_modify(esc_dev, addr, TJA1101_COMMCFG_REG, TJA1101_COMMCFG_CLK_MODE_MASK,
		       TJA1101_COMMCFG_AUTO_OP | clk_mode) != 0 ||
	    phy_modify(esc_dev, addr, TJA1101_CFG1_REG,
		       TJA1101_CFG1_MII_MODE_MASK | role_clear, role_set) != 0 ||
	    phy_modify(esc_dev, addr, TJA1101_ECTRL_REG, TJA1101_ECTRL_POWER_MODE_MASK,
		       TJA1101_ECTRL_POWER_MODE_NORMAL) != 0) {
		printk("ESC %s: TJA1101 autonomous/clock/MII register sequence failed\n", name);
		return status_fail;
	}
	bool pll_locked = false;

	for (unsigned int ms = 0U; ms < TJA1101_PLL_LOCK_TIMEOUT_MS && !pll_locked; ++ms) {
		k_msleep(1);
		pll_locked = phy_read(esc_dev, addr, TJA1101_GENSTAT_REG, &genstat) == 0 &&
			     (genstat & TJA1101_GENSTAT_PLL_LOCKED) != 0U;
	}
	if (!pll_locked) {
		/* Keep the port configured: the link poll reports the link once the clock is fixed. */
		printk("ESC %s: TJA1101 PLL not locked after %u ms (GENSTAT=0x%04x); check the %s\n",
		       name, TJA1101_PLL_LOCK_TIMEOUT_MS, genstat,
		       BOARD_ECAT_T1_CLK == BOARD_ECAT_T1_CLK_EXT_25M ?
		       "25 MHz clock on CLK_IN_OUT (ESC0_REFCK)" : "25 MHz crystal");
	}
	if (phy_modify(esc_dev, addr, TJA1101_ECTRL_REG, 0U, TJA1101_ECTRL_LINK_CONTROL) != 0 ||
	    phy_read(esc_dev, addr, TJA1101_ECTRL_REG, &ectrl) != 0 ||
	    phy_read(esc_dev, addr, TJA1101_CFG1_REG, &cfg1) != 0 ||
	    phy_read(esc_dev, addr, TJA1101_COMMCFG_REG, &commcfg) != 0) {
		printk("ESC %s: TJA1101 link control write failed\n", name);
		return status_fail;
	}
	printk("ESC %s: TJA1101 ECTRL=0x%04x CFG1=0x%04x COMMCFG=0x%04x\n",
	       name, ectrl, cfg1, commcfg);
	if ((commcfg & TJA1101_COMMCFG_AUTO_OP) == 0U ||
	    (commcfg & TJA1101_COMMCFG_CLK_MODE_MASK) != clk_mode ||
	    (cfg1 & TJA1101_CFG1_MII_MODE_MASK) != 0U ||
	    (cfg1 & (role_clear | role_set)) != role_set ||
	    (ectrl & TJA1101_ECTRL_LINK_CONTROL) == 0U ||
	    (ectrl & TJA1101_ECTRL_POWER_MODE_MASK) != TJA1101_ECTRL_POWER_MODE_NORMAL) {
		printk("ESC %s: TJA1101 readback does not match autonomous/clock/MII/role/normal/link\n",
		       name);
		return status_fail;
	}
	hpm_esc_set_link_state(esc_dev, port, false);
	printk("ESC %s: TJA1101 autonomous, %s clock, PLL %s, MII, link control on, %s (%s)\n",
	       name, BOARD_ECAT_T1_CLK == BOARD_ECAT_T1_CLK_EXT_25M ? "external 25 MHz" : "crystal",
	       pll_locked ? "locked" : "NOT locked",
	       (cfg1 & TJA1101_CFG1_MASTER) != 0U ? "master" : "slave",
	       role == BOARD_ECAT_T1_ROLE_STRAP ? "strap" : "forced");
	return status_success;
}
/*-----------------------------------------------------------*/

/*----------------------------Link----------------------------*/
static bool rtl8201f_link_up(const struct device *esc_dev, uint8_t addr)
{
	uint16_t status = 0U, control = 0U;

	return phy_read(esc_dev, addr, PHY_BMSR_REG, &status) == 0 &&
	       phy_read(esc_dev, addr, PHY_BMSR_REG, &status) == 0 &&
	       status != 0xffffU &&
	       phy_read(esc_dev, addr, PHY_BMCR_REG, &control) == 0 &&
	       control != 0xffffU &&
	       (status & PHY_LINK_STATUS) != 0U &&
	       (control & (BIT(13) | BIT(8))) == (BIT(13) | BIT(8));
}
static bool tja1101_link_up(const struct device *esc_dev, uint8_t addr)
{
	uint16_t status = 0U;

	return phy_read(esc_dev, addr, PHY_BMSR_REG, &status) == 0 &&
	       phy_read(esc_dev, addr, PHY_BMSR_REG, &status) == 0 &&
	       status != 0xffffU &&
	       (status & PHY_LINK_STATUS) != 0U;
}
/*-----------------------------------------------------------*/


/*----------------------上层API------------------------------*/
hpm_stat_t ecat_phy_config(const struct device *esc_dev)
{
	hpm_stat_t s0, s1;

	port0_configured = false;
	port1_configured = false;

	/* Diagnostic MDIO scan so the boot log verifies board strapping before the
	 * address-specific configuration below.
	 */
	printk("ESC PHY layout: Port0 %s, Port1 %s\n", phy_name(BOARD_ECAT_PORT0_PHY),
	       phy_name(BOARD_ECAT_PORT1_PHY));
	for (uint8_t addr = 0U; addr < 4U; ++addr) {
		uint16_t a1, a2;
		if (phy_read(esc_dev, addr, PHY_ID1_REG, &a1) == 0 &&
		    phy_read(esc_dev, addr, PHY_ID2_REG, &a2) == 0) {
			printk("ESC MDIO %u: PHY ID=0x%04x%04x\n", addr, a1, a2);
		}
	}

	s0 = port0_config(esc_dev);
	s1 = port1_config(esc_dev);
	port0_configured = s0 == status_success && BOARD_ECAT_PORT0_PHY != BOARD_ECAT_PHY_NONE;
	port1_configured = s1 == status_success && BOARD_ECAT_PORT1_PHY != BOARD_ECAT_PHY_NONE;

	if (s0 != status_success) {
		printk("ESC Port0 (%s) configuration failed\n", phy_name(BOARD_ECAT_PORT0_PHY));
		return status_fail;
	}
	if (s1 != status_success) {
		printk("ESC Port1 (%s) configuration failed; continuing with Port0 only\n",
		       phy_name(BOARD_ECAT_PORT1_PHY));
	}
	return status_success;
}

void ecat_phy_update_link(const struct device *esc_dev)
{
	static int prev0 = -1, prev1 = -1;
	int up;

	up = port0_configured && port0_link_up(esc_dev);
	hpm_esc_set_link_state(esc_dev, 0, up != 0);
	if (up != prev0) {
		printk("ESC Port0 link %s\n", up ? "up" : "down");
		prev0 = up;
	}

	up = port1_configured && port1_link_up(esc_dev);
	hpm_esc_set_link_state(esc_dev, 1, up != 0);
	if (up != prev1) {
		printk("ESC Port1 link %s\n", up ? "up" : "down");
		prev1 = up;
	}
}
/*----------------------------------------------------------*/

/*-----------------------复位相关API------------------------*/
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
//软复位

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
}//硬复位
/*----------------------------------------------------------*/


/*----------------------读改写API----------------*/
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
/*-------------------------------------------------*/

/*---------------------------MDIO驱动接口--------------------------*/
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
/*----------------------------------------------------------------*/

/*----------------------------其他-----------------------------*/
static const char *phy_name(int phy)
{
	switch (phy) {
	case BOARD_ECAT_PHY_RTL8201F:
		return "RTL8201F";
	case BOARD_ECAT_PHY_TJA1101:
		return "TJA1101";
	case BOARD_ECAT_PHY_NONE:
		return "none";
	default:
		return "unknown";
	}
}
/*----------------------------------------------------------*/