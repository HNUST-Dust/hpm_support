/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 */

#include <errno.h>
#include <hpm_clock_drv.h>
#include <hpm_pcfg_drv.h>
#include <hpm_pllctlv2_drv.h>
#include <hpm_pmp_drv.h>
#include <hpm_soc.h>
#include <zephyr/init.h>
#include <zephyr/irq.h>
#include <zephyr/linker/linker-defs.h>

#ifdef CONFIG_XIP
#include <hpm_bootheader.h>
#define SOC_NV_FLASH_NODE DT_CHOSEN(zephyr_flash)
__attribute__((section(".nor_cfg_option"), used)) const uint32_t option[4] = {
    DT_PROP(SOC_NV_FLASH_NODE, nor_cfg_opt_hdr), DT_PROP(SOC_NV_FLASH_NODE, nor_cfg_opt_opt0),
    DT_PROP(SOC_NV_FLASH_NODE, nor_cfg_opt_opt1), 0x0};
__attribute__((section(".last_section"))) const uint32_t rom_marker =
    CONFIG_LINKER_LAST_SECTION_ID_PATTERN;
#endif

__attribute__((weak)) void c_startup(void) {}

static int soc_init_clock(void) {
    if (clock_get_frequency(clock_cpu0) == PLLCTL_SOC_PLL_REFCLK_FREQ) {
        pllctlv2_xtal_set_rampup_time(HPM_PLLCTLV2, 32UL * 1000UL * 9U);
        sysctl_clock_set_preset(HPM_SYSCTL, 2);
    }
    clock_add_to_group(clock_cpu0, 0);
    clock_add_to_group(clock_mchtmr0, 0);
    clock_add_to_group(clock_ahb0, 0);
    clock_add_to_group(clock_axic, 0);
    clock_add_to_group(clock_axif, 0);
    clock_add_to_group(clock_axis, 0);
    clock_add_to_group(clock_rom0, 0);
    clock_add_to_group(clock_lmm0, 0);
    clock_add_to_group(clock_xpi0, 0);
    clock_add_to_group(clock_ram0, 0);
    clock_add_to_group(clock_hdma, 0);
    clock_add_to_group(clock_xdma, 0);
    clock_add_to_group(clock_gpio, 0);
    clock_add_to_group(clock_pwm0, 0);
    clock_connect_group_to_cpu(0, 0);
    // Boot ROM/preset 的 PLL0 可能仍为 600 MHz，不能直接作为 5E31 CPU 时钟。
    // 重配 PLL 前先切到晶振；电压、后分频和 PLL 频率对齐 SDK 5E00EVK。
    clock_set_source_divider(clock_cpu0, clk_src_osc24m, 1);
    if (pcfg_dcdc_set_voltage(HPM_PCFG, 1275) != status_success) {
        return -EIO;
    }
    pcfg_dcdc_switch_to_dcm_mode(HPM_PCFG);
    pllctlv2_set_postdiv(HPM_PLLCTLV2, pllctlv2_pll0, pllctlv2_clk0, pllctlv2_div_1p0);
    pllctlv2_set_postdiv(HPM_PLLCTLV2, pllctlv2_pll0, pllctlv2_clk1, pllctlv2_div_1p2);
    if (pllctlv2_init_pll_with_freq(HPM_PLLCTLV2, pllctlv2_pll0, 480000000UL) != status_success) {
        return -EIO;
    }
    clock_set_source_divider(clock_cpu0, clk_src_pll0_clk0, 1);
    clock_update_core_clock();
    clock_set_source_divider(clock_mchtmr0, clk_src_osc24m, 1);
    return 0;
}

#ifdef CONFIG_NOCACHE_MEMORY
static void soc_init_pma(void) {
    uint32_t start = (uint32_t)&_nocache_ram_start;
    uint32_t length = (uint32_t)&_nocache_ram_size;
    pma_attr_t attr = {0};

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

static int hpmicro_soc_init(void) {
    unsigned int key = irq_lock();

    int result = soc_init_clock();
#ifdef CONFIG_NOCACHE_MEMORY
    if (!result) {
        soc_init_pma();
    }
#endif
    irq_unlock(key);
    return result;
}

SYS_INIT(hpmicro_soc_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
