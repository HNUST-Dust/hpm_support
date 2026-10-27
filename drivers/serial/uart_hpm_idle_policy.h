/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
/* Zero bits selects no hardware detection for SYS_FOREVER_US (-1). */
static inline int uart_hpm_idle_threshold(uint32_t baud, int32_t timeout_us, uint8_t *bits) {
    if (!bits || !baud) {
        return -EINVAL;
    }
    if (timeout_us == -1) {
        *bits = 0;
        return 0;
    }
    if (timeout_us <= 0) {
        return -EINVAL;
    }
    const uint64_t count = ((uint64_t)baud * (uint32_t)timeout_us + 999999U) / 1000000U;
    if (count > 255) {
        return -ERANGE;
    }
    *bits = (uint8_t)count;
    return 0;
}
/* A pending DMA terminal-count callback owns full-buffer publication/release. */
static inline int uart_hpm_idle_delta(uint32_t total, uint32_t offset, uint32_t pending,
                                      uint32_t *length) {
    if (!pending) {
        return -EAGAIN;
    }
    if (pending > total || offset > total - pending) {
        return -EIO;
    }
    *length = total - pending - offset;
    return 0;
}
static inline bool uart_hpm_idle_resume(uint32_t before, uint32_t after, bool running) {
    return before == after && running;
}
