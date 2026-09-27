/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_ETHERCAT_ETHERCAT_HPMICRO_H_
#define ZEPHYR_INCLUDE_DRIVERS_ETHERCAT_ETHERCAT_HPMICRO_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

enum hpm_ethercat_irq {
	HPM_ETHERCAT_IRQ_PDI,
	HPM_ETHERCAT_IRQ_SYNC0,
	HPM_ETHERCAT_IRQ_SYNC1,
	HPM_ETHERCAT_IRQ_RESET,
};

typedef void (*hpm_ethercat_irq_callback_t)(const struct device *dev, void *user_data);

struct hpm_ethercat_callbacks {
	hpm_ethercat_irq_callback_t pdi;
	hpm_ethercat_irq_callback_t sync0;
	hpm_ethercat_irq_callback_t sync1;
	hpm_ethercat_irq_callback_t reset;
	void *user_data;
};

/* EEPROM callbacks use EtherCAT word addresses and little-endian 16-bit data. */
struct hpm_ethercat_eeprom_ops {
	int (*read)(uint32_t word_addr, uint16_t *data, size_t word_count, void *user_data);
	int (*write)(uint32_t word_addr, uint16_t data, void *user_data);
	int (*reload)(uint32_t *reload_data, void *user_data);
	void *user_data;
};

int hpm_ethercat_set_callbacks(const struct device *dev,
			       const struct hpm_ethercat_callbacks *callbacks);
int hpm_ethercat_set_eeprom_ops(const struct device *dev,
				const struct hpm_ethercat_eeprom_ops *ops);
int hpm_ethercat_start(const struct device *dev);
void hpm_ethercat_stop(const struct device *dev);
void hpm_ethercat_irq_enable(const struct device *dev, enum hpm_ethercat_irq irq,
			     bool enable);
uint16_t hpm_ethercat_get_al_event(const struct device *dev);
int hpm_ethercat_read(const struct device *dev, uint16_t address, void *data, size_t len);
int hpm_ethercat_write(const struct device *dev, uint16_t address, const void *data, size_t len);
int hpm_ethercat_process_eeprom(const struct device *dev);

static inline uint32_t hpm_ethercat_timer_get(void)
{
	return k_uptime_get_32();
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_ETHERCAT_ETHERCAT_HPMICRO_H_ */
