/*
 * Copyright (c) 2025 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * SSC interrupt callback registration.
 */

#ifndef ECAT_ZEPHYR_PORT_H
#define ECAT_ZEPHYR_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Register SSC handlers with the ESC device before enabling interrupts. */
void ecat_irq_init(void);

#ifdef __cplusplus
}
#endif

#endif /* ECAT_ZEPHYR_PORT_H */
