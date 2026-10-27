/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../drivers/can/hpm_mcan_tx_state.h"

int main(void) {
    struct hpm_mcan_tx_state state = {0};
    bool failed = false;
    assert(!hpm_mcan_tx_begin(&state, 0));
    assert(!hpm_mcan_tx_begin(&state, 17));
    assert(hpm_mcan_tx_begin(&state, 3));
    assert(!hpm_mcan_tx_begin(&state, 1));
    /* Completing only buffer zero must not release a three-frame batch. */
    assert(!hpm_mcan_tx_observe(&state, 1, 0, &failed));
    assert(!hpm_mcan_tx_observe(&state, 4, 0, &failed));
    assert(hpm_mcan_tx_observe(&state, 2, 0, &failed) && !failed);
    assert(!hpm_mcan_tx_observe(&state, 7, 0, &failed));
    assert(hpm_mcan_tx_begin(&state, 2));
    assert(!hpm_mcan_tx_observe(&state, 1, 0, &failed));
    assert(hpm_mcan_tx_observe(&state, 1, 2, &failed) && failed);
    assert(hpm_mcan_tx_begin(&state, 2));
    /* A cancellation may finish after the same frame was transmitted. */
    assert(hpm_mcan_tx_observe(&state, 3, 3, &failed) && !failed);
    assert(hpm_mcan_tx_begin(&state, 2));
    assert(!hpm_mcan_tx_observe(&state, 0xffff0000U, 0xffff0000U, &failed));
    assert(hpm_mcan_tx_observe(&state, 0, 3, &failed) && failed);
    assert(hpm_mcan_tx_begin(&state, 16));
    for (unsigned i = 0; i < 15; ++i) {
        assert(!hpm_mcan_tx_observe(&state, 1U << i, 0, &failed));
    }
    assert(hpm_mcan_tx_observe(&state, 1U << 15, 0, &failed) && !failed);
    /* Every pattern of successful versus cancelled members terminates once,
     * regardless of the order in which the interrupt snapshots arrive. */
    for (unsigned count = 1; count <= 8; ++count) {
        const unsigned mask = (1U << count) - 1;
        for (unsigned success = 0; success <= mask; ++success) {
            assert(hpm_mcan_tx_begin(&state, count));
            for (unsigned i = count; i > 0; --i) {
                const unsigned bit = 1U << (i - 1);
                const bool terminal =
                    hpm_mcan_tx_observe(&state, success & bit, (~success) & bit, &failed);
                assert(terminal == (i == 1));
            }
            assert(failed == (success != mask));
            assert(!hpm_mcan_tx_observe(&state, mask, mask, &failed));
        }
    }
    assert(hpm_mcan_tx_begin(&state, 1));
    const uint32_t old_generation = state.generation;
    assert(hpm_mcan_tx_observe(&state, 1, 0, &failed));
    assert(hpm_mcan_tx_begin(&state, 1));
    assert(!hpm_mcan_tx_observe_generation(&state, 1, 1, old_generation, &failed));
    assert(state.active == 1);
    assert(hpm_mcan_tx_observe_generation(&state, 1, 0, state.generation, &failed));
    /* Generation zero is reserved for the current ISR snapshot. */
    state.generation = UINT32_MAX;
    assert(hpm_mcan_tx_begin(&state, 1) && state.generation == 1);
    assert(hpm_mcan_tx_observe_generation(&state, 1, 0, 0, &failed));
    puts("MCAN batch completion / cancellation contracts PASS");
}
