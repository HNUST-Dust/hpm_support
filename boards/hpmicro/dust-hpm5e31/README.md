# Dust HPM5E31

依据用户提供的 `HPM5E31.pdf`（5 页原理图，2026-10-26）建立；MCU 为
HPM5E31IGN1。参考同系列 dust-hpm5e3y 的启动、内置 Flash 分区与 RTT 布局，
外设引脚按这张原理图重新映射，不沿用旧板的 SIP PHY 连接。

## 外设映射

| 外设 | TX / RX（或其他信号） |
| --- | --- |
| UART0 / ISP | PA00 / PA01 |
| UART1 | PC07 / PC06 |
| UART2 | PC08 / PC09 |
| UART3 | PC15 / PC14 |
| UART4 | PC16 / PC17 |
| UART5 / DBUS | 仅 RX PA22；TX 未引出 |
| UART6 | PC24 / PC25 |
| CAN0 | PC00 / PC01 |
| CAN1 | PC05 / PC04 |
| CAN2 | PD08 / PD09 |
| CAN3 | PD15 / PD14 |
| SPI3 / ICM-42688P | SCLK PC20、CS PC21、MISO PC22、MOSI PC23；INT1/2 PC18/19 |
| IMU 加热门极 | PD04 / PWM0 P4，默认 GPIO 输出低 |
| 蜂鸣器 | PF01 / PWM0 P1 |

UART0～3 使用 HDMA 通道 0～7；UART4～6 使用 XDMA 通道 0～5。
UART5 预留 RX/TX DMA 对以满足驱动 async API 的要求，但板上只能接收。
串口波特率/校验、物理 UART 与逻辑通道的对应应由应用配置，DTS 仅给出默认值。
DBUS 默认 100000 波特，其 8E2 格式仍须由应用设置，不能按默认 8N1 接收遥控。

## EtherCAT

- Port0 为外置 RTL8201F，MII 引脚 PA16～21、PA23～29。
- Port1 为外置 TJA1101B，MII 引脚 PF02～14。
- 两端共享 MDC PA31、MDIO PA30、25 MHz REFCK PA09。
- TJA1101B INT_N 为 PF26，RST_N 为 PF28（低有效）；RTL8201F 的 PHYRSTB
  只有上拉，没有 MCU 复位控制线。`reset-gpios` 只控制 TJA1101B。
- 原理图电阻配置对应 RTL8201F 地址 1、TJA1101B 地址 0；保留启动时 MDIO
  扫描以供实板核验。软件声明 PHY 地址不等于已验证 ESC 自动链路检测。
- 不使用旧板的 PV/PW 内置 PHY 配置，也不将 PA28 切换为 CTR1。
- EEPROM 仿真和旧 Flash 分区保持不变；SPI2 外置 W25Q128 暂未启用。

## 暂时禁用与待确认

SPI3 默认 disabled，板级配置默认 `CONFIG_SPI=n`；IMU 片选保持高，加热门极
保持低，PWM 默认引脚中不包含 PD04。恢复 IMU 时需一并调整 SPI、GPIO 初始化
与加热 pinctrl，不能只打开应用选项。

WS2812 的 DIN 网络为 `PWM0_P2`，但 MCU 页没有同名连接（PD02、PE02 均未接，
PF02 用于 MII TXCK）。因此暂不提供 ws2812 别名，不猜测 LED 引脚。

此定义经过编译检查，尚未刷板。PHY 专有寄存器、100BASE-T1 对端角色、ESC
链路检测及 INIT/PREOP/SAFEOP/OP 状态切换需要在后续无输出实板联调中确认。
