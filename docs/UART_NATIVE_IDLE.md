# HPM5E00 原生 UART RX idle

5E31/5E00 的异步 RX 使用硬件 RX idle 检测，不需要把 RX 线接到 GPTMR/TRGM。
`CONFIG_UART_HPMICRO_RX_NATIVE_IDLE` 在 HPM5E00 + UART_ASYNC_API + UART_INTERRUPT_DRIVEN
时默认启用。芯片必须由 SDK 声明 `HPM_IP_FEATURE_UART_RX_IDLE_DETECT=1`。
5E31 同时声明 E00018_FIX，因此 SDK 经由 IIR2 检查/清除 idle 状态。

## 调用与阈值

应用继续调用 Zephyr `uart_rx_enable(dev, buffer, length, timeout_us)`，不直接配置寄存器。
接收 FIFO 为 not-empty DMA 阈值；DMA 启动成功后才启用原生 idle 和 UART IRQ。
检测条件为 UART 状态机 idle，避免把字符内部的高电平误当成包间空闲。

硬件阈值是 1..255 **bit 时间**：`ceil(baudrate * timeout_us / 1000000)`。
向上取整保证不早于请求时间。例如：

- 115200 baud、1000 us → 116 bit，约 1006.94 us。
- 100000 baud、1000 us → 100 bit，1000 us。
- 921600 baud、1000 us 超过 255 bit，返回 `-ERANGE`，不截断、不静默钳位。
- `SYS_FOREVER_US`（-1）关闭部分缓冲区 idle 通知，仍保留满缓冲区 DMA 回调。
- 零或其他负超时返回 `-EINVAL`；已有 RX/circular RX 活动时不能覆盖当前接收。

每个 UART 有独立 DMA 缓冲区、阈值和状态。原生路径覆盖具备异步 DMA 的 UART，
不再依赖旧板的 UART10 特定接线。更改运行时波特率应先停止 RX，再重配并重新启动。

## 数据与生命周期

idle ISR 先清本轮标志，再发布当前缓冲区新增字节；UART_RX_RDY 保留原始 offset/len，
不固定 40 字节，也不因 idle 释放/替换 DMA 缓冲区。DMA terminal-count 回调继续拥有
满缓冲区发布、归还、切换的职责。pending=0 或终止回调待处理时，idle 让出所有权。

partial flush 复用原有 suspend / DMA 计数快照 / callback / resume 路径；回调前推进 offset，
重复 idle 不重复报告。generation 检查防止回调停止/重启 RX 后恢复旧 DMA，
DMA 满缓冲区回调也检查 generation，避免把新会话缓冲区当成旧缓冲区释放。
停止 RX 时关闭并清除 native idle，交还 current/next 缓冲区；旧停止流程不覆盖新会话。
循环 DMA 接收不进入该部分缓冲区发布路径。

DMA 查询/恢复等错误在 native ISR 触发 UART_RX_STOPPED（reason=0，表示未归类驱动错误，
不冒充奇偶或帧格式错误），随后停止并交还缓冲区，允许应用在 UART_RX_DISABLED 后重启。

idle 是字节流发布触发条件，不等于应用协议帧边界。接收缓冲区仍必须 coherent/noncacheable；
CPU fence 不构成 DMA 目的写入完成的独立证明。本次不宣称已完成实际总线压力下的数据一致性验收。

## 兼容和验证

旧 HPM6750 的 `CONFIG_UART_HPMICRO_RX_LINE_IDLE` 保持 GPTMR/TRGM 方案；
两个选项互斥，不能把旧 UART10 检测器的接线用于 5E31。

```sh
./tests/native/test_uart_idle.sh
```

ASan/UBSan 测试包括 bit 阈值及边界、重复 idle、TC 让位、错误计数和 generation。
第二组测试提取并编译驱动的真实 ISR、partial flush、回调和停止函数体，模拟寄存器/DMA/IRQ
边界；仅将 RISC-V MMIO fence 换成宿主屏障。它验证回调停止/重启、current/next 归还、
查询错误后的停止通知，不模拟硬件采样、DMA 总线写入延迟、完整 RX enable 和调度延迟。

已编译 dust-hpm5e31 EtherCAT 目标与 dust-hpm6750 USB 目标，尚未刷板。
实机仍需检查短帧/连续流、idle 与 DMA TC 相邻、HI91/遥控波特率及 CPU/总线负载下的延迟。
