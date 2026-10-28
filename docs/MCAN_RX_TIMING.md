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

## FIFO backlog found on the HPM5E31 bench

The first 64 diagnostic samples all observed FIFO occupancy 8 and RX SOF age
5968..7040 us (mean 6511.750 us), while ISR body entry to callback took only
5.935 us on average. Counter rate checks passed with 16 us quantization.

The receive fix drains the FIFO occupancy sampled at ISR entry, capped at the
hardware 64-slot maximum. It never waits for future traffic. Flags are cleared
before draining; arrivals during the drain retain their new interrupt flag for
the next ISR. Both FIFOs are serviced. SDK read failures stop the drain and never
dispatch stale scratch data. Recovery from an actual peripheral read failure is
not guaranteed by this drain helper.

Zephyr and MCAN classic filters both use mask bit 1 to compare identifier bits.
The previous mask inversion made exact hardware masks permissive; the software
lookup used inverted OR matching. The fix stores the positive 11/29-bit mask,
uses AND matching, and rejects nonmatching traffic in the global filter.
Catch-all registration with mask 0 remains supported.

`bash tests/native/test_mcan_rx.sh` exercises extracted actual matching, FIFO
reader, drain and ISR code, plus production mask conversion expressions:
exact/range/extended/catch-all masks, eight-frame backlogs in both FIFOs, new
arrival after interrupt acknowledgement, simultaneous TX/error servicing,
read failure without stale callbacks, and discarded-frame consumption. The
configuration test also checks nonmatching rejection with diagnostics on/off.

On-board verification of the combined fix is pending; these observations alone
do not prove the original accumulation trigger or an end-to-end RTT bound.
