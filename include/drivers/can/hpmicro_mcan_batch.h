/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Load every dedicated TX buffer before one TXBAR write. Classic CAN only;
 * 1..16 frames, bounded by the configured dedicated buffer count. All frames
 * are validated before any hardware request. Callback is required and called
 * exactly once after all buffers terminate, with 0 or negative errno.
 * Admission is atomic; on-wire CAN arbitration may reorder the batch.
 * Do not mix this MCAN API with the CASTCAN hpm_can_send_batch API.
 */
int hpm_mcan_send_batch(const struct device *dev, const struct can_frame *frames, size_t count,
                        k_timeout_t timeout, can_tx_callback_t callback, void *user_data);
/* Same atomic submission, with a final deadline check immediately before
 * TXBAR. deadline_us is nonzero absolute microsecond uptime, in the domain of
 * k_ticks_to_us_floor64(k_uptime_ticks()). Rejection returns -ETIMEDOUT and
 * never requests hardware transmission or invokes the callback.
 */
int hpm_mcan_send_batch_until(const struct device *dev, const struct can_frame *frames,
                              size_t count, k_timeout_t timeout, uint64_t deadline_us,
                              can_tx_callback_t callback, void *user_data);
/* Cancel the current batch and wait for hardware pending bits to clear.
 * timeout_us must be 1..20000; includes mutex acquisition. Returns -ETIMEDOUT
 * if hardware is still pending. Admission remains blocked in that case.
 * Already transmitted frames cannot be recalled. Callback may run in the
 * calling context or ISR, outside driver locks. Never call this API from ISR.
 */
int hpm_mcan_abort_tx(const struct device *dev, uint32_t timeout_us);
#ifdef __cplusplus
}
#endif
