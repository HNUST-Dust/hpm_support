/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "drivers/can/hpmicro_mcan_timing.h"
#define ARG_UNUSED(x) (void)(x)
#define MCAN_TIMESTAMP_SEL_VALUE_INCREMENT 1
struct mcan_config { uint32_t baudrate; struct { uint32_t counter_prescaler, timestamp_selection; } timestamp_cfg; };
typedef struct mcan_config mcan_config_t;
struct registers { uint32_t TSCC, TSCV; };
struct hpm_mcan_config { struct registers *base; };
struct hpm_mcan_data { mcan_config_t config;
#if defined(CONFIG_HPM_MCAN_RX_TIMING)
struct hpm_mcan_rx_timing rx_timing;
#endif
};
struct device { const void *config; void *data; };
static uint64_t cycles;
static inline uint64_t k_cycle_get_64(void) { return ++cycles; }
static inline uint16_t mcan_get_timestamp_counter_value(struct registers *r) { return (uint16_t)r->TSCV; }
static void mcan_get_default_config(void *can, mcan_config_t *config) { (void)can; memset(config,0,sizeof(*config)); }
#if defined(CONFIG_HPM_MCAN_RX_TIMING)
static struct { uint16_t rx_timestamp; } s_can_rx_buf;
#endif
#include "mcan_timing.inc"
int main(void) {
    struct registers regs = {.TSCC=0xf0001,.TSCV=5};
    struct hpm_mcan_config cfg = {.base=&regs};
    struct hpm_mcan_data data = {0};
    struct device dev = {.config=&cfg,.data=&data};
    configure_timing(&data.config);
#if defined(CONFIG_HPM_MCAN_RX_TIMING)
    assert(data.config.timestamp_cfg.counter_prescaler == 16);
    assert(data.config.timestamp_cfg.timestamp_selection == 1);
    assert(hpm_mcan_last_rx_timing(NULL) == NULL);
    data.config.baudrate = 1000000;
    data.rx_timing.irq_cycles = 12;
    data.rx_timing.irq_flags = 0x20;
    s_can_rx_buf.rx_timestamp = 65530;
    cycles = 100;
    hpm_mcan_sample_rx_timing(&dev, 2);
    const struct hpm_mcan_rx_timing *t = hpm_mcan_last_rx_timing(&dev);
    assert(t->irq_cycles == 12 && t->irq_flags == 0x20);
    assert(t->sample_before_cycles == 101 && t->sample_after_cycles == 102);
    assert(t->tscc == 0xf0001 && t->rx_timestamp == 65530 && t->counter == 5);
    assert(t->nominal_bitrate == 1000000 && t->fifo_status == 2);
    assert(regs.TSCV == 5); // Reader does not reset timestamp counter.
#else
    assert(data.config.timestamp_cfg.counter_prescaler == 0);
    assert(hpm_mcan_last_rx_timing(&dev) == NULL);
#endif
}
