/*
 * Copyright (c) 2022 hpmicro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <errno.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/clock_control.h>
#include <soc.h>
#include <zephyr/drivers/pinctrl.h>
#include "hpm_clock_drv.h"
#include "dt-bindings/pwm/hpmicro-pwm-common.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pwm_hpmicro, CONFIG_PWM_LOG_LEVEL);

/* Support both PWM and PWMv2 */
#if CONFIG_DT_HAS_HPMICRO_HPM_PWM_ENABLED && CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED
#error "Cannot enable both PWM and PWMv2 in the same build"
#elif CONFIG_DT_HAS_HPMICRO_HPM_PWM_ENABLED
#include <hpm_pwm_drv.h>
#define DT_DRV_COMPAT hpmicro_hpm_pwm
#define HPM_PWM_BASE_TYPE PWM_Type
#elif CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED  
#include <hpm_pwmv2_drv.h>
#define DT_DRV_COMPAT hpmicro_hpm_pwmv2
#define HPM_PWM_BASE_TYPE PWMV2_Type  /* PWMv2 may use different type, adjust as needed */
#else
#error "No PWM peripheral enabled"
#endif

struct pwm_hpmicro_config {
	HPM_PWM_BASE_TYPE *base;
	uint32_t clock_name;
	clk_src_t clock_src;
	uint32_t clock_div;
	uint32_t period;
	uint32_t dead_zone_in_half_cycle;
	const struct pinctrl_dev_config *pincfg;
};

struct pwm_hpmicro_data {
	uint32_t period_cycles[1];
#if CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED
	uint8_t enabled_channels;
#endif
};

/* PWM version specific implementations */
#if CONFIG_DT_HAS_HPMICRO_HPM_PWM_ENABLED
static int hpmicro_pwm_v1_set_cycles(const struct device *dev, uint32_t channel,
		       uint32_t period_cycles, uint32_t pulse_cycles,
		       pwm_flags_t flags)
{
	const struct pwm_hpmicro_config *config = dev->config;
	pwm_config_t pwm_config;
	pwm_cmp_config_t cmp_config[2] = {0};
	HPM_PWM_BASE_TYPE *pwm_base = config->base;
	uint32_t rld = 0, xrld = 0, prld = 0;
	uint32_t rld_cmp = 0, xrld_cmp = 0;
	if (channel >= PWM_SOC_PWM_MAX_COUNT || period_cycles == 0U ||
	    pulse_cycles > period_cycles) {
		return -EINVAL;
	}
	/* Reserve a representable compare beyond reload for constant inactive
	 * output. Extended-counter periods need a separate validated encoding. */
	if (period_cycles >= 0xffffffU) {
		return -ENOTSUP;
	}

	pwm_get_default_pwm_config(pwm_base, &pwm_config);
	if (flags == PWM_POLARITY_INVERTED) {
		pwm_config.invert_output = true;
	} else if (flags == PWM_POLARITY_NORMAL) {
		pwm_config.invert_output = false;
	} else if (flags == PWM_TRIG_ENABLE) {
		cmp_config[0].enable_ex_cmp  = false;
		cmp_config[0].mode = pwm_cmp_mode_output_compare;
		cmp_config[0].cmp = period_cycles + 1;
		cmp_config[0].update_trigger = pwm_shadow_register_update_on_hw_event;

		cmp_config[1].mode = pwm_cmp_mode_output_compare;
		cmp_config[1].cmp = period_cycles;
		cmp_config[1].update_trigger = pwm_shadow_register_update_on_modify;
		pwm_config_cmp(pwm_base, channel, &cmp_config[0]);
		pwm_load_cmp_shadow_on_capture(pwm_base, PWM_SOC_CMP_MAX_COUNT - 1, 0);
		pwm_config_cmp(pwm_base, PWM_SOC_CMP_MAX_COUNT - 1, &cmp_config[1]);
	
		pwm_start_counter(pwm_base);
		pwm_issue_shadow_register_lock_event(pwm_base);
		return 0;
	} else {
		return -ENOTSUP;
	}

	rld = period_cycles;
	prld = period_cycles;
	/* Compare output starts low, rises on match, clears at reload.
	 * SDK uses a compare beyond reload for constant inactive output. */
	rld_cmp = pulse_cycles == 0U ? rld + 1U : period_cycles - pulse_cycles;

	if (prld != ((PWM_RLD_XRLD_GET(pwm_base->RLD) << 24) | PWM_RLD_RLD_GET(pwm_base->RLD))) {

		pwm_set_reload(pwm_base, xrld, rld);
		pwm_set_start_count(pwm_base, 0, 0);
	}

	/* Channel setup and polarity must also apply when the period is unchanged. */
	{
		pwm_shadow_register_unlock(pwm_base);
		pwm_config.enable_output = true;
		pwm_config.dead_zone_in_half_cycle = config->dead_zone_in_half_cycle;

		cmp_config[0].enable_ex_cmp  = true;
		cmp_config[0].mode = pwm_cmp_mode_output_compare;
		cmp_config[0].cmp = rld_cmp;
		cmp_config[0].ex_cmp = xrld;
		cmp_config[0].update_trigger = pwm_shadow_register_update_on_modify;

		cmp_config[1].enable_ex_cmp  = true;
		cmp_config[1].mode = pwm_cmp_mode_output_compare;
		cmp_config[1].cmp = rld;
		cmp_config[1].ex_cmp = xrld;
		cmp_config[1].update_trigger = pwm_shadow_register_update_on_modify;

		if (status_success != pwm_setup_waveform(pwm_base, channel, &pwm_config, channel, &cmp_config[0], 1)) {
			LOG_ERR("failed to setup waveform\n");
			return -ENOTSUP;
		}
		pwm_load_cmp_shadow_on_capture(pwm_base, PWM_SOC_CMP_MAX_COUNT - 1, 0);
		pwm_config_cmp(pwm_base, PWM_SOC_CMP_MAX_COUNT - 1, &cmp_config[1]);
	
		pwm_start_counter(pwm_base);
		pwm_issue_shadow_register_lock_event(pwm_base);
	}

	pwm_shadow_register_unlock(pwm_base);
    pwm_cmp_update_cmp_value(pwm_base, channel, rld_cmp, xrld_cmp);
	pwm_issue_shadow_register_lock_event(pwm_base);
	return 0;
}
#endif /* CONFIG_DT_HAS_HPMICRO_HPM_PWM_ENABLED */

#if CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED
static int hpmicro_pwm_v2_set_cycles(const struct device *dev, uint32_t channel,
		       uint32_t period_cycles, uint32_t pulse_cycles,
		       pwm_flags_t flags)
{
	const struct pwm_hpmicro_config *config = dev->config;
	struct pwm_hpmicro_data *data = dev->data;
	HPM_PWM_BASE_TYPE *pwm_base = config->base;
	pwmv2_cmp_config_t cmp_cfg[2] = {0};
	pwmv2_config_t pwm_cfg = {0};
	pwm_counter_t counter;
	uint8_t cmp_start_index;
	uint8_t shadow_reload;
	uint8_t shadow_cmp;

	if (channel > 7) {
		return -ENOTSUP;
	}
	if (period_cycles == 0U || pulse_cycles > period_cycles) {
		return -EINVAL;
	}
	if (period_cycles > 0xffffffU) {
		return -ENOTSUP;
	}
	if ((flags != PWM_POLARITY_NORMAL) && (flags != PWM_POLARITY_INVERTED)) {
		return -ENOTSUP;
	}

	counter = (pwm_counter_t)(channel >> 1);
	cmp_start_index = (uint8_t)((channel >> 1) * 4 + ((channel & 0x01) << 1));
	shadow_reload = (uint8_t)((channel >> 1) * 5);
	shadow_cmp = (uint8_t)(shadow_reload + 1 + ((channel & 0x01) << 1));

	/* Edge-aligned output: high while a <= CNT < b, so a = 0, b = duty.
	 * This mirrors the reference pwmv2 setup (see HPM SDK bldc example). */
	cmp_cfg[0].cmp = 0U;
	cmp_cfg[0].enable_half_cmp = false;
	cmp_cfg[0].enable_hrcmp = false;
	cmp_cfg[0].cmp_source = cmp_value_from_shadow_val;
	cmp_cfg[0].cmp_source_index = shadow_cmp;
	cmp_cfg[0].update_trigger = pwm_shadow_register_update_on_reload;
	cmp_cfg[1].cmp = pulse_cycles;
	cmp_cfg[1].enable_half_cmp = false;
	cmp_cfg[1].enable_hrcmp = false;
	cmp_cfg[1].cmp_source = cmp_value_from_shadow_val;
	cmp_cfg[1].cmp_source_index = (uint8_t)(shadow_cmp + 1U);
	cmp_cfg[1].update_trigger = pwm_shadow_register_update_on_reload;

	pwmv2_get_default_config(&pwm_cfg);
	pwm_cfg.enable_output = true;
	pwm_cfg.invert_output = (flags == PWM_POLARITY_INVERTED);
	pwm_cfg.enable_four_cmp = false;
	pwm_cfg.update_trigger = pwm_reload_update_on_reload;

	pwmv2_shadow_register_unlock(pwm_base);
	pwmv2_set_shadow_val(pwm_base, shadow_reload, period_cycles, 0, false);
	pwmv2_shadow_register_lock(pwm_base);

	pwmv2_counter_select_data_offset_from_shadow_value(pwm_base, counter,
							   shadow_reload);
	pwmv2_counter_burst_disable(pwm_base, counter);
	pwmv2_set_reload_update_time(pwm_base, counter, pwm_reload_update_on_reload);

	/* Full CFG0/CFG1 (output enable, polarity, force off) and compare
	 * sources in one go, as the SDK reference does. */
	pwmv2_setup_waveform(pwm_base, (pwm_channel_t)channel, &pwm_cfg,
			     cmp_start_index, &cmp_cfg[0], 2);

	/* A lone odd channel needs its paired even channel's TRIG_SEL4 set to
	 * enable the output (PWMV2 manual, PWM[CFG0].TRIG_SEL4 note). */
	if (((channel & 0x01U) != 0U) &&
	    ((data->enabled_channels & BIT(channel & ~1U)) == 0U)) {
		pwmv2_enable_four_cmp(pwm_base, (pwm_channel_t)(channel & ~1U));
	}

	pwmv2_channel_enable_output(pwm_base, (pwm_channel_t)channel);
	data->enabled_channels |= BIT(channel);

	pwmv2_enable_counter(pwm_base, counter);
	pwmv2_start_pwm_output(pwm_base, counter);

	return 0;
}
#endif /* CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED */

static int hpmicro_pwm_set_cycles(const struct device *dev, uint32_t channel,
		       uint32_t period_cycles, uint32_t pulse_cycles,
		       pwm_flags_t flags)
{
#if CONFIG_DT_HAS_HPMICRO_HPM_PWM_ENABLED
	return hpmicro_pwm_v1_set_cycles(dev, channel, period_cycles, pulse_cycles, flags);
#elif CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED  
	return hpmicro_pwm_v2_set_cycles(dev, channel, period_cycles, pulse_cycles, flags);
#endif
}

#if CONFIG_DT_HAS_HPMICRO_HPM_PWM_ENABLED
static int hpmicro_pwm_v1_get_cycles_per_sec(const struct device *dev,
			       uint32_t channel, uint64_t *cycles)
{
	const struct pwm_hpmicro_config *config = dev->config;
	uint32_t freqc;

	freqc = clock_get_frequency(config->clock_name);
	*cycles = freqc;

	return 0;
}
#endif /* CONFIG_DT_HAS_HPMICRO_HPM_PWM_ENABLED */

#if CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED
static int hpmicro_pwm_v2_get_cycles_per_sec(const struct device *dev,
			       uint32_t channel, uint64_t *cycles)
{
	const struct pwm_hpmicro_config *config = dev->config;
	uint32_t freqc;

	freqc = clock_get_frequency(config->clock_name);
	*cycles = freqc;

	return 0;
}
#endif /* CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED */

static int hpmicro_pwm_get_cycles_per_sec(const struct device *dev,
			       uint32_t channel, uint64_t *cycles)
{
#if CONFIG_DT_HAS_HPMICRO_HPM_PWM_ENABLED
	return hpmicro_pwm_v1_get_cycles_per_sec(dev, channel, cycles);
#elif CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED  
	return hpmicro_pwm_v2_get_cycles_per_sec(dev, channel, cycles);
#endif
}

#if CONFIG_DT_HAS_HPMICRO_HPM_PWM_ENABLED
static int pwm_hpmicro_v1_init(const struct device *dev)
{
	const struct pwm_hpmicro_config *config = dev->config;
	pwm_config_t pwm_config;
	uint32_t freqc;
	HPM_PWM_BASE_TYPE *pwm_base = config->base;
	int err;

	err = pinctrl_apply_state(config->pincfg, PINCTRL_STATE_DEFAULT);
	if (err < 0) {
		return err;
	}

	/* Enable and configure the PWM peripheral clock; the SDK gate
	 * defaults to off and the DT clocks cell carries source/divider. */
	clock_add_to_group(config->clock_name, 0);
	clock_set_source_divider(config->clock_name, config->clock_src,
				 config->clock_div);

	pwm_get_default_pwm_config(pwm_base, &pwm_config);
	pwm_config.enable_output = true;
    pwm_config.dead_zone_in_half_cycle = config->dead_zone_in_half_cycle;
    pwm_config.invert_output = false;
	freqc = clock_get_frequency(config->clock_name);
	pwm_set_reload(pwm_base, 0, freqc / config->period);
    pwm_set_start_count(pwm_base, 0, 0);

	return 0;
}
#endif /* CONFIG_DT_HAS_HPMICRO_HPM_PWM_ENABLED */

#if CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED
static int pwm_hpmicro_v2_init(const struct device *dev)
{
	const struct pwm_hpmicro_config *config = dev->config;
	HPM_PWM_BASE_TYPE *pwm_base = config->base;
	int err;

	/* Apply pin configuration */
	err = pinctrl_apply_state(config->pincfg, PINCTRL_STATE_DEFAULT);
	if (err < 0) {
		return err;
	}

	/* Enable and configure the PWM peripheral clock; the SDK gate
	 * defaults to off and the DT clocks cell carries source/divider. */
	clock_add_to_group(config->clock_name, 0);
	clock_set_source_divider(config->clock_name, config->clock_src,
				 config->clock_div);

	/* Deinitialize PWMv2 to reset to known state */
	pwmv2_deinit(pwm_base);

	return 0;
}
#endif /* CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED */

static int pwm_hpmicro_init(const struct device *dev)
{
#if CONFIG_DT_HAS_HPMICRO_HPM_PWM_ENABLED
	return pwm_hpmicro_v1_init(dev);
#elif CONFIG_DT_HAS_HPMICRO_HPM_PWMV2_ENABLED  
	return pwm_hpmicro_v2_init(dev);
#endif
}

static const struct pwm_driver_api pwm_hpmicro_driver_api = {
	.set_cycles = hpmicro_pwm_set_cycles,
	.get_cycles_per_sec = hpmicro_pwm_get_cycles_per_sec,
};

#define PWM_DEVICE_INIT_HPMICRO(n)			  \
	static struct pwm_hpmicro_data pwm_hpmicro_data_##n;		  \
	PINCTRL_DT_INST_DEFINE(n);					  \
									  \
	static const struct pwm_hpmicro_config pwm_hpmicro_config_##n = {     \
		.base = (HPM_PWM_BASE_TYPE *)DT_INST_REG_ADDR(n),	  \
		.clock_name = DT_INST_CLOCKS_CELL(n, name),		\
		.clock_src = (clk_src_t)DT_INST_CLOCKS_CELL(n, src),	\
		.clock_div = DT_INST_CLOCKS_CELL(n, div),		\
		.pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),		  \
		.period = DT_INST_PROP(n, period_init),			\
		.dead_zone_in_half_cycle = DT_INST_PROP(n, dead_zone_in_half_cycle),	\
	};								  \
									  \
	DEVICE_DT_INST_DEFINE(n,					  \
			    pwm_hpmicro_init,				  \
			    NULL,					  \
			    &pwm_hpmicro_data_##n,			  \
			    &pwm_hpmicro_config_##n,			  \
			    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,\
			    &pwm_hpmicro_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PWM_DEVICE_INIT_HPMICRO)
