# Optional MCAN RX timing diagnostics

`CONFIG_HPM_MCAN_RX_TIMING=y` enables diagnostic snapshots, default n.
No CAN payload/filter/bitrate, transmit admission or protection behavior changes.

The driver enables the internal timestamp source, TCP=16, preserving the setting
through set_timing/start reinitialization. HPM5E00 MCAN register definitions state
that TSCC.TCP counts multiples of nominal CAN bit times (the SDK configuration
field's AHB comment is misleading). At 1 Mbps one tick is 16 us and the 16-bit
wrap period is 1.048576 s. RX timestamps latch at start of frame, so age at ISR
sampling includes frame reception, not just interrupt delivery delay.

`hpm_mcan_last_rx_timing(dev)` is valid only during the same device's RX callback.
It returns a driver-owned snapshot: CPU cycles at ISR body entry, cycles before
and after the counter read, raw TSCC/RX timestamp/counter, nominal bitrate,
FIFO status before dequeue and IRQ flags. Copy before returning from callback.
When disabled the API returns NULL and no timestamp setup/sampling is added.
The hardware counter is only read, never reset by the diagnostic sampling.

Always keep clocks separate: derive CPU intervals from CPU cycles and hardware
age from modulo MCAN-counter differences. Calibrate counter progression against
CPU intervals in successive observations; the 16-bit age cannot disambiguate a
full wrap or more. FIFO sampling is observational; this change does not alter
the driver's dequeue policy.

`bash tests/native/test_mcan_timing.sh` extracts and runs actual setup, callback
snapshot API and sampling functions in enabled/disabled configurations. On-board
validation is still required to confirm the counter's effective rate and timing.
