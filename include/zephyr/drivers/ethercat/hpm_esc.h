/* SPDX-License-Identifier: Apache-2.0 */
#ifndef HPM_ESC_ZEPHYR_H_
#define HPM_ESC_ZEPHYR_H_

#include <zephyr/device.h>
#include <stdint.h>
#include <stddef.h>

enum hpm_esc_irq {
	HPM_ESC_IRQ_PDI,
	HPM_ESC_IRQ_SYNC0,
	HPM_ESC_IRQ_SYNC1,
	HPM_ESC_IRQ_RESET,
	HPM_ESC_IRQ_COUNT,
};

typedef void (*hpm_esc_irq_callback_t)(const struct device *dev, void *user_data);

struct hpm_esc_driver_api {
	uintptr_t (*base)(const struct device *dev);
	void (*read)(const struct device *dev, uint32_t offset, void *buf, size_t len);
	void (*read_block)(const struct device *dev, uint32_t offset, void *buf, size_t len);
	void (*write)(const struct device *dev, uint32_t offset, const void *buf, size_t len);
	void (*set_irq_callback)(const struct device *dev, enum hpm_esc_irq irq,
				 hpm_esc_irq_callback_t cb, void *user_data);
	void (*set_irq_state)(const struct device *dev, bool enable);
	void (*phy_reset)(const struct device *dev, bool asserted);
	void (*configure_eeprom)(const struct device *dev, bool emulation,
				 bool size_over_16kbit);
	int (*check_eeprom)(const struct device *dev);
	void (*configure_phy)(const struct device *dev);
	void (*configure_signals)(const struct device *dev);
	void (*start)(const struct device *dev);
	int (*mdio_read)(const struct device *dev, uint8_t phy_addr,
			 uint8_t reg_addr, uint16_t *value);
	int (*mdio_write)(const struct device *dev, uint8_t phy_addr,
			  uint8_t reg_addr, uint16_t value);
	void (*set_link_state)(const struct device *dev, unsigned int port, bool up);
};

static inline const struct hpm_esc_driver_api *hpm_esc_api(const struct device *dev)
{
	return (const struct hpm_esc_driver_api *)dev->api;
}

static inline uintptr_t hpm_esc_base(const struct device *dev)
{
	return hpm_esc_api(dev)->base(dev);
}

static inline void hpm_esc_read(const struct device *dev, uint32_t offset,
				void *buf, size_t len)
{
	hpm_esc_api(dev)->read(dev, offset, buf, len);
}

static inline void hpm_esc_write(const struct device *dev, uint32_t offset,
				 const void *buf, size_t len)
{
	hpm_esc_api(dev)->write(dev, offset, buf, len);
}

static inline void hpm_esc_read_block(const struct device *dev, uint32_t offset,
				      void *buf, size_t len)
{
	hpm_esc_api(dev)->read_block(dev, offset, buf, len);
}

static inline void hpm_esc_set_irq_callback(const struct device *dev,
				 enum hpm_esc_irq irq, hpm_esc_irq_callback_t cb,
				 void *user_data)
{
	hpm_esc_api(dev)->set_irq_callback(dev, irq, cb, user_data);
}

static inline void hpm_esc_irq_enable(const struct device *dev, bool enable)
{
	hpm_esc_api(dev)->set_irq_state(dev, enable);
}

static inline void hpm_esc_phy_reset(const struct device *dev, bool asserted)
{
	hpm_esc_api(dev)->phy_reset(dev, asserted);
}

static inline void hpm_esc_configure_eeprom(const struct device *dev, bool emulation,
					    bool size_over_16kbit)
{
	hpm_esc_api(dev)->configure_eeprom(dev, emulation, size_over_16kbit);
}

/* Returns 0, -EBADMSG for a loaded EEPROM with bad checksum, or -EIO. */
static inline int hpm_esc_check_eeprom(const struct device *dev)
{
	return hpm_esc_api(dev)->check_eeprom(dev);
}

static inline void hpm_esc_configure_phy(const struct device *dev)
{
	hpm_esc_api(dev)->configure_phy(dev);
}

static inline void hpm_esc_configure_signals(const struct device *dev)
{
	hpm_esc_api(dev)->configure_signals(dev);
}

static inline void hpm_esc_start(const struct device *dev)
{
	hpm_esc_api(dev)->start(dev);
}

static inline int hpm_esc_mdio_read(const struct device *dev, uint8_t phy_addr,
				    uint8_t reg_addr, uint16_t *value)
{
	return hpm_esc_api(dev)->mdio_read(dev, phy_addr, reg_addr, value);
}

static inline int hpm_esc_mdio_write(const struct device *dev, uint8_t phy_addr,
				     uint8_t reg_addr, uint16_t value)
{
	return hpm_esc_api(dev)->mdio_write(dev, phy_addr, reg_addr, value);
}

static inline void hpm_esc_set_link_state(const struct device *dev,
					 unsigned int port, bool up)
{
	hpm_esc_api(dev)->set_link_state(dev, port, up);
}

#endif
