/*
 * Copyright (c) 2025 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Application callbacks for the ESC Zephyr device and DIO setup.
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <errno.h>

#include "ecat_zephyr_port.h"
#include <zephyr/drivers/ethercat/hpm_esc.h>

static const struct device *const esc_dev = DEVICE_DT_GET(DT_NODELABEL(esc0));

/* ISRs implemented in hpm_ecat_hw.c */
void ecat_pdi_isr(void);
void ecat_sync0_isr(void);
void ecat_sync1_isr(void);
void ecat_reset_isr(void);

static void ecat_esc_irq_z(const struct device *dev, void *user_data)
{
	ARG_UNUSED(dev);
	switch ((uintptr_t)user_data) {
	case HPM_ESC_IRQ_PDI: ecat_pdi_isr(); break;
	case HPM_ESC_IRQ_SYNC0: ecat_sync0_isr(); break;
	case HPM_ESC_IRQ_SYNC1: ecat_sync1_isr(); break;
	case HPM_ESC_IRQ_RESET: ecat_reset_isr(); break;
	default: break;
	}
}

void ecat_irq_init(void)
{
	for (uintptr_t irq = 0; irq < HPM_ESC_IRQ_COUNT; irq++) {
		hpm_esc_set_irq_callback(esc_dev, irq, ecat_esc_irq_z, (void *)irq);
	}
}

/* required by hpm_ecat_hw.c, originally intc_m_enable_irq_with_priority() */
void ENABLE_ESC_INT(void)
{
	hpm_esc_irq_enable(esc_dev, true);
}

void DISABLE_ESC_INT(void)
{
	hpm_esc_irq_enable(esc_dev, false);
}

