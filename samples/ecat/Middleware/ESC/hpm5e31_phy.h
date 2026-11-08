/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef HPM5E31_PHY_H_
#define HPM5E31_PHY_H_

#include <zephyr/device.h>
#include "hpm_common.h"

void ecat_phy_reset(const struct device *esc_dev);
void ecat_phy_keep_reset(const struct device *esc_dev);
hpm_stat_t ecat_phy_config(const struct device *esc_dev);
void ecat_phy_update_link(const struct device *esc_dev);

#endif
