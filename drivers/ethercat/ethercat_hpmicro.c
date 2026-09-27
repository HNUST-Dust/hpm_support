/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 */

#define DT_DRV_COMPAT hpmicro_hpm_esc

#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#include <hpm_clock_drv.h>
#include <hpm_esc_drv.h>

#include <drivers/ethercat/ethercat_hpmicro.h>

LOG_MODULE_REGISTER(ethercat_hpmicro, CONFIG_LOG_DEFAULT_LEVEL);

#define ESC_AL_EVENT_OFFSET 0x0220U
#define ESC_DC_SYNC_STATUS  0x098EU
#define JL11X1_PAGE_SELECT  31U
#define JL11X1_WOL_PAGE     7U
#define JL11X1_WOL_REG      19U

struct hpm_ethercat_config {
	ESC_Type *base;
	uintptr_t esc_mem;
	size_t esc_mem_size;
	const struct pinctrl_dev_config *pincfg;
	struct gpio_dt_spec reset_gpio;
	uint8_t phy_offset;
	uint8_t port_count;
	uint8_t phy_addresses[3];
	uint8_t link_ctrl_indices[3];
	bool link_inverted[3];
	uint8_t link_from_io_mask;
	bool eeprom_emulation;
	bool eeprom_large;
	uint8_t eeprom_read_size;
	bool configure_jl1111;
	void (*irq_config)(const struct device *dev);
};

struct hpm_ethercat_data {
	struct hpm_ethercat_callbacks callbacks;
	struct hpm_ethercat_eeprom_ops eeprom;
	bool started;
};

static int phy_write(ESC_Type *esc, uint8_t phy, uint8_t reg, uint16_t value)
{
	return esc_mdio_write(esc, phy, reg, value) == status_success ? 0 : -EIO;
}

static int configure_phy(const struct hpm_ethercat_config *cfg, uint8_t phy)
{
	int ret;

	ret = phy_write(cfg->base, phy, JL11X1_PAGE_SELECT, JL11X1_WOL_PAGE);
	if (ret == 0) {
		ret = phy_write(cfg->base, phy, JL11X1_WOL_REG, 0U);
	}
	if (ret == 0) {
		ret = phy_write(cfg->base, phy, JL11X1_PAGE_SELECT, 0U);
	}
	if (ret == 0 && (phy + cfg->phy_offset) != 0U) {
		ret = phy_write(cfg->base, phy, JL11X1_PAGE_SELECT, 128U);
		if (ret == 0) {
			ret = phy_write(cfg->base, phy, 19U,
					(uint16_t)(((phy + cfg->phy_offset) << 5) | 0x1FU));
		}
		if (ret == 0) {
			ret = phy_write(cfg->base, phy, JL11X1_PAGE_SELECT, 0U);
		}
	}
	return ret;
}

static void invoke_callback(const struct device *dev, enum hpm_ethercat_irq irq)
{
	struct hpm_ethercat_data *data = dev->data;
	hpm_ethercat_irq_callback_t callback = NULL;

	switch (irq) {
	case HPM_ETHERCAT_IRQ_PDI:
		callback = data->callbacks.pdi;
		break;
	case HPM_ETHERCAT_IRQ_SYNC0:
		callback = data->callbacks.sync0;
		break;
	case HPM_ETHERCAT_IRQ_SYNC1:
		callback = data->callbacks.sync1;
		break;
	case HPM_ETHERCAT_IRQ_RESET:
		callback = data->callbacks.reset;
		break;
	}
	if (callback != NULL) {
		callback(dev, data->callbacks.user_data);
	}
}

static void pdi_isr(const void *arg) { invoke_callback(arg, HPM_ETHERCAT_IRQ_PDI); }
static void reset_isr(const void *arg) { invoke_callback(arg, HPM_ETHERCAT_IRQ_RESET); }

static void sync0_isr(const void *arg)
{
	const struct device *dev = arg;
	const struct hpm_ethercat_config *cfg = dev->config;

	(void)sys_read32(cfg->esc_mem + ESC_DC_SYNC_STATUS);
	invoke_callback(dev, HPM_ETHERCAT_IRQ_SYNC0);
}

static void sync1_isr(const void *arg)
{
	const struct device *dev = arg;
	const struct hpm_ethercat_config *cfg = dev->config;

	(void)sys_read32(cfg->esc_mem + ESC_DC_SYNC_STATUS);
	invoke_callback(dev, HPM_ETHERCAT_IRQ_SYNC1);
}

int hpm_ethercat_set_callbacks(const struct device *dev,
			       const struct hpm_ethercat_callbacks *callbacks)
{
	struct hpm_ethercat_data *data = dev->data;

	if (callbacks == NULL) {
		memset(&data->callbacks, 0, sizeof(data->callbacks));
	} else {
		data->callbacks = *callbacks;
	}
	return 0;
}

int hpm_ethercat_set_eeprom_ops(const struct device *dev,
				const struct hpm_ethercat_eeprom_ops *ops)
{
	const struct hpm_ethercat_config *cfg = dev->config;
	struct hpm_ethercat_data *data = dev->data;

	if (!cfg->eeprom_emulation) {
		return -ENOTSUP;
	}
	if (ops == NULL || ops->read == NULL || ops->write == NULL || ops->reload == NULL) {
		return -EINVAL;
	}
	data->eeprom = *ops;
	return 0;
}

int hpm_ethercat_process_eeprom(const struct device *dev)
{
	const struct hpm_ethercat_config *cfg = dev->config;
	struct hpm_ethercat_data *data = dev->data;
	ESC_Type *esc = cfg->base;
	uint8_t cmd;
	uint32_t address;
	uint16_t words[4] = { 0U };
	uint16_t word;
	uint64_t read_data = 0U;
	uint32_t reload_data;
	int ret;

	if (!cfg->eeprom_emulation || data->eeprom.read == NULL) {
		return -ENOTSUP;
	}
	if ((esc->EEPROM_CTRL_STAT & ESC_EEPROM_CTRL_STAT_BUSY_MASK) == 0U) {
		return 0;
	}
	cmd = esc_get_eeprom_cmd(esc);
	address = esc_get_eeprom_word_address(esc);
	switch (cmd) {
	case esc_eeprom_read_cmd:
		ret = data->eeprom.read(address, words, cfg->eeprom_read_size / sizeof(uint16_t),
					data->eeprom.user_data);
		if (ret == 0) {
			for (uint8_t i = 0U; i < cfg->eeprom_read_size / sizeof(uint16_t); i++) {
				read_data |= (uint64_t)words[i] << (i * 16U);
			}
			esc_write_eeprom_data(esc, read_data);
		}
		break;
	case esc_eeprom_write_cmd:
		word = (uint16_t)esc_read_eeprom_data(esc);
		ret = data->eeprom.write(address, word, data->eeprom.user_data);
		break;
	case esc_eeprom_reload_cmd:
		ret = data->eeprom.reload(&reload_data, data->eeprom.user_data);
		if (ret == 0) {
			esc_write_eeprom_data(esc, reload_data);
		}
		break;
	default:
		return 0;
	}
	esc_eeprom_emulation_ack(esc, cmd, ret != 0, false);
	return ret;
}

int hpm_ethercat_start(const struct device *dev)
{
	const struct hpm_ethercat_config *cfg = dev->config;
	struct hpm_ethercat_data *data = dev->data;
	esc_eeprom_clock_config_t clock_cfg = {
		.core_clock_en = true,
		.phy_refclk_en = true,
		.eeprom_emulation = cfg->eeprom_emulation,
		.eeprom_size_over_16kbit = cfg->eeprom_large,
	};
	int ret;

	if (data->started) {
		return -EALREADY;
	}
	if (cfg->eeprom_emulation && data->eeprom.read == NULL) {
		return -EINVAL;
	}
	esc_config_eeprom_and_clock(cfg->base, &clock_cfg);
	if (cfg->eeprom_emulation) {
		ret = hpm_ethercat_process_eeprom(dev);
		if (ret != 0 && ret != -ENOTSUP) {
			return ret;
		}
	}
	if (esc_check_eeprom_loading(cfg->base) != status_success) {
		LOG_WRN("ESC EEPROM did not load cleanly; check its image and wiring");
	}

	ret = gpio_pin_set_dt(&cfg->reset_gpio, 1);
	if (ret != 0) {
		return ret;
	}
	k_msleep(1);
	ret = gpio_pin_set_dt(&cfg->reset_gpio, 0);
	if (ret != 0) {
		return ret;
	}
	k_msleep(5);

	esc_set_phy_offset(cfg->base, cfg->phy_offset);
	for (uint8_t i = 0U; i < cfg->port_count; i++) {
		if (cfg->configure_jl1111) {
			ret = configure_phy(cfg, cfg->phy_addresses[i]);
			if (ret != 0) {
				return ret;
			}
		}
		if ((cfg->link_from_io_mask & BIT(i)) != 0U) {
			esc_config_ctrl_signal_function(cfg->base, cfg->link_ctrl_indices[i],
						       (esc_ctrl_signal_function_t)i,
						       cfg->link_inverted[i]);
		}
	}
	esc_config_nmii_link_source(cfg->base, (cfg->link_from_io_mask & BIT(0)) != 0U,
				    (cfg->link_from_io_mask & BIT(1)) != 0U,
				    (cfg->link_from_io_mask & BIT(2)) != 0U);
#if defined(HPM_IP_FEATURE_ESC_SYNC_IRQ_MASK) && HPM_IP_FEATURE_ESC_SYNC_IRQ_MASK
	esc_enable_sync_irq_to_pdi_irq(cfg->base, false, false);
#endif
	esc_enable_irq(cfg->base, esc_irq_mask_all);
	for (enum hpm_ethercat_irq irq = HPM_ETHERCAT_IRQ_PDI;
	     irq <= HPM_ETHERCAT_IRQ_RESET; irq++) {
		hpm_ethercat_irq_enable(dev, irq, true);
	}
	data->started = true;
	return 0;
}

void hpm_ethercat_stop(const struct device *dev)
{
	const struct hpm_ethercat_config *cfg = dev->config;
	struct hpm_ethercat_data *data = dev->data;

	for (enum hpm_ethercat_irq irq = HPM_ETHERCAT_IRQ_PDI;
	     irq <= HPM_ETHERCAT_IRQ_RESET; irq++) {
		hpm_ethercat_irq_enable(dev, irq, false);
	}
	esc_enable_irq(cfg->base, esc_irq_mask_none);
	data->started = false;
}

void hpm_ethercat_irq_enable(const struct device *dev, enum hpm_ethercat_irq irq, bool enable)
{
	ARG_UNUSED(dev);
	unsigned int line;

	switch (irq) {
	case HPM_ETHERCAT_IRQ_PDI:
		line = DT_INST_IRQN_BY_IDX(0, 0);
		break;
	case HPM_ETHERCAT_IRQ_SYNC0:
		line = DT_INST_IRQN_BY_IDX(0, 1);
		break;
	case HPM_ETHERCAT_IRQ_SYNC1:
		line = DT_INST_IRQN_BY_IDX(0, 2);
		break;
	case HPM_ETHERCAT_IRQ_RESET:
		line = DT_INST_IRQN_BY_IDX(0, 3);
		break;
	default:
		return;
	}
	if (enable) {
		irq_enable(line);
	} else {
		irq_disable(line);
	}
}

uint16_t hpm_ethercat_get_al_event(const struct device *dev)
{
	const struct hpm_ethercat_config *cfg = dev->config;

	return sys_read16(cfg->esc_mem + ESC_AL_EVENT_OFFSET);
}

int hpm_ethercat_read(const struct device *dev, uint16_t address, void *data, size_t len)
{
	const struct hpm_ethercat_config *cfg = dev->config;
	uint8_t *dest = data;
	volatile const uint8_t *src = (volatile const uint8_t *)(cfg->esc_mem + address);

	if (data == NULL || (size_t)address + len > cfg->esc_mem_size) {
		return -EINVAL;
	}
	for (size_t i = 0U; i < len; i++) {
		dest[i] = src[i];
	}
	return 0;
}

int hpm_ethercat_write(const struct device *dev, uint16_t address, const void *data, size_t len)
{
	const struct hpm_ethercat_config *cfg = dev->config;
	const uint8_t *src = data;
	volatile uint8_t *dest = (volatile uint8_t *)(cfg->esc_mem + address);

	if (data == NULL || (size_t)address + len > cfg->esc_mem_size) {
		return -EINVAL;
	}
	for (size_t i = 0U; i < len; i++) {
		dest[i] = src[i];
	}
	return 0;
}

static int hpm_ethercat_init(const struct device *dev)
{
	const struct hpm_ethercat_config *cfg = dev->config;
	int ret;

	ret = pinctrl_apply_state(cfg->pincfg, PINCTRL_STATE_DEFAULT);
	if (ret != 0) {
		return ret;
	}
	if (!gpio_is_ready_dt(&cfg->reset_gpio)) {
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&cfg->reset_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		return ret;
	}
	clock_add_to_group(clock_esc0, 0);
	cfg->irq_config(dev);
	return 0;
}

#define ARRAY_U8_ELEM(node_id, prop, idx) DT_PROP_BY_IDX(node_id, prop, idx)
#define ARRAY_BOOL_ELEM(node_id, prop, idx) (DT_PROP_BY_IDX(node_id, prop, idx) != 0)

PINCTRL_DT_INST_DEFINE(0);

static void hpm_ethercat_irq_config(const struct device *dev)
{
	IRQ_CONNECT(DT_INST_IRQN_BY_IDX(0, 0), DT_INST_IRQ_BY_NAME(0, pdi, priority),
		    pdi_isr, DEVICE_DT_INST_GET(0), 0);
	IRQ_CONNECT(DT_INST_IRQN_BY_IDX(0, 1), DT_INST_IRQ_BY_NAME(0, sync0, priority),
		    sync0_isr, DEVICE_DT_INST_GET(0), 0);
	IRQ_CONNECT(DT_INST_IRQN_BY_IDX(0, 2), DT_INST_IRQ_BY_NAME(0, sync1, priority),
		    sync1_isr, DEVICE_DT_INST_GET(0), 0);
	IRQ_CONNECT(DT_INST_IRQN_BY_IDX(0, 3), DT_INST_IRQ_BY_NAME(0, reset, priority),
		    reset_isr, DEVICE_DT_INST_GET(0), 0);
}

BUILD_ASSERT(DT_INST_PROP_LEN(0, phy_addresses) <= 3,
	     "HPM ESC supports at most three EtherCAT ports");
BUILD_ASSERT(DT_INST_PROP_LEN(0, phy_addresses) ==
	     DT_INST_PROP_LEN(0, nmii_link_ctrl_indices), "EtherCAT port arrays must match");
BUILD_ASSERT(DT_INST_PROP_LEN(0, phy_addresses) ==
	     DT_INST_PROP_LEN(0, nmii_link_inverted), "EtherCAT port arrays must match");

static const struct hpm_ethercat_config hpm_ethercat_config_0 = {
	.base = (ESC_Type *)DT_INST_REG_ADDR(0),
	.esc_mem = DT_INST_REG_ADDR(0),
	.esc_mem_size = DT_INST_REG_SIZE(0),
	.pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(0),
	.reset_gpio = GPIO_DT_SPEC_INST_GET(0, reset_gpios),
	.phy_offset = DT_INST_PROP(0, phy_address_offset),
	.port_count = DT_INST_PROP_LEN(0, phy_addresses),
	.phy_addresses = { DT_INST_FOREACH_PROP_ELEM_SEP(0, phy_addresses, ARRAY_U8_ELEM, (,)) },
	.link_ctrl_indices = { DT_INST_FOREACH_PROP_ELEM_SEP(0, nmii_link_ctrl_indices,
						      ARRAY_U8_ELEM, (,)) },
	.link_inverted = { DT_INST_FOREACH_PROP_ELEM_SEP(0, nmii_link_inverted,
						  ARRAY_BOOL_ELEM, (,)) },
	.link_from_io_mask = DT_INST_PROP(0, nmii_link_from_io_mask),
	.eeprom_emulation = DT_INST_PROP(0, eeprom_emulation),
	.eeprom_large = DT_INST_PROP(0, eeprom_size_over_16kbit),
	.eeprom_read_size = DT_INST_PROP(0, eeprom_read_size),
	.configure_jl1111 = DT_INST_PROP(0, configure_jl1111),
	.irq_config = hpm_ethercat_irq_config,
};

static struct hpm_ethercat_data hpm_ethercat_data_0;

DEVICE_DT_INST_DEFINE(0, hpm_ethercat_init, NULL, &hpm_ethercat_data_0,
		      &hpm_ethercat_config_0, POST_KERNEL,
		      CONFIG_ETHERCAT_HPMICRO_INIT_PRIORITY, NULL);
