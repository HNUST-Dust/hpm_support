/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
enum {
    clock_cpu0,
    clock_mchtmr0,
    clock_ahb0,
    clock_axic,
    clock_axif,
    clock_axis,
    clock_rom0,
    clock_lmm0,
    clock_xpi0,
    clock_ram0,
    clock_hdma,
    clock_xdma,
    clock_gpio,
    clock_pwm0
};
enum {
    clk_src_osc24m,
    clk_src_pll0_clk0,
    pllctlv2_pll0,
    pllctlv2_clk0,
    pllctlv2_clk1,
    pllctlv2_div_1p0,
    pllctlv2_div_1p2
};
#define HPM_PLLCTLV2 1
#define HPM_SYSCTL 2
#define HPM_PCFG 3
#define PLLCTL_SOC_PLL_REFCLK_FREQ 24000000UL
#define status_success 0
static bool resources[32], high_selected;
static uint32_t boot_hz, pll_hz;
static int source, voltage, fail_voltage, fail_pll, updates;
static uint32_t clock_get_frequency(int clock) {
    assert(clock == clock_cpu0);
    return boot_hz;
}
static void pllctlv2_xtal_set_rampup_time(int dev, uint32_t cycles) {
    assert(dev == 1 && cycles == 288000);
}
static void sysctl_clock_set_preset(int dev, int preset) { assert(dev == 2 && preset == 2); }
static void clock_add_to_group(int clock, int group) {
    assert(!group);
    resources[clock] = true;
}
static void clock_connect_group_to_cpu(int group, int cpu) { assert(!group && !cpu); }
static int pcfg_dcdc_set_voltage(int dev, int mv) {
    assert(dev == 3 && source == clk_src_osc24m && mv == 1275);
    if (fail_voltage) {
        return 1;
    }
    voltage = mv;
    return 0;
}
static void pcfg_dcdc_switch_to_dcm_mode(int dev) { assert(dev == 3 && voltage == 1275); }
static void pllctlv2_set_postdiv(int dev, int pll, int clock, int divider) {
    assert(dev == 1 && pll == pllctlv2_pll0 && source == clk_src_osc24m && voltage == 1275);
    assert((clock == pllctlv2_clk0 && divider == pllctlv2_div_1p0) ||
           (clock == pllctlv2_clk1 && divider == pllctlv2_div_1p2));
}
static int pllctlv2_init_pll_with_freq(int dev, int pll, uint32_t hz) {
    assert(dev == 1 && pll == pllctlv2_pll0 && source == clk_src_osc24m && hz == 480000000);
    if (fail_pll) {
        return 1;
    }
    pll_hz = hz;
    return 0;
}
static void clock_set_source_divider(int clock, int src, int div) {
    assert(div == 1);
    if (clock == clock_cpu0) {
        if (src == clk_src_pll0_clk0) {
            assert(pll_hz == 480000000 && voltage == 1275 && resources[clock_rom0] &&
                   resources[clock_lmm0] && resources[clock_axis]);
            high_selected = true;
        }
        source = src;
    } else {
        assert(clock == clock_mchtmr0 && src == clk_src_osc24m);
    }
}
static void clock_update_core_clock(void) {
    assert(high_selected);
    ++updates;
}
#include "hpm5e_clock.inc"
static void Reset(uint32_t boot) {
    memset(resources, 0, sizeof(resources));
    high_selected = false;
    boot_hz = boot;
    pll_hz = 600000000;
    source = clk_src_pll0_clk0;
    voltage = 1200;
    fail_voltage = fail_pll = updates = 0;
}
int main(void) {
    Reset(24000000);
    assert(soc_init_clock() == 0 && high_selected && updates == 1);
    Reset(600000000);
    assert(soc_init_clock() == 0 && high_selected && updates == 1);
    Reset(600000000);
    fail_voltage = 1;
    assert(soc_init_clock() == -EIO && !high_selected && source == clk_src_osc24m);
    Reset(600000000);
    fail_pll = 1;
    assert(soc_init_clock() == -EIO && !high_selected && source == clk_src_osc24m);
    puts("Real HPM5E clock startup: safe source / voltage / PLL / LMM ROM clocks contracts PASS");
}
