# HPM5E 时钟启动修正

2026-10-27，HPM5E31 空载台架，ISP 恢复后通过 RISC-V Debug Module 的 SBA 读取，CPU 无法暂停。
Flash 启动头/入口/初始化表的抽查字节与提供的 RX-only BIN 一致；不是完整 Flash 校验。

实际寄存器：

| 地址 | 值 | SDK 定义对应含义 |
|---|---|---|
| 0xf4104010 | 0xb00104b0 | DCDC 目标值低12位为 1200 mV，不代表外部仪表实测电压 |
| 0xf4001800 | 0x03020100 | CPU 选择 PLL0CLK0，分频 1 |
| 0xf40c0080 | 0x30000019 | PLL0 MFI=25 |
| 0xf40c0084 | 0 | PLL0 无小数分量 |
| 0xf40c00c0 | 0x30000000 | PLL0CLK0 后分频 1.0 |
| 0xf4000800 | 0x1fc0002f | GROUP0 LMM0 bit4 未启用 |

按 SDK 24 MHz 参考频率及其 PLL 公式推导，旧初始化实际选择 600 MHz / 目标 1.20 V。
旧代码只切换到 PLL0CLK0，没有重设 Boot ROM/preset 留下的 PLL 倍频；且未显式启用
LMM0、ROM0、AXIS。该配置与 SDK `boards/hpm5e00evk/board.c` 的 480 MHz / 1275 mV 不符。

现初始化先切回 24 MHz 晶振，再设置 1275 mV/DCM、PLL0=480 MHz 和 1.0/1.2 后分频，
最后选择 PLL0CLK0 并更新 core_clock；补开 LMM0/ROM0/AXIS。
电压/PLL 配置返回错误时停留在晶振时钟并返回初始化错误。

`tests/native/test_hpm5e_clock.sh` 提取实际时钟函数，验证 24/600 MHz 初始条件、
电压和 PLL 顺序、资源时钟以及失败时不切换高频。目标编译通过不代表硬件问题已解决。
仍需用修正版 ISP 重启，确认 JTAG 可暂停、RTT 启动完成及实际时钟寄存器符合预期。

SBA 读取缓存 RAM 可能不是 CPU 最新视图；RTT 空日志本身不足以判定没有执行 main。
现场仅把 CPU 临时切至 24 MHz 并开启 LMM0，未能恢复暂停，不能据此证明已排除其他异常。
