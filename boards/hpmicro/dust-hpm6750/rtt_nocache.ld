/* SPDX-License-Identifier: Apache-2.0 */

/*
 * SEGGER_RTT.c is compiled with -fdata-sections, so its default channel-0
 * storage arrives as three dedicated BSS input sections. Consume them inside
 * Zephyr's nocache output section before the generic BSS linker rules do.
 *
 * The control block alone is not sufficient: its read/write offsets and both
 * ring buffers are shared concurrently by the CPU and OpenOCD SBA.
 */
*(.bss._SEGGER_RTT)
*(.bss._acUpBuffer)
*(.bss._acDownBuffer)
