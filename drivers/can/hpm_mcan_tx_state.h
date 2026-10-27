/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* State is owned under the driver TX spinlock, not by mailbox admission alone. */
struct hpm_mcan_tx_state {
    uint32_t generation;
    uint32_t active;
    uint32_t transmitted;
    uint32_t cancelled;
};
static inline bool hpm_mcan_tx_begin(struct hpm_mcan_tx_state *state, unsigned count) {
    if (state->active || !count || count > 16) {
        return false;
    }
    if (++state->generation == 0) {
        ++state->generation;
    }
    state->active = (1U << count) - 1U;
    state->transmitted = 0;
    state->cancelled = 0;
    return true;
}
/* True only once, when every member terminated; TXBTO wins over TXBCF because
 * cancellation can finish after a frame was successfully transmitted. */
static inline bool hpm_mcan_tx_observe(struct hpm_mcan_tx_state *state, uint32_t transmitted,
                                       uint32_t cancelled, bool *failed) {
    if (!state->active) {
        return false;
    }
    state->transmitted |= transmitted & state->active;
    state->cancelled |= cancelled & state->active;
    if (((state->transmitted | state->cancelled) & state->active) != state->active) {
        return false;
    }
    *failed = (state->cancelled & ~state->transmitted) != 0;
    state->active = 0;
    return true;
}

/* A timed abort may resume after an ISR completed the old batch and admitted
 * a new one. Its old status snapshot must not complete the new generation. */
static inline bool hpm_mcan_tx_observe_generation(struct hpm_mcan_tx_state *state,
                                                  uint32_t transmitted, uint32_t cancelled,
                                                  uint32_t generation, bool *failed) {
    if (generation && generation != state->generation) {
        return false;
    }
    return hpm_mcan_tx_observe(state, transmitted, cancelled, failed);
}
