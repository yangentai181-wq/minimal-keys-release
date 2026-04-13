/*
 * Copyright (c) 2026 minimal-keys
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include <wireless_config/wireless_config.h>

LOG_MODULE_REGISTER(wireless_config, CONFIG_WIRELESS_CONFIG_LOG_LEVEL);

/* Current configuration state */
static struct wc_holdtap_config holdtap_config = {
    .tapping_term_ms = 280,
    .quick_tap_ms = 175,
    .require_prior_idle_ms = 150,
    .flavor = 1, /* balanced */
};

static struct wc_trackball_config trackball_config = {
    .cpi = 1650,
    .x_scale = 130,
    .y_scale = 160,
    .invert_x = false,
    .invert_y = false,
    .scroll_tick = 32,
    .invert_scroll_x = true,
    .invert_scroll_y = false,
};

int wireless_config_get_holdtap(struct wc_holdtap_config *config)
{
    if (!config) {
        return -EINVAL;
    }
    *config = holdtap_config;
    return 0;
}

int wireless_config_set_holdtap(const struct wc_holdtap_config *config)
{
    if (!config) {
        return -EINVAL;
    }

    holdtap_config = *config;
    LOG_INF("Hold-tap updated: tapping=%d, quick_tap=%d, idle=%d, flavor=%d",
            config->tapping_term_ms, config->quick_tap_ms,
            config->require_prior_idle_ms, config->flavor);

    /* TODO: Apply to ZMK behavior runtime */
    return 0;
}

int wireless_config_get_trackball(struct wc_trackball_config *config)
{
    if (!config) {
        return -EINVAL;
    }
    *config = trackball_config;
    return 0;
}

int wireless_config_set_trackball(const struct wc_trackball_config *config)
{
    if (!config) {
        return -EINVAL;
    }

    trackball_config = *config;
    LOG_INF("Trackball updated: cpi=%d, scale=%dx%d, scroll_tick=%d",
            config->cpi, config->x_scale, config->y_scale, config->scroll_tick);

    /* TODO: Apply to PMW3610 driver runtime */
    return 0;
}

int wireless_config_save(void)
{
#if IS_ENABLED(CONFIG_WIRELESS_CONFIG_PERSIST_SETTINGS)
    /* TODO: Implement settings save */
    LOG_INF("Saving configuration to flash");
    return 0;
#else
    LOG_WRN("Settings persistence not enabled");
    return -ENOTSUP;
#endif
}

int wireless_config_reset(void)
{
    /* Reset to defaults */
    holdtap_config = (struct wc_holdtap_config){
        .tapping_term_ms = 280,
        .quick_tap_ms = 175,
        .require_prior_idle_ms = 150,
        .flavor = 1,
    };

    trackball_config = (struct wc_trackball_config){
        .cpi = 1650,
        .x_scale = 130,
        .y_scale = 160,
        .invert_x = false,
        .invert_y = false,
        .scroll_tick = 32,
        .invert_scroll_x = true,
        .invert_scroll_y = false,
    };

    LOG_INF("Configuration reset to defaults");
    return 0;
}

static int wireless_config_init_func(void)
{
    LOG_INF("Wireless Config Service initialized");
    return 0;
}

SYS_INIT(wireless_config_init_func, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
