/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>

#if defined(CONFIG_USE_SEGGER_RTT)
#include <SEGGER_RTT.h>

/* Reinitialize RTT after PRE_KERNEL_1 has installed the SoC's nocache PMA. */
static int hpm_board_rtt_init(void)
{
	SEGGER_RTT_Init();
	return 0;
}

SYS_INIT(hpm_board_rtt_init, PRE_KERNEL_2, 0);
#endif
