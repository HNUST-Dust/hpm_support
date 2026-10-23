/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Optional UART RX line-idle detector for partial DMA publication.
 *
 * The HPM UART FIFO timeout cannot report a partially filled DMA buffer, so a
 * board may wire the RX line to a GPTMR capture input through TRGM and publish
 * the buffer once the line has been quiet for a configured interval. On Dust V3
 * the DBUS RX signal reaches UART10 RX/PC04 and PB13/TRGM1_P03; GPTMR2 channel 2
 * captures edges and channel 0 provides the idle compare event.
 *
 * The detector is board wiring, not a generic UART capability. Enable it only
 * on boards that define the uart10_idle_timer interrupt and the TRGM routing
 * used here.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/irq.h>
#include <zephyr/sys/util.h>

#include <errno.h>
#include <stdint.h>

#include <hpm_clock_drv.h>
#include <hpm_gptmr_drv.h>
#include <hpm_trgm_drv.h>
#include <hpm_trgmmux_src.h>

#include <drivers/uart_hpmicro.h>

#if IS_ENABLED(CONFIG_UART_HPMICRO_RX_LINE_IDLE)

#define LINE_IDLE_COMPARE_CHANNEL 0U
#define LINE_IDLE_CAPTURE_CHANNEL 2U
#define LINE_IDLE_IRQ DT_IRQN(DT_NODELABEL(uart10_idle_timer))

static const struct device *line_idle_receiver;
static uint32_t line_idle_ticks;

/**
 * @brief Publish DMA progress after a quiet RX interval and rearm detection.
 *
 * A compare event is deferred when a new RX edge has already restarted the
 * counter, and DMA completion keeps ownership of buffer replacement. Only the
 * detector is rearmed here.
 */
static void line_idle_isr(const void *arg)
{
	const uint32_t compare_status =
		GPTMR_CH_CMP_STAT_MASK(LINE_IDLE_COMPARE_CHANNEL, 0);
	const uint32_t capture_status =
		GPTMR_CH_CAP_STAT_MASK(LINE_IDLE_CAPTURE_CHANNEL);
	unsigned int key = irq_lock();

	ARG_UNUSED(arg);

	if (gptmr_check_status(HPM_GPTMR2, compare_status)) {
		if (gptmr_channel_get_counter(HPM_GPTMR2,
					      LINE_IDLE_COMPARE_CHANNEL,
					      gptmr_counter_type_normal) + 1 <
		    line_idle_ticks) {
			gptmr_clear_status(HPM_GPTMR2, compare_status);
			irq_unlock(key);
			return;
		}
		gptmr_stop_counter(HPM_GPTMR2, LINE_IDLE_COMPARE_CHANNEL);
		gptmr_disable_irq(HPM_GPTMR2,
				  GPTMR_CH_CMP_IRQ_MASK(LINE_IDLE_COMPARE_CHANNEL, 0));
		gptmr_clear_status(HPM_GPTMR2, compare_status | capture_status);
		(void)uart_hpm_rx_line_idle_flush(line_idle_receiver);
		gptmr_enable_irq(HPM_GPTMR2,
				 GPTMR_CH_CAP_IRQ_MASK(LINE_IDLE_CAPTURE_CHANNEL));
	} else if (gptmr_check_status(HPM_GPTMR2, capture_status)) {
		gptmr_clear_status(HPM_GPTMR2, capture_status | compare_status);
		gptmr_disable_irq(HPM_GPTMR2,
				  GPTMR_CH_CAP_IRQ_MASK(LINE_IDLE_CAPTURE_CHANNEL));
		gptmr_channel_reset_count(HPM_GPTMR2, LINE_IDLE_COMPARE_CHANNEL);
		gptmr_start_counter(HPM_GPTMR2, LINE_IDLE_COMPARE_CHANNEL);
		gptmr_enable_irq(HPM_GPTMR2,
				 GPTMR_CH_CMP_IRQ_MASK(LINE_IDLE_COMPARE_CHANNEL, 0));
	}
	irq_unlock(key);
}

/**
 * @brief Configure the GPTMR2/TRGM1 detector with a microsecond threshold.
 */
static int line_idle_start(const struct device *dev, uint32_t timeout_us)
{
	gptmr_channel_config_t config;
	uint64_t ticks;

	line_idle_receiver = dev;
	clock_add_to_group(clock_gptmr2, 0);
	ticks = (uint64_t)clock_get_frequency(clock_gptmr2) * timeout_us / 1000000U;
	if (!ticks || ticks >= UINT32_MAX - 1) return -EINVAL;
	line_idle_ticks = (uint32_t)ticks;

	gptmr_channel_get_default_config(HPM_GPTMR2, &config);
	config.enable_cmp_output = true;
	config.cmp[0] = line_idle_ticks;
	config.synci_edge = gptmr_synci_edge_both;
	if (gptmr_channel_config(HPM_GPTMR2, LINE_IDLE_COMPARE_CHANNEL, &config,
				 false) != status_success)
		return -EIO;

	gptmr_channel_get_default_config(HPM_GPTMR2, &config);
	config.enable_cmp_output = false;
	config.mode = gptmr_work_mode_capture_at_falling_edge;
	if (gptmr_channel_config(HPM_GPTMR2, LINE_IDLE_CAPTURE_CHANNEL, &config,
				 false) != status_success)
		return -EIO;

	trgm_output_update_source(HPM_TRGM1, HPM_TRGM1_OUTPUT_SRC_GPTMR2_SYNCI,
				  HPM_TRGM1_INPUT_SRC_TRGM1_P3);
	trgm_output_update_source(HPM_TRGM1, HPM_TRGM1_OUTPUT_SRC_GPTMR2_IN2,
				  HPM_TRGM1_INPUT_SRC_TRGM1_P3);
	gptmr_clear_status(HPM_GPTMR2,
			   GPTMR_CH_CMP_STAT_MASK(LINE_IDLE_COMPARE_CHANNEL, 0) |
				   GPTMR_CH_CAP_STAT_MASK(LINE_IDLE_CAPTURE_CHANNEL));
	gptmr_channel_reset_count(HPM_GPTMR2, LINE_IDLE_COMPARE_CHANNEL);
	gptmr_channel_reset_count(HPM_GPTMR2, LINE_IDLE_CAPTURE_CHANNEL);
	gptmr_enable_irq(HPM_GPTMR2,
			 GPTMR_CH_CMP_IRQ_MASK(LINE_IDLE_COMPARE_CHANNEL, 0));
	IRQ_CONNECT(LINE_IDLE_IRQ, 2, line_idle_isr, NULL, 0);
	irq_enable(LINE_IDLE_IRQ);
	gptmr_start_counter(HPM_GPTMR2, LINE_IDLE_CAPTURE_CHANNEL);
	gptmr_start_counter(HPM_GPTMR2, LINE_IDLE_COMPARE_CHANNEL);
	return 0;
}

int uart_hpm_rx_line_idle_prepare(const struct device *dev)
{
	if (dev != DEVICE_DT_GET(DT_NODELABEL(uart10))) return 0;
	return uart_hpm_rx_line_idle_enable(dev);
}

int uart_hpm_rx_line_idle_start(const struct device *dev, uint32_t timeout_us)
{
	if (dev != DEVICE_DT_GET(DT_NODELABEL(uart10))) return 0;
	return line_idle_start(dev, timeout_us);
}

#else

int uart_hpm_rx_line_idle_prepare(const struct device *dev)
{
	ARG_UNUSED(dev);
	return 0;
}

int uart_hpm_rx_line_idle_start(const struct device *dev, uint32_t timeout_us)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(timeout_us);
	return 0;
}

#endif /* CONFIG_UART_HPMICRO_RX_LINE_IDLE */
