/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/toolchain.h>
#include <soc.h>
#include "hpm_csr_regs.h"

GTEXT(_start)
SECTION_FUNC(init, _start)
	csrrw x0, mstatus, x0
#ifdef CONFIG_ICACHE
	call l1c_ic_enable
#else
	call l1c_ic_disable
#endif
#ifdef CONFIG_DCACHE
	call l1c_dc_enable
	call l1c_dc_invalidate_all
#else
	call l1c_dc_invalidate_all
	call l1c_dc_disable
#endif
	call c_startup
	li t0, 0xf4002800
	li t1, 0x3402
	sw t1, 0(t0)
	j __start
