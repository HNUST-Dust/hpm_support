/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 */

#include <zephyr/init.h>
#include <zephyr/irq.h>
#include <zephyr/linker/linker-defs.h>
#include <hpm_clock_drv.h>
#include <hpm_pcfg_drv.h>
#include <hpm_pllctlv2_drv.h>
#include <hpm_pmp_drv.h>
#include <hpm_soc.h>

#ifdef CONFIG_XIP
#include <hpm_bootheader.h>
#define SOC_NV_FLASH_NODE DT_CHOSEN(zephyr_flash)
__attribute__((section(".nor_cfg_option"), used)) const uint32_t option[4] = {
	DT_PROP(SOC_NV_FLASH_NODE, nor_cfg_opt_hdr),
	DT_PROP(SOC_NV_FLASH_NODE, nor_cfg_opt_opt0),
	DT_PROP(SOC_NV_FLASH_NODE, nor_cfg_opt_opt1),
	0x0
};
__attribute__((section(".last_section"))) const uint32_t rom_marker =
	CONFIG_LINKER_LAST_SECTION_ID_PATTERN;
#endif

__attribute__((weak)) void c_startup(void) {}

static void soc_init_clock(void)
{
	if (clock_get_frequency(clock_cpu0) == PLLCTL_SOC_PLL_REFCLK_FREQ) {
		pllctlv2_xtal_set_rampup_time(HPM_PLLCTLV2, 32UL * 1000UL * 9U);
		sysctl_clock_set_preset(HPM_SYSCTL, 2);
	}
	clock_add_to_group(clock_cpu0, 0);
	clock_add_to_group(clock_mchtmr0, 0);
	clock_add_to_group(clock_ahb0, 0);
	clock_add_to_group(clock_axic, 0);
	clock_add_to_group(clock_axif, 0);
	clock_add_to_group(clock_xpi0, 0);
	clock_add_to_group(clock_ram0, 0);
	clock_add_to_group(clock_hdma, 0);
	clock_add_to_group(clock_xdma, 0);
	clock_add_to_group(clock_gpio, 0);
	clock_add_to_group(clock_pwm0, 0);
	clock_connect_group_to_cpu(0, 0);
	pcfg_dcdc_set_voltage(HPM_PCFG, 1200);
	clock_set_source_divider(clock_cpu0, clk_src_pll0_clk0, 1);
	clock_set_source_divider(clock_mchtmr0, clk_src_osc24m, 1);
}

#ifdef CONFIG_NOCACHE_MEMORY
static void soc_init_pma(void)
{
	uint32_t start = (uint32_t)&_nocache_ram_start;
	uint32_t length = (uint32_t)&_nocache_ram_size;
	pma_attr_t attr = { 0 };

	if (length == 0U) {
		return;
	}
	__ASSERT((length & (length - 1U)) == 0U, "nocache size must be power of two");
	__ASSERT((start & (length - 1U)) == 0U, "nocache region must be aligned");
	attr.pma_addr = PMA_NAPOT_ADDR(start, length);
	attr.pma_cfg.val = PMA_CFG(ADDR_MATCH_NAPOT, MEM_TYPE_MEM_NON_CACHE_BUF, AMO_EN);
	pma_config_attributes(&attr, 1U);
}
#endif

static int hpmicro_soc_init(void)
{
	unsigned int key = irq_lock();

	soc_init_clock();
#ifdef CONFIG_NOCACHE_MEMORY
	soc_init_pma();
#endif
	irq_unlock(key);
	return 0;
}

SYS_INIT(hpmicro_soc_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
