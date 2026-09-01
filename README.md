<h1 align="center">
  Zephyr SDK Glue
</h1>

<p align="center">
  <strong>HPMicro's Zephyr RTOS Adaptation for RISC-V MCUs</strong>
</p>

<p align="center">
  <a href="#-features">Features</a> •
  <a href="#-supported-boards">Boards</a> •
  <a href="#-quick-start">Quick Start</a> •
  <a href="#-documentation">Docs</a> •
  <a href="#-license">License</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Version-0.7.0-blue?style=flat-square" alt="Version">
  <img src="https://img.shields.io/badge/Zephyr-v3.7.0%20LTS-green?style=flat-square" alt="Zephyr">
  <img src="https://img.shields.io/badge/License-Apache%202.0-orange?style=flat-square" alt="License">
  <img src="https://img.shields.io/badge/Architecture-RISC--V-red?style=flat-square" alt="RISC-V">
</p>

---

## 📖 Overview

**Zephyr SDK Glue** is a manifest repository developed by HPMicro based on the Zephyr project. This repository contains all source program files adapted by HPMicro for its own MCUs in the Zephyr project. Together with HPMicro's official software development kit (SDK), it forms the HPMicro chip development suite for the Zephyr project.

> This repository is bound to **Zephyr v3.7.0 (LTS)** and undergoes related iterations on this version basis.

### ✨ Features

| Feature | Description |
|---------|-------------|
| **Independent Manifest** | Uses its own manifest file, fetching all source code starting from the Glue repository |
| **Direct HPM_SDK Integration** | No Zephyr HAL repository required, developed directly based on HPM_SDK |
| **LTS Version Binding** | Bound to Zephyr v3.7.0 LTS for long-term stability |
| **Docker Support** | Ready-to-use Docker development environment |
| **MCUboot Support** | Bootloader support for secure firmware updates |

---

## 🎯 Supported Boards

<table>
  <tr>
    <th>Board</th>
    <th>SoC</th>
    <th>Features</th>
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
      <img src="https://img.shields.io/badge/Ethernet-✓-success?style=flat-square" alt="Ethernet">
      <img src="https://img.shields.io/badge/Display-✓-success?style=flat-square" alt="Display">
      <img src="https://img.shields.io/badge/Video-✓-success?style=flat-square" alt="Video">
      <img src="https://img.shields.io/badge/SDHC-✓-success?style=flat-square" alt="SDHC">
    </td>
  </tr>
  <tr>
    <td><b>hpm6800evk</b></td>
    <td>HPM6880</td>
    <td>
      <img src="https://img.shields.io/badge/CAN-✓-success?style=flat-square" alt="CAN">
      <img src="https://img.shields.io/badge/USB-✓-success?style=flat-square" alt="USB">
      <img src="https://img.shields.io/badge/Ethernet-✓-success?style=flat-square" alt="Ethernet">
      <img src="https://img.shields.io/badge/Display-RGB%2FMIPI%2FLVDS-success?style=flat-square" alt="Display">
      <img src="https://img.shields.io/badge/Video-DVP%2FMIPI-success?style=flat-square" alt="Video">
      <img src="https://img.shields.io/badge/SDHC-✓-success?style=flat-square" alt="SDHC">
    </td>
  </tr>
  <tr>
    <td><b>hpm6e00evk</b></td>
    <td>HPM6E80</td>
    <td>
      <img src="https://img.shields.io/badge/CAN-✓-success?style=flat-square" alt="CAN">
      <img src="https://img.shields.io/badge/USB-✓-success?style=flat-square" alt="USB">
      <img src="https://img.shields.io/badge/Ethernet-✓-success?style=flat-square" alt="Ethernet">
    </td>
  </tr>
</table>

---

## 🔌 Supported Peripherals

| Peripheral | Driver | Description |
|:----------:|:------:|-------------|
| ADC | `adc` | ADC12 and ADC16 support |
| CAN | `can` | MCAN controller support |
| Clock | `clock_control` | Clock management |
| Display | `display` | RGB, MIPI DSI, LVDS display support |
| Video | `video` | DVP and MIPI CSI camera support |
| DMA | `dma` | DMA controller support |
| Ethernet | `ethernet` | Ethernet MAC support |
| GPIO | `gpio` | GPIO controller support |
| I2C | `i2c` | I2C controller support |
| IOC | `pinctrl` | Pin multiplexing control |
| PWM | `pwm` | PWM and PWMv2 support |
| SDHC | `sdhc` | SD/MMC host controller support |
| UART | `serial` | UART support |
| SPI | `spi` | SPI controller support |
| USB | `usb` | USB device and host (UDC/CherryUSB) |


## 🚀 Quick Start

### Prerequisites

- Zephyr SDK 0.16.5+
- Python 3.8+
- CMake 3.20+
- Ninja build system

### Build with West

```bash
# Initialize workspace
west init -m https://github.com/hpmicro/sdk_glue.git zephyr_hpmicro
cd zephyr_hpmicro
west update
west supply

# Build hello_world example
west build -p always -b dust-hpm6750 zephyr/samples/hello_world

# Flash to board
west flash
```

### Build with CMake

```bash
cmake -GNinja -B build -DBOARD=dust-hpm6750 zephyr/samples/hello_world
ninja -C build
```

> Use `-S ${snippet}` to specify hardware configuration snippets for specific samples.

---

## 📁 Directory Structure

```
sdk_glue/
├── 📂 boards/      # Board and shield support files
├── 📂 cmake/       # CMake extensions
├── 📂 docs/        # Documentation
├── 📂 drivers/     # Zephyr standard driver files
├── 📂 dts/         # Devicetree files
├── 📂 include/     # Header files
├── 📂 modules/     # Additional modules (CherryUSB, etc.)
├── 📂 samples/     # Sample applications
├── 📂 snippets/    # Hardware configuration snippets
├── 📂 soc/         # SoC specific source
└── 📂 zephyr/      # Zephyr build definitions
```

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| 📖 [Linux Getting Started](docs/source/en/starting/linux.rst) | Build environment setup on Linux |
| 📖 [Windows Getting Started](docs/source/en/starting/windows.rst) | Build environment setup on Windows |
| 🐳 [Docker Getting Started](docs/source/en/starting/docker.rst) | Using Docker development environment |
| 🔧 [Toolchain Guide](docs/source/en/starting/toolchain.rst) | Toolchain configuration (Zephyr SDK / ZCC) |

---

## 📄 License

This project is licensed under the **Apache License 2.0** - see the [LICENSE](LICENSE) file for details.

---
