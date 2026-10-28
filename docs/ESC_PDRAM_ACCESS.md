# HPM5E ESC process RAM word transfers

`CONFIG_HPM_ESC_PDRAM_WORD_ACCESS` is opt-in and restricted to HPM5E00. It widens only process RAM blocks in `[0x1000,0x10000)` with aligned offsets, lengths divisible by four and at least eight bytes. Scalar/register operations and unaligned/non-multiple block requests retain the previous path. Transfers increase addresses and touch precisely the requested span; no padding, read-modify-write or asynchronous return is introduced.

The aligned CPU-buffer path uses explicit builtin memcpy with a checked alignment assumption to preserve C effective types while generating `lw/sw` without per-word libc calls under Zephyr's compiler flags. Unaligned SSC buffers remain supported. MMIO is always volatile and aligned. This implementation uses the CPU, not DMA.

References: the HPM SDK `samples/ethercat/port/hpm_ecat_hw.h` already defines scalar DWORD reads/writes and an aligned four-byte bulk read implementation; the EtherCAT Technology Group lists HPM5E00's internal PDI as 32 bit ([ESC overview](https://www.ethercat.org/download/documents/ESC_Overview.pdf)). The [HPM5E00 reference manual](https://www.hpmicro.com/Public/Uploads/uploadfile/files/20250505/HPM5E00UMV01.pdf) describes PDRAM and SYNC0/1 DMA requests in chapter 57. DMA requests alone do not establish DMA access bandwidth or safe asynchronous SSC operation.

Native test:

```bash
bash tests/native/test_esc_access.sh
```

It extracts actual driver functions and replaces only MMIO transactions with instrumented bus accesses. Both byte and word modes cover address/length/buffer alignments, 96/400-byte PDOs, register regions, end-of-RAM, exact width/order/count, canaries, overflow rejection and zero length under ASan/UBSan. No neighboring register/PDRAM byte may be touched.

Hardware: DUST HPM5E31, 125 us FreeRun, Rx96/Tx400, trace enabled, DM CAN3 ID3 disable only. Final 64 commands all passed with complete matched traces, no timeout/WKC failure. Mean ESC read 14.013 us and write 49.343 us vs byte baseline 31.446/98.114 us. RTT mean 746.987 us, maximum 998.614 us; this short run is not a hard real-time guarantee. Removing per-word memcpy calls did not materially improve the ESC transfer durations compared with the first word implementation (13.904/48.930 us), so DMA bandwidth benefits must be measured rather than assumed.

Support default stays off; the dust-adapter EtherCAT application enables it explicitly. Set the symbol to n to reproduce the byte path.
