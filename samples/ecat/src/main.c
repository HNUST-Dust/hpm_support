/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <hpm_clock_drv.h>

#include "applInterface.h"
#include "digital_io.h"
#include "hpm_ecat_hw.h"
#include "ecat_zephyr_port.h"

BUILD_ASSERT(DT_PROP(DT_NODELABEL(esc0), software_link0),
	     "Port0 link polling requires software-link0");

int main(void)
{
	printk("HPM5E31 EtherCAT Port0 MII\n");
	printk("CPU0 clock: %u Hz, ESC bus clock: %u Hz\n",
	       clock_get_frequency(clock_cpu0), clock_get_frequency(clock_esc0));
	printk("Console UART0 OK\n");

#if 1
	const struct device *esc = ECAT_ESC_DEVICE;
	uint32_t last_link_poll;

	if (!device_is_ready(esc)) {
		printk("ESC device is not ready\n");
		return 0;
	}
	ecat_irq_init();
	if (ecat_hardware_init(esc) != status_success) {
		printk("EtherCAT startup failed: check EEPROM/PHY diagnostics above\n");
		return 0;
	}
	if (MainInit() != 0U) {
		printk("SSC initialization failed\n");
		return 0;
	}
#if defined(ESC_EEPROM_EMULATION) && ESC_EEPROM_EMULATION
	/* MainInit clears these hooks; install them after stack initialization. */
	pAPPL_EEPROM_Read = ecat_eeprom_emulation_read;
	pAPPL_EEPROM_Write = ecat_eeprom_emulation_write;
	pAPPL_EEPROM_Reload = ecat_eeprom_emulation_reload;
	pAPPL_EEPROM_Store = ecat_eeprom_emulation_store;
#endif
	if (APPL_GenerateMapping(&nPdInputSize, &nPdOutputSize) != 0U) {
		printk("EtherCAT PDO mapping failed\n");
		return 0;
	}
	ecat_phy_update_link(esc);
	bRunApplication = TRUE;
	hpm_esc_start(esc);
	printk("EtherCAT SSC running, PDO input/output: %u/%u bytes\n",
	       nPdInputSize, nPdOutputSize);

	last_link_poll = k_uptime_get_32();
	while (bRunApplication == TRUE) {
		MainLoop();
		uint32_t now = k_uptime_get_32();
		if ((uint32_t)(now - last_link_poll) >= 100U) {
			ecat_phy_update_link(esc);
			last_link_poll = now;
		}
	}
	HW_Release();
#endif
	return 0;
}
