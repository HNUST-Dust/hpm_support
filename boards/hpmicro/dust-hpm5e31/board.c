/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>

#if defined(CONFIG_USE_SEGGER_RTT)
#include <SEGGER_RTT.h>

/* Reinitialize RTT after PRE_KERNEL_1 has installed the SoC's nocache PMA. */
static int hpm_board_rtt_init(void)
{
    SEGGER_RTT_Init();
    return 0;
}

SYS_INIT(hpm_board_rtt_init, PRE_KERNEL_2, 0);
#endif

/* SPI IMU 暂停使用时，片选保持无效，加热 MOSFET 保持关闭。 */
static int hpm_board_imu_safe_init(void)
{
    const struct gpio_dt_spec heater =
        GPIO_DT_SPEC_GET(DT_ALIAS(onboard_imu_power), gpios);
    const struct gpio_dt_spec imu_cs =
        GPIO_DT_SPEC_GET(DT_ALIAS(onboard_imu_cs), gpios);

    if (!gpio_is_ready_dt(&heater) || !gpio_is_ready_dt(&imu_cs)) {
        return -ENODEV;
    }
    int ret = gpio_pin_configure_dt(&heater, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        return ret;
    }
    return gpio_pin_configure_dt(&imu_cs, GPIO_OUTPUT_INACTIVE);
}

SYS_INIT(hpm_board_imu_safe_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
