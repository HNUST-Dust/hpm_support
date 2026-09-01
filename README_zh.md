<h1 align="center">
  Zephyr SDK Glue
</h1>

<p align="center">
  <strong>先楫半导体 RISC-V MCU 的 Zephyr RTOS 适配</strong>
</p>

<p align="center">
  <a href="#-特性">特性</a> •
  <a href="#-支持的开发板">开发板</a> •
  <a href="#-快速开始">快速开始</a> •
  <a href="#-文档">文档</a> •
  <a href="#-许可证">许可证</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/版本-0.7.0-blue?style=flat-square" alt="版本">
  <img src="https://img.shields.io/badge/Zephyr-v3.7.0%20LTS-green?style=flat-square" alt="Zephyr">
  <img src="https://img.shields.io/badge/许可证-Apache%202.0-orange?style=flat-square" alt="许可证">
  <img src="https://img.shields.io/badge/架构-RISC--V-red?style=flat-square" alt="RISC-V">
</p>

---

## 📖 概述

**Zephyr SDK Glue** 是 HPMicro 基于 Zephyr 工程编写的清单仓库。在本仓库中包含 HPMicro 为自身 MCU 适配 Zephyr 工程的所有源程序文件，其与 HPMicro 官方软件开发包共同组成 Zephyr 工程的 HPMicro 芯片开发套件。

> 本仓库绑定 **Zephyr v3.7.0 (LTS)**，在此版本的基础上进行相关迭代。

### ✨ 特性

| 特性 | 描述 |
|------|------|
|  **独立清单文件** | 使用独立的清单文件，以 Glue 仓库为起点获取所有源码 |
|  **直接集成 HPM_SDK** | 无需 Zephyr HAL 仓库，直接基于 HPM_SDK 开发 |
|  **LTS 版本绑定** | 绑定 Zephyr v3.7.0 LTS，保证长期稳定性 |
|  **Docker 支持** | 提供开箱即用的 Docker 开发环境 |
|  **MCUboot 支持** | 支持安全固件更新的引导程序 |

---

## 🎯 支持的开发板

<table>
  <tr>
    <th>开发板</th>
    <th>SoC</th>
    <th>特性</th>
  </tr>
  <tr>
    <td><b>hpm6200evk</b></td>
    <td>HPM6280</td>
    <td>
      <img src="https://img.shields.io/badge/CAN-✓-success?style=flat-square" alt="CAN">
      <img src="https://img.shields.io/badge/USB-✓-success?style=flat-square" alt="USB">
      <img src="https://img.shields.io/badge/PWM-✓-success?style=flat-square" alt="PWM">
    </td>
  </tr>
  <tr>
    <td><b>dust-hpm6750</b></td>
    <td>HPM6750</td>
    <td>
      <img src="https://img.shields.io/badge/CAN-✓-success?style=flat-square" alt="CAN">
      <img src="https://img.shields.io/badge/USB-✓-success?style=flat-square" alt="USB">
      <img src="https://img.shields.io/badge/以太网-✓-success?style=flat-square" alt="以太网">
      <img src="https://img.shields.io/badge/显示-✓-success?style=flat-square" alt="显示">
      <img src="https://img.shields.io/badge/摄像头-✓-success?style=flat-square" alt="摄像头">
      <img src="https://img.shields.io/badge/SDHC-✓-success?style=flat-square" alt="SDHC">
    </td>
  </tr>
  <tr>
    <td><b>hpm6800evk</b></td>
    <td>HPM6880</td>
    <td>
      <img src="https://img.shields.io/badge/CAN-✓-success?style=flat-square" alt="CAN">
      <img src="https://img.shields.io/badge/USB-✓-success?style=flat-square" alt="USB">
      <img src="https://img.shields.io/badge/以太网-✓-success?style=flat-square" alt="以太网">
      <img src="https://img.shields.io/badge/显示-RGB%2FMIPI%2FLVDS-success?style=flat-square" alt="显示">
      <img src="https://img.shields.io/badge/摄像头-DVP%2FMIPI-success?style=flat-square" alt="摄像头">
      <img src="https://img.shields.io/badge/SDHC-✓-success?style=flat-square" alt="SDHC">
    </td>
  </tr>
  <tr>
    <td><b>hpm6e00evk</b></td>
    <td>HPM6E80</td>
    <td>
      <img src="https://img.shields.io/badge/CAN-✓-success?style=flat-square" alt="CAN">
      <img src="https://img.shields.io/badge/USB-✓-success?style=flat-square" alt="USB">
      <img src="https://img.shields.io/badge/以太网-✓-success?style=flat-square" alt="以太网">
    </td>
  </tr>
</table>

---

## 🔌 支持的外设驱动

| 外设 | 驱动 | 描述 |
|:----:|:----:|------|
|  ADC | `adc` | 支持 ADC12 和 ADC16 |
|  CAN | `can` | MCAN 控制器支持 |
|  时钟 | `clock_control` | 时钟管理 |
|  显示 | `display` | 支持 RGB, MIPI DSI, LVDS 显示 |
|  摄像头 | `video` | 支持 DVP 和 MIPI CSI 摄像头 |
|  DMA | `dma` | DMA 控制器支持 |
|  以太网 | `ethernet` | 以太网 MAC 支持 |
|  GPIO | `gpio` | GPIO 控制器支持 |
|  I2C | `i2c` | I2C 控制器支持 |
|  IOC | `pinctrl` | 引脚复用控制 |
|  PWM | `pwm` | 支持 PWM 和 PWMv2 |
|  SDHC | `sdhc` | SD/MMC 主机控制器支持 |
|  UART | `serial` | UART 支持 |
|  SPI | `spi` | SPI 控制器支持 |
|  USB | `usb` | USB 设备和主机 (UDC/CherryUSB) |


## 🚀 快速开始

### 前置条件

- Zephyr SDK 0.16.5+
- Python 3.8+
- CMake 3.20+
- Ninja 构建系统

### 使用 West 构建

```bash
# 初始化工作空间
west init -m https://github.com/hpmicro/sdk_glue.git zephyr_hpmicro
cd zephyr_hpmicro
west update
west supply

# 构建 hello_world 示例
west build -p always -b dust-hpm6750 zephyr/samples/hello_world

# 烧录到开发板
west flash
```

### 使用 CMake 构建

```bash
cmake -GNinja -B build -DBOARD=dust-hpm6750 zephyr/samples/hello_world
ninja -C build
```

> 使用 `-S ${snippet}` 可以为特定示例指定硬件配置代码片段。

---

## 📁 目录结构

```
sdk_glue/
├── 📂 boards/      # 开发板、拓展板支持
├── 📂 cmake/       # CMake 脚本文件
├── 📂 docs/        # 文档系统
├── 📂 drivers/     # 驱动文件
├── 📂 dts/         # 设备树文件
├── 📂 include/     # 头文件
├── 📂 modules/     # 拓展模块 (CherryUSB 等)
├── 📂 samples/     # 示例程序
├── 📂 snippets/    # 硬件配置代码片段
├── 📂 soc/         # SoC 相关文件
└── 📂 zephyr/      # Zephyr 构建相关
```

---

## 📚 文档

| 文档 | 描述 |
|------|------|
| 📖 [Linux 入门指南](docs/source/zh/starting/linux.rst) | Linux 上的开发环境搭建 |
| 📖 [Windows 入门指南](docs/source/zh/starting/windows.rst) | Windows 上的开发环境搭建 |
| 🐳 [Docker 入门指南](docs/source/zh/starting/docker.rst) | 使用 Docker 开发环境 |
| 🔧 [工具链指南](docs/source/zh/starting/toolchain.rst) | 工具链配置 (Zephyr SDK / ZCC) |

---

## 📄 许可证

本项目采用 **Apache License 2.0** 许可证 - 详见 [LICENSE](LICENSE) 文件。

---