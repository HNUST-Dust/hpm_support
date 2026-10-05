/* SPDX-License-Identifier: Apache-2.0 */

/* HPM5E3Y has a data cache and OpenOCD reads target memory through system
 * bus access while the core is running. Keep RTT's control block and buffers
 * in Zephyr's non-cacheable RAM output section so host-side writes are visible
 * to the CPU and CPU-side writes are visible to OpenOCD.
 */
*(.bss._SEGGER_RTT)
*(.bss._acUpBuffer)
*(.bss._acDownBuffer)
*(SORT_BY_ALIGNMENT(.bss._SEGGER_RTT.*))
*(SORT_BY_ALIGNMENT(.bss._acUpBuffer.*))
*(SORT_BY_ALIGNMENT(.bss._acDownBuffer.*))
