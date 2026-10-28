/* SPDX-License-Identifier: Apache-2.0 */
#define DT_DRV_COMPAT hpmicro_hpm_esc

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/irq.h>
#include <zephyr/sys/util.h>
#include <string.h>
#include <zephyr/drivers/ethercat/hpm_esc.h>
#include <errno.h>
#include <hpm_soc.h>
#include <hpm_clock_drv.h>
#include <hpm_esc_drv.h>

struct hpm_esc_config {
	uintptr_t base;
	const struct device *clock_dev;
	uint32_t clock_name;
	uint32_t clock_src;
	uint32_t clock_div;
	const struct pinctrl_dev_config *pincfg;
	struct gpio_dt_spec phy_reset;
	void (*irq_config)(const struct device *dev);
	uint32_t irqs[HPM_ESC_IRQ_COUNT];
	uint8_t phy_addr_offset;
	uint8_t link0_ctrl_index;
	uint8_t link1_ctrl_index;
	uint8_t link2_ctrl_index;
	uint8_t run_led_ctrl_index;
	uint8_t error_led_ctrl_index;
	bool port1_enabled;
	bool port2_enabled;
	bool port0_disabled;
	bool software_link0;
	bool software_link1;
	bool link0_invert;
	bool link1_invert;
	bool link2_invert;
	bool run_error_leds;
};

struct hpm_esc_data {
	hpm_esc_irq_callback_t callbacks[HPM_ESC_IRQ_COUNT];
	void *callback_data[HPM_ESC_IRQ_COUNT];
};

static uintptr_t esc_base(const struct device *dev)
{
	const struct hpm_esc_config *cfg = dev->config;
	return cfg->base;
}

#if defined(CONFIG_HPM_ESC_PDRAM_WORD_ACCESS)
/* PDRAM occupies [0x1000, 0x10000). Do not widen register reads, pad across
 * SyncManager boundaries, or touch the first/last byte more than once.
 * memcpy on the CPU side permits unaligned SSC buffers without unaligned MMIO.
 */
static bool esc_pdram_word_access(uint32_t offset, size_t len)
{
	return offset >= 0x1000U && offset < 0x10000U &&
		len >= 8U && len <= 0x10000U - offset &&
		(offset & 3U) == 0U && (len & 3U) == 0U;
}

static void esc_read_words(uintptr_t addr, void *buf, size_t len)
{
	if (((uintptr_t)buf & 3U) == 0U) {
		/* Explicit builtin keeps four-byte copies inline even with -fno-builtin.
		 * memcpy preserves the effective type of byte-array SSC buffers.
		 */
		for (size_t pos = 0; pos < len; pos += 4U) {
			uint32_t word = *(volatile uint32_t *)(addr + pos);
			__builtin_memcpy(__builtin_assume_aligned((uint8_t *)buf + pos, 4),
					 &word, sizeof(word));
		}
	} else {
		for (size_t pos = 0; pos < len; pos += 4U) {
			uint32_t word = *(volatile uint32_t *)(addr + pos);
			memcpy((uint8_t *)buf + pos, &word, sizeof(word));
		}
	}
}

static void esc_write_words(uintptr_t addr, const void *buf, size_t len)
{
	if (((uintptr_t)buf & 3U) == 0U) {
		for (size_t pos = 0; pos < len; pos += 4U) {
			uint32_t word;
			__builtin_memcpy(&word,
				__builtin_assume_aligned((const uint8_t *)buf + pos, 4), sizeof(word));
			*(volatile uint32_t *)(addr + pos) = word;
		}
	} else {
		for (size_t pos = 0; pos < len; pos += 4U) {
			uint32_t word;
			memcpy(&word, (const uint8_t *)buf + pos, sizeof(word));
			*(volatile uint32_t *)(addr + pos) = word;
		}
	}
}
#endif

static void esc_read_block(const struct device *dev, uint32_t offset, void *buf, size_t len)
{
	const struct hpm_esc_config *cfg = dev->config;
	uintptr_t addr = cfg->base + offset;

#if defined(CONFIG_HPM_ESC_PDRAM_WORD_ACCESS)
	if (esc_pdram_word_access(offset, len)) {
		esc_read_words(addr, buf, len);
		return;
	}
#endif

	#if defined(HPM_IP_FEATURE_ESC_BYTE_READ) && HPM_IP_FEATURE_ESC_BYTE_READ
	for (size_t pos = 0; pos < len; pos++) {
		((uint8_t *)buf)[pos] = *(volatile uint8_t *)(addr + pos);
	}
	#else
	/* HPM6E ESC block reads must use aligned 32-bit transactions. */
	for (size_t pos = 0; pos < len; pos += 4) {
		uint32_t word = *(volatile uint32_t *)(addr + pos);
		memcpy((uint8_t *)buf + pos, &word, MIN((size_t)4, len - pos));
	}
	#endif
}

static void esc_read(const struct device *dev, uint32_t offset, void *buf, size_t len)
{
	const struct hpm_esc_config *cfg = dev->config;
	uintptr_t addr = cfg->base + offset;

	if (len == 1) {
		*(uint8_t *)buf = *(volatile uint8_t *)addr;
	} else if (len == 2) {
		uint16_t word = *(volatile uint16_t *)addr;
		memcpy(buf, &word, sizeof(word));
	} else if (len == 4) {
		uint32_t word = *(volatile uint32_t *)addr;
		memcpy(buf, &word, sizeof(word));
	} else {
		esc_read_block(dev, offset, buf, len);
	}
}

static void esc_write(const struct device *dev, uint32_t offset,
		      const void *buf, size_t len)
{
	const struct hpm_esc_config *cfg = dev->config;
	uintptr_t addr = cfg->base + offset;

	if (len == 1) {
		*(volatile uint8_t *)addr = *(const uint8_t *)buf;
	} else if (len == 2) {
		uint16_t word;
		memcpy(&word, buf, sizeof(word));
		*(volatile uint16_t *)addr = word;
	} else if (len == 4) {
		uint32_t word;
		memcpy(&word, buf, sizeof(word));
		*(volatile uint32_t *)addr = word;
	} else {
#if defined(CONFIG_HPM_ESC_PDRAM_WORD_ACCESS)
		if (esc_pdram_word_access(offset, len)) {
			esc_write_words(addr, buf, len);
			return;
		}
#endif
		for (size_t i = 0; i < len; i++) {
			*(volatile uint8_t *)(addr + i) = ((const uint8_t *)buf)[i];
		}
	}
}

static void esc_set_irq_callback(const struct device *dev, enum hpm_esc_irq irq,
				 hpm_esc_irq_callback_t cb, void *user_data)
{
	struct hpm_esc_data *data = dev->data;

	__ASSERT(irq >= 0 && irq < HPM_ESC_IRQ_COUNT, "invalid ESC IRQ");
	data->callbacks[irq] = cb;
	data->callback_data[irq] = user_data;
}

static void esc_irq_enable(const struct device *dev, bool enable)
{
	const struct hpm_esc_config *cfg = dev->config;

	for (size_t i = 0; i < HPM_ESC_IRQ_COUNT; i++) {
		if (enable) {
			irq_enable(cfg->irqs[i]);
		} else {
			irq_disable(cfg->irqs[i]);
		}
	}
}

static void esc_phy_reset(const struct device *dev, bool asserted)
{
	const struct hpm_esc_config *cfg = dev->config;

	if (cfg->phy_reset.port != NULL) {
		gpio_pin_set_dt(&cfg->phy_reset, asserted ? 1 : 0);
	}
}

static void esc_configure_eeprom(const struct device *dev, bool emulation,
				 bool size_over_16kbit)
{
	esc_eeprom_clock_config_t settings = {
		.core_clock_en = true,
		.phy_refclk_en = true,
		.eeprom_emulation = emulation,
		.eeprom_size_over_16kbit = size_over_16kbit,
	};

	esc_config_eeprom_and_clock((ESC_Type *)esc_base(dev), &settings);
}

static int esc_check_eeprom(const struct device *dev)
{
	hpm_stat_t status = esc_check_eeprom_loading((ESC_Type *)esc_base(dev));

	if (status == status_success) {
		return 0;
	}
	if (status == status_esc_eeprom_checksum_error) {
		return -EBADMSG;
	}
	return -EIO;
}

static void esc_configure_phy(const struct device *dev)
{
	const struct hpm_esc_config *cfg = dev->config;

	esc_set_phy_offset((ESC_Type *)cfg->base, cfg->phy_addr_offset);
}

static void esc_configure_signals(const struct device *dev)
{
	const struct hpm_esc_config *cfg = dev->config;
	ESC_Type *esc = (ESC_Type *)cfg->base;

#if defined(HPM_IP_FEATURE_ESC_SYNC_IRQ_MASK) && HPM_IP_FEATURE_ESC_SYNC_IRQ_MASK
	esc_enable_sync_irq_to_pdi_irq(esc, false, false);
#endif
	if (!cfg->port0_disabled && !cfg->software_link0) {
		esc_config_ctrl_signal_function(esc, cfg->link0_ctrl_index,
						esc_ctrl_signal_func_alt_nmii_link0, cfg->link0_invert);
	}
	if (cfg->port1_enabled && !cfg->software_link1) {
		esc_config_ctrl_signal_function(esc, cfg->link1_ctrl_index,
					esc_ctrl_signal_func_alt_nmii_link1, cfg->link1_invert);
	}
	if (cfg->port2_enabled) {
		esc_config_ctrl_signal_function(esc, cfg->link2_ctrl_index,
					esc_ctrl_signal_func_alt_nmii_link2, cfg->link2_invert);
	}
	esc_config_nmii_link_source(esc, !cfg->port0_disabled && !cfg->software_link0,
				    cfg->port1_enabled && !cfg->software_link1,
				    cfg->port2_enabled);
#if defined(HPM_IP_FEATURE_ESC_PORT_DIS) && HPM_IP_FEATURE_ESC_PORT_DIS
	esc_change_port_description(esc, cfg->port0_disabled,
				    !cfg->port1_enabled, !cfg->port2_enabled);
#endif
	if (cfg->run_error_leds) {
		esc_config_ctrl_signal_function(esc, cfg->run_led_ctrl_index,
					esc_ctrl_signal_func_alt_led_run, false);
		esc_config_ctrl_signal_function(esc, cfg->error_led_ctrl_index,
					esc_ctrl_signal_func_alt_led_err, false);
	}
}

static void esc_start(const struct device *dev)
{
	esc_enable_irq((ESC_Type *)esc_base(dev), esc_irq_mask_all);
	esc_irq_enable(dev, true);
}

static int esc_mdio_read_z(const struct device *dev, uint8_t phy_addr,
			   uint8_t reg_addr, uint16_t *value)
{
	return esc_mdio_read((ESC_Type *)esc_base(dev), phy_addr, reg_addr, value) ==
		status_success ? 0 : -EIO;
}

static int esc_mdio_write_z(const struct device *dev, uint8_t phy_addr,
			    uint8_t reg_addr, uint16_t value)
{
	return esc_mdio_write((ESC_Type *)esc_base(dev), phy_addr, reg_addr, value) ==
		status_success ? 0 : -EIO;
}

static void esc_set_link_state_z(const struct device *dev, unsigned int port, bool up)
{
	ESC_Type *esc = (ESC_Type *)esc_base(dev);
	uint32_t mask;

	switch (port) {
	case 0: mask = ESC_GPR_CFG2_NMII_LINK0_GPR_MASK; break;
	case 1: mask = ESC_GPR_CFG2_NMII_LINK1_GPR_MASK; break;
	case 2: mask = ESC_GPR_CFG2_NMII_LINK2_GPR_MASK; break;
	default: return;
	}
	if (up) {
		esc->GPR_CFG2 &= ~mask;
	} else {
		esc->GPR_CFG2 |= mask;
	}
}

static void esc_isr(const void *arg, enum hpm_esc_irq irq)
{
	const struct device *dev = arg;
	struct hpm_esc_data *data = dev->data;

	if (data->callbacks[irq] != NULL) {
		data->callbacks[irq](dev, data->callback_data[irq]);
	}
}

#define ESC_ISR(n) \
	static void esc_isr_##n(const void *arg) { esc_isr(arg, n); }

ESC_ISR(0)
ESC_ISR(1)
ESC_ISR(2)
ESC_ISR(3)

static int esc_init(const struct device *dev)
{
	const struct hpm_esc_config *cfg = dev->config;
	int ret;

	if (!device_is_ready(cfg->clock_dev) ||
	    (cfg->phy_reset.port != NULL && !gpio_is_ready_dt(&cfg->phy_reset))) {
		return -ENODEV;
	}
	/* Leave ESC0's clock source at its reset/preset default, which already
	 * provides the required 100 MHz. Do not switch it here: on HPM5E31
	 * clock_esc0 is in the COMMON clock group, so clock_set_source_divider()
	 * performs a real mux switch and busy-waits on PLL2, which is not locked
	 * until some node selects it — this hangs the CPU during POST_KERNEL.
	 * The upstream SDK's board_init_ethercat() likewise only does
	 * clock_add_to_group(clock_esc0, 0); rely on clock_control_on() ->
	 * clock_add_to_group() below to enable the clock.
	 */
	ret = clock_control_on(cfg->clock_dev, (clock_control_subsys_t)&cfg->clock_name);
	if (ret != 0) {
		return ret;
	}
	ret = pinctrl_apply_state(cfg->pincfg, PINCTRL_STATE_DEFAULT);
	if (ret != 0) {
		return ret;
	}
	if (cfg->phy_reset.port != NULL) {
		ret = gpio_pin_configure_dt(&cfg->phy_reset, GPIO_OUTPUT_ACTIVE);
		if (ret != 0) {
			return ret;
		}
	}
	cfg->irq_config(dev);
	return 0;
}

static const struct hpm_esc_driver_api esc_api = {
	.base = esc_base,
	.read = esc_read,
	.read_block = esc_read_block,
	.write = esc_write,
	.set_irq_callback = esc_set_irq_callback,
	.set_irq_state = esc_irq_enable,
	.phy_reset = esc_phy_reset,
	.configure_eeprom = esc_configure_eeprom,
	.check_eeprom = esc_check_eeprom,
	.configure_phy = esc_configure_phy,
	.configure_signals = esc_configure_signals,
	.start = esc_start,
	.mdio_read = esc_mdio_read_z,
	.mdio_write = esc_mdio_write_z,
	.set_link_state = esc_set_link_state_z,
};

#define ESC_IRQ(n, idx) DT_IRQN_BY_IDX(DT_DRV_INST(n), idx)
#define ESC_PRIO(n, idx) DT_INST_IRQ_BY_IDX(n, idx, priority)

#define HPM_ESC_DEFINE(n) \
	PINCTRL_DT_INST_DEFINE(n); \
	static void esc_irq_config_##n(const struct device *dev) \
	{ \
		ARG_UNUSED(dev); \
		IRQ_CONNECT(ESC_IRQ(n, 0), ESC_PRIO(n, 0), esc_isr_0, DEVICE_DT_INST_GET(n), 0); \
		IRQ_CONNECT(ESC_IRQ(n, 1), ESC_PRIO(n, 1), esc_isr_1, DEVICE_DT_INST_GET(n), 0); \
		IRQ_CONNECT(ESC_IRQ(n, 2), ESC_PRIO(n, 2), esc_isr_2, DEVICE_DT_INST_GET(n), 0); \
		IRQ_CONNECT(ESC_IRQ(n, 3), ESC_PRIO(n, 3), esc_isr_3, DEVICE_DT_INST_GET(n), 0); \
	} \
	static const struct hpm_esc_config esc_config_##n = { \
		.base = DT_INST_REG_ADDR(n), \
		.clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)), \
		.clock_name = DT_INST_CLOCKS_CELL(n, name), \
		.clock_src = DT_INST_CLOCKS_CELL(n, src), \
		.clock_div = DT_INST_CLOCKS_CELL(n, div), \
		.pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n), \
		.phy_reset = GPIO_DT_SPEC_INST_GET_OR(n, phy_reset_gpios, {0}), \
		.irq_config = esc_irq_config_##n, \
		.irqs = {ESC_IRQ(n, 0), ESC_IRQ(n, 1), ESC_IRQ(n, 2), ESC_IRQ(n, 3)}, \
		.phy_addr_offset = DT_INST_PROP_OR(n, phy_addr_offset, 0), \
		.link0_ctrl_index = DT_INST_PROP(n, link0_ctrl_index), \
		.link1_ctrl_index = DT_INST_PROP_OR(n, link1_ctrl_index, 0), \
		.link2_ctrl_index = DT_INST_PROP_OR(n, link2_ctrl_index, 0), \
		.run_led_ctrl_index = DT_INST_PROP_OR(n, run_led_ctrl_index, 0), \
		.error_led_ctrl_index = DT_INST_PROP_OR(n, error_led_ctrl_index, 0), \
		.port1_enabled = DT_INST_PROP(n, port1_enabled), \
		.port2_enabled = DT_INST_PROP(n, port2_enabled), \
		.port0_disabled = DT_INST_PROP(n, port0_disabled), \
		.software_link0 = DT_INST_PROP(n, software_link0), \
		.software_link1 = DT_INST_PROP(n, software_link1), \
		.link0_invert = DT_INST_PROP(n, link0_invert), \
		.link1_invert = DT_INST_PROP(n, link1_invert), \
		.link2_invert = DT_INST_PROP(n, link2_invert), \
		.run_error_leds = DT_INST_PROP(n, run_error_leds), \
	}; \
	static struct hpm_esc_data esc_data_##n; \
	DEVICE_DT_INST_DEFINE(n, esc_init, NULL, &esc_data_##n, &esc_config_##n, \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &esc_api);

DT_INST_FOREACH_STATUS_OKAY(HPM_ESC_DEFINE)
