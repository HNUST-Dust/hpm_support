/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct device;
/* Diagnostic only. CPU cycle clock and MCAN counter are independent clocks.
 * TSCC TCP uses nominal CAN bit times, despite the SDK config-field comment.
 * Raw RX timestamp is latched at start of frame. Counter is 16-bit; age is
 * modulo one wrap and cannot distinguish delays longer than that period.
 */
struct hpm_mcan_rx_timing {
    uint64_t irq_cycles, sample_before_cycles, sample_after_cycles;
    uint32_t tscc, rx_timestamp, counter, nominal_bitrate, fifo_status, irq_flags;
};
/* Only valid inside the same device's RX callback; copy before returning.
 * NULL if diagnostics were not built. Does not read/clear peripheral state.
 */
const struct hpm_mcan_rx_timing *hpm_mcan_last_rx_timing(const struct device *dev);
#ifdef __cplusplus
}
#endif
