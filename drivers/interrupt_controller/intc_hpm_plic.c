/*
 * Copyright (c) 2026 HPMicro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT hpmicro_hpm_plic

#include <zephyr/arch/cpu.h>
#include <zephyr/arch/riscv/csr.h>
#include <zephyr/device.h>
#include <zephyr/devicetree/interrupt_controller.h>
#include <zephyr/drivers/interrupt_controller/riscv_plic.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/sw_isr_table.h>

#include "sw_isr_common.h"

#define PLIC_CONTEXT_SIZE          0x1000U
#define PLIC_CONTEXT_CLAIM         0x04U
#define PLIC_CONTEXT_ENABLE_SIZE   0x80U
#define PLIC_REG_BITS              32U
#define PLIC_REG_MASK              BIT_MASK(LOG2(PLIC_REG_BITS))

typedef void (*hpm_plic_irq_config_func_t)(void);

struct hpm_plic_config {
	mem_addr_t prio;
	mem_addr_t irq_en;
	mem_addr_t context;
	uint32_t max_prio;
	uint32_t num_irqs;
	hpm_plic_irq_config_func_t irq_config_func;
	struct _isr_table_entry *isr_table;
};

/* HPM priority nesting is currently deliberately restricted to !SMP. */
static uint32_t active_irq;
static const struct device *active_dev;
#ifdef CONFIG_HPM_PLIC_PRIORITY_NESTING
static uint32_t plic_nesting_depth;
#endif

static inline uint32_t local_irq_to_reg_index(uint32_t local_irq)
{
	return local_irq >> LOG2(PLIC_REG_BITS);
}

static inline uint32_t local_irq_to_reg_offset(uint32_t local_irq)
{
	return local_irq_to_reg_index(local_irq) * sizeof(uint32_t);
}

static inline uint32_t first_context(uint32_t hartid)
{
	return hartid == 0U ? 0U : (hartid * 2U) - 1U;
}

static inline mem_addr_t context_enable_addr(const struct device *dev,
					     uint32_t cpu_num)
{
	const struct hpm_plic_config *config = dev->config;
	uint32_t hartid;

#if CONFIG_SMP
	hartid = _kernel.cpus[cpu_num].arch.hartid;
#else
	ARG_UNUSED(cpu_num);
	hartid = arch_proc_id();
#endif
	return config->irq_en + first_context(hartid) * PLIC_CONTEXT_ENABLE_SIZE;
}

static inline mem_addr_t context_addr(const struct device *dev)
{
	const struct hpm_plic_config *config = dev->config;

	return config->context + first_context(arch_proc_id()) * PLIC_CONTEXT_SIZE;
}

static inline const struct device *plic_dev_from_irq(uint32_t irq)
{
#ifdef CONFIG_DYNAMIC_INTERRUPTS
	return z_get_sw_isr_device_from_irq(irq);
#else
	return DEVICE_DT_INST_GET(0);
#endif
}

static void set_irq_enabled(uint32_t irq, bool enabled)
{
	const struct device *dev = plic_dev_from_irq(irq);
	const uint32_t local_irq = irq_from_level_2(irq);

	for (uint32_t cpu = 0U; cpu < arch_num_cpus(); ++cpu) {
		mem_addr_t addr = context_enable_addr(dev, cpu) +
				  local_irq_to_reg_offset(local_irq);
		uint32_t key = irq_lock();
		uint32_t value = sys_read32(addr);

		WRITE_BIT(value, local_irq & PLIC_REG_MASK, enabled);
		sys_write32(value, addr);
		irq_unlock(key);
	}
}

void riscv_plic_irq_enable(uint32_t irq)
{
	set_irq_enabled(irq, true);
}

void riscv_plic_irq_disable(uint32_t irq)
{
	set_irq_enabled(irq, false);
}

int riscv_plic_irq_is_enabled(uint32_t irq)
{
	const struct device *dev = plic_dev_from_irq(irq);
	const uint32_t local_irq = irq_from_level_2(irq);
	mem_addr_t addr = context_enable_addr(dev, 0U) +
			  local_irq_to_reg_offset(local_irq);

	return !!(sys_read32(addr) & BIT(local_irq & PLIC_REG_MASK));
}

void riscv_plic_set_priority(uint32_t irq, uint32_t priority)
{
	const struct device *dev = plic_dev_from_irq(irq);
	const struct hpm_plic_config *config = dev->config;
	const uint32_t local_irq = irq_from_level_2(irq);

	priority = MIN(priority, config->max_prio);
	sys_write32(priority, config->prio + local_irq * sizeof(uint32_t));
}

unsigned int riscv_plic_get_irq(void)
{
	return active_irq;
}

const struct device *riscv_plic_get_dev(void)
{
	return active_dev;
}

static inline void plic_io_fence(void)
{
	__asm__ volatile("fence iorw, iorw" ::: "memory");
}

static void hpm_plic_irq_handler(const struct device *dev)
{
	const struct hpm_plic_config *config = dev->config;
	const mem_addr_t threshold_addr = context_addr(dev);
	const mem_addr_t claim_addr = threshold_addr + PLIC_CONTEXT_CLAIM;
	const uint32_t local_irq = sys_read32(claim_addr);
	uint32_t previous_irq = active_irq;
	const struct device *previous_dev = active_dev;

	if ((local_irq == 0U) || (local_irq >= config->num_irqs)) {
		z_irq_spurious(NULL);
	}

	active_irq = local_irq;
	active_dev = dev;

#ifdef CONFIG_HPM_PLIC_PRIORITY_NESTING
	const uint32_t previous_threshold = sys_read32(threshold_addr);
	const uint32_t priority = sys_read32(config->prio +
					     local_irq * sizeof(uint32_t));
	const unsigned long previous_mie = csr_read(mie);
	bool nesting_enabled;

	++plic_nesting_depth;
	nesting_enabled = plic_nesting_depth < CONFIG_HPM_PLIC_MAX_NESTING_DEPTH;
	sys_write32(priority, threshold_addr);
	plic_io_fence();
	if (nesting_enabled) {
		/* Do not let timer/software IRQs bypass the PLIC priority policy. */
		csr_write(mie, previous_mie & MIP_MEIP);
		csr_set(mstatus, MSTATUS_MIE);
	}
#endif

	config->isr_table[local_irq].isr(config->isr_table[local_irq].arg);

#ifdef CONFIG_HPM_PLIC_PRIORITY_NESTING
	/* Bookkeeping and PLIC completion must be atomic with respect to nesting. */
	csr_clear(mstatus, MSTATUS_MIE);
	if (nesting_enabled) {
		csr_write(mie, previous_mie);
	}
	plic_io_fence();
	sys_write32(previous_threshold, threshold_addr);
	--plic_nesting_depth;
#endif

	sys_write32(local_irq, claim_addr);
	plic_io_fence();
	active_irq = previous_irq;
	active_dev = previous_dev;
}

static int hpm_plic_init(const struct device *dev)
{
	const struct hpm_plic_config *config = dev->config;

	for (uint32_t cpu = 0U; cpu < arch_num_cpus(); ++cpu) {
		mem_addr_t enable = context_enable_addr(dev, cpu);
		uint32_t words = local_irq_to_reg_index(config->num_irqs) + 1U;

		for (uint32_t i = 0U; i < words; ++i) {
			sys_write32(0U, enable + i * sizeof(uint32_t));
		}
	}

	sys_write32(0U, context_addr(dev));
	for (uint32_t i = 0U; i < config->num_irqs; ++i) {
		sys_write32(0U, config->prio + i * sizeof(uint32_t));
	}

	config->irq_config_func();
	return 0;
}

#define HPM_PLIC_IRQ_CONFIG_DECLARE(n) \
	static void hpm_plic_irq_config_##n(void)

#define HPM_PLIC_IRQ_CONFIG_DEFINE(n)                                             \
	static void hpm_plic_irq_config_##n(void)                                  \
	{                                                                           \
		IRQ_CONNECT(DT_INST_IRQN(n), 0, hpm_plic_irq_handler,                \
			    DEVICE_DT_INST_GET(n), 0);                                 \
		irq_enable(DT_INST_IRQN(n));                                          \
	}

#define HPM_PLIC_DEVICE_DEFINE(n)                                                  \
	HPM_PLIC_IRQ_CONFIG_DECLARE(n);                                             \
	static const struct hpm_plic_config hpm_plic_config_##n = {                 \
		.prio = DT_INST_REG_ADDR_BY_NAME(n, prio),                           \
		.irq_en = DT_INST_REG_ADDR_BY_NAME(n, irq_en),                       \
		.context = DT_INST_REG_ADDR_BY_NAME(n, reg),                         \
		.max_prio = DT_INST_PROP(n, riscv_max_priority),                     \
		.num_irqs = DT_INST_PROP(n, riscv_ndev),                             \
		.irq_config_func = hpm_plic_irq_config_##n,                           \
		.isr_table = &_sw_isr_table[INTC_INST_ISR_TBL_OFFSET(n)],            \
	};                                                                          \
	HPM_PLIC_IRQ_CONFIG_DEFINE(n)                                               \
	IRQ_PARENT_ENTRY_DEFINE(hpm_plic##n, DEVICE_DT_INST_GET(n),                 \
				DT_INST_IRQN(n), INTC_INST_ISR_TBL_OFFSET(n),          \
				DT_INST_INTC_GET_AGGREGATOR_LEVEL(n));                  \
	DEVICE_DT_INST_DEFINE(n, &hpm_plic_init, NULL, NULL,                         \
			      &hpm_plic_config_##n, PRE_KERNEL_1,                    \
			      CONFIG_INTC_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(HPM_PLIC_DEVICE_DEFINE)
