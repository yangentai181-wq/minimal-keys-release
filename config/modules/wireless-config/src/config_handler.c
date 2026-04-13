/*
 * Copyright (c) 2026 minimal-keys
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <wireless_config/wireless_config.h>

LOG_MODULE_DECLARE(wireless_config, CONFIG_WIRELESS_CONFIG_LOG_LEVEL);

/**
 * This file will contain the runtime configuration handlers that
 * integrate with ZMK behaviors and the PMW3610 trackball driver.
 *
 * TODO: Implement runtime behavior modification
 *
 * For hold-tap timing changes, we need to modify the ZMK behavior
 * parameters at runtime. This requires:
 * 1. Finding the behavior device
 * 2. Calling the appropriate API to update timing parameters
 *
 * For trackball settings, we need to:
 * 1. Access the PMW3610 driver
 * 2. Call spi_write to update registers (CPI, etc.)
 *
 * Reference: hyhy-masa/zmk-pmw3610-driver
 */

/* Placeholder for future implementation */
int config_handler_apply_holdtap(const struct wc_holdtap_config *config)
{
    /* TODO: Apply to ZMK &mt and &lt behaviors
     *
     * The ZMK behavior system uses device tree overlays to define
     * behavior parameters. Runtime modification would require:
     * - Accessing the behavior_hold_tap device
     * - Modifying the timing parameters directly
     *
     * This may require patches to ZMK core to expose these APIs.
     */
    LOG_DBG("Apply hold-tap: not yet implemented");
    return 0;
}

int config_handler_apply_trackball(const struct wc_trackball_config *config)
{
    /* TODO: Apply to PMW3610 driver
     *
     * The PMW3610 driver supports runtime CPI changes via Kconfig,
     * but dynamic changes would require:
     * - Accessing the PMW3610 device
     * - Writing to the CPI register (0x03)
     * - Updating scale factors in memory
     *
     * Reference: CONFIG_PMW3610_CPI, CONFIG_PMW3610_X_SCALE, etc.
     */
    LOG_DBG("Apply trackball: not yet implemented");
    return 0;
}
