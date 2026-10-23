/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HPMICRO_UART_DRIVER_EXTENSION_H_
#define HPMICRO_UART_DRIVER_EXTENSION_H_

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 UART RX 硬件循环 DMA。
 *
 * DMA 启动后持续写入 @p buffer，写到 @p size 后由 linked descriptor 自动
 * 回到起点，不经过 stop/reconfigure/start。该模式与 Zephyr UART async RX
 * API 互斥，应用通过 uart_hpm_rx_circular_get_position() 轮询生产者位置。
 */
int uart_hpm_rx_circular_enable(const struct device *dev, uint8_t *buffer, size_t size);

/** @brief 获取循环 DMA 下一字节将写入的位置，范围为 0..size。 */
int uart_hpm_rx_circular_get_position(const struct device *dev, size_t *position);

/** @brief 停止由 uart_hpm_rx_circular_enable() 启动的循环接收。 */
int uart_hpm_rx_circular_disable(const struct device *dev);

/** Select external RX-line idle notification before finite async RX starts.
 * Sets RFIFOT=0 and suppresses FIFO RX IRQs; the board owns the idle detector.
 */
int uart_hpm_rx_line_idle_enable(const struct device *dev);

/** @brief Prepare optional board RX-line idle detection before async RX starts.
 *
 * Selects external idle publication for the UART wired to the board detector.
 * Other UARTs and boards without a detector return 0 and keep native timeout
 * handling. Call before uart_rx_enable(); must be paired with
 * uart_hpm_rx_line_idle_start() once reception is enabled.
 */
int uart_hpm_rx_line_idle_prepare(const struct device *dev);

/** @brief Start optional board RX-line idle detection after async RX is enabled.
 *
 * @param dev UART device passed to uart_rx_enable().
 * @param timeout_us Quiet-line interval; the board detector defines resolution.
 * @return Zero on success or when the UART has no detector, negative errno
 * otherwise.
 */
int uart_hpm_rx_line_idle_start(const struct device *dev, uint32_t timeout_us);

/** Publish newly received bytes after independently detected RX-line idle.
 * Retains the current DMA buffer. Returns byte count, or -EAGAIN if DMA
 * completion must be handled first. Must not be used as an active-stream poll.
 * The caller must provide coherent/noncacheable RX memory and serialize
 * detector setup with RX lifecycle. This is not a DMA write-drain primitive.
 */
int uart_hpm_rx_line_idle_flush(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* HPMICRO_UART_DRIVER_EXTENSION_H_ */
