# HPMicro EtherCAT slave controller

This driver exposes the integrated ESC in HPM5E3x devices to an EtherCAT slave
stack. It follows the initialization order used by the HPM SDK EtherCAT samples:
enable the ESC clocks, configure EEPROM mode, reset and configure the PHYs,
route NMII link inputs, and finally enable PDI/SYNC/reset interrupts.

The driver intentionally does not include Beckhoff SSC generated sources. An
application owns its object dictionary and SSC version and registers the four
SSC interrupt entry points through `hpm_ethercat_set_callbacks()`.

## Devicetree

The following is the HPM5E00EVK topology used by the SDK sample (the pinctrl
group is board-specific and is omitted here):

```dts
esc0: ethercat@f1700000 {
	compatible = "hpmicro,hpm-esc";
	reg = <0xf1700000 0x10000>;
	interrupt-parent = <&plic>;
	interrupts = <61 2>, <62 3>, <63 3>, <64 2>;
	interrupt-names = "pdi", "sync0", "sync1", "reset";
	pinctrl-0 = <&pinctrl_esc0>;
	pinctrl-names = "default";
	reset-gpios = <&gpiob 24 GPIO_ACTIVE_LOW>;
	phy-address-offset = <1>;
	phy-addresses = <0 1 2>;
	nmii-link-ctrl-indices = <2 5 6>;
	nmii-link-inverted = <1 0 0>;
	configure-jl1111;
	eeprom-emulation;
	eeprom-read-size = <4>;
	status = "okay";
};
```

Set `eeprom-emulation` when EEPROM words are supplied by firmware. Register
`hpm_ethercat_eeprom_ops` before calling `hpm_ethercat_start()`, and call
`hpm_ethercat_process_eeprom()` from the SSC EEPROM application hooks. Without
that property the ESC uses its external I2C EEPROM; the board pinctrl group must
then include the EEPROM pins.

## SSC adapter outline

```c
static void pdi(const struct device *dev, void *arg)   { PDI_Isr(); }
static void sync0(const struct device *dev, void *arg) { Sync0_Isr(); }
static void sync1(const struct device *dev, void *arg) { Sync1_Isr(); }

static const struct hpm_ethercat_callbacks callbacks = {
	.pdi = pdi,
	.sync0 = sync0,
	.sync1 = sync1,
};

const struct device *esc = DEVICE_DT_GET(DT_NODELABEL(esc0));
hpm_ethercat_set_callbacks(esc, &callbacks);
/* Register EEPROM callbacks here when emulation is selected. */
hpm_ethercat_start(esc);
```

`hpm_ethercat_read()`, `hpm_ethercat_write()` and
`hpm_ethercat_get_al_event()` replace the direct `pEsc` accesses in the HPM SDK
port. `hpm_ethercat_timer_get()` supplies the SSC millisecond time base from the
Zephyr uptime clock.
