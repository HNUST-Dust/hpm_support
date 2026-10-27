# HPM5E31 custom board

The first stage runs from the 256 KiB on-chip SRAM over JTAG. UART0 uses
PA00 (TX) and PA01 (RX) at 115200 baud; both use IOC alternate function 2.
This stage does not configure Flash boot.

UART6 uses PA24 (TX) and PA25 (RX), both IOC alternate function 2, at
115200 baud. These pins are provisional until the board schematic is
available. HDMA channel 0 sends with DMAMUX request 0x15; channel 1 receives
with request 0x14. UART0 remains the console.

MCAN0 is enabled at a nominal 500 kbit/s. PB00 (MCAN0_TXD) and PB01
(MCAN0_RXD) use IOC alternate function 7; these pins are provisional until
the schematic and package are confirmed. The controller uses PLL1_CLK0 / 10,
matching the HPM5E00 SDK's 80 MHz CAN clock setup. Its message RAM is placed
in the SoC's 32 KiB AHB SRAM. An external CAN transceiver and its enable or
standby wiring must be specified from the board schematic before bus traffic
can be tested. Applications enable `CONFIG_CAN=y` to use the controller via
`DT_CHOSEN(zephyr_canbus)` or `DT_NODELABEL(mcan0)`.

UART6 supports Zephyr's asynchronous UART API. `uart_tx()` uses TX DMA.
`uart_rx_enable()` starts RX DMA and HPM5E31's UART RX idle detector; an
idle gap produces `UART_RX_RDY` with the number of newly received bytes.
The timeout argument is converted to UART bit times. The hardware threshold
is 8 bits wide, so at 115200 baud the largest timeout is about 2.2 ms;
larger values return `-ENOTSUP`. `SYS_FOREVER_US` disables idle events while
the full-buffer event remains available. DMA buffers must be in accessible,
non-cacheable SRAM. The board enables `CONFIG_NOCACHE_MEMORY` for this use.

Build from an initialized HPM west workspace:

```powershell
west build -p always -b hpm5e31_custom D:/HPM/zephyr/samples/hello_world -d D:/HPM/sdk_glue/build/hpm5e31_custom
```

The UART6 DMA example is in `samples/uart6_dma`. Build it with:

```powershell
west build -p always -b hpm5e31_custom D:/HPM/sdk_glue/samples/uart6_dma -d D:/HPM/sdk_glue/build/hpm5e31_uart6_dma
```

The example transmits a startup message on UART6, receives variable-length
data with a 200 us idle gap, and prints received bytes in hex on UART0.

The current local west manifest cannot import Zephyr's project metadata, so
this checkout was verified with CMake and an explicit `sdk_glue` module list.
The resulting image is `build/hpm5e31_custom/zephyr/zephyr.elf`.

Once the west workspace is initialized and the board and CMSIS-DAP JTAG probe
are attached, run
`west flash -d D:/HPM/sdk_glue/build/hpm5e31_custom`. A power cycle removes
the RAM image.
