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

/* Keymap storage - initialized to transparent (0x0000) */
static struct wc_layer_keymap layer_keymaps[WC_NUM_LAYERS];

/* Combo storage */
static struct wc_combo_config combos[WC_MAX_COMBOS];

/* Auto-mouse configuration */
static struct wc_automouse_config automouse_config = {
    .enabled = false,
    .target_layer = 4,  /* Mouse layer */
    .timeout_ms = 500,
    .scroll_layer = -1, /* Disabled */
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

int wireless_config_get_keymap(uint8_t layer_index, struct wc_layer_keymap *keymap)
{
    if (!keymap || layer_index >= WC_NUM_LAYERS) {
        return -EINVAL;
    }

    *keymap = layer_keymaps[layer_index];
    keymap->layer_index = layer_index;
    return 0;
}

int wireless_config_set_keymap(const struct wc_layer_keymap *keymap)
{
    if (!keymap || keymap->layer_index >= WC_NUM_LAYERS) {
        return -EINVAL;
    }

    layer_keymaps[keymap->layer_index] = *keymap;
    LOG_INF("Keymap updated: layer=%d", keymap->layer_index);

    /* TODO: Apply to ZMK keymap runtime */
    return 0;
}

int wireless_config_get_combo(uint8_t combo_id, struct wc_combo_config *combo)
{
    if (!combo || combo_id >= WC_MAX_COMBOS) {
        return -EINVAL;
    }

    *combo = combos[combo_id];
    combo->id = combo_id;
    return 0;
}

int wireless_config_get_all_combos(struct wc_combo_config *out_combos, size_t *count)
{
    if (!out_combos || !count) {
        return -EINVAL;
    }

    memcpy(out_combos, combos, sizeof(combos));
    *count = WC_MAX_COMBOS;
    return 0;
}

int wireless_config_set_combo(const struct wc_combo_config *combo)
{
    if (!combo || combo->id >= WC_MAX_COMBOS) {
        return -EINVAL;
    }

    combos[combo->id] = *combo;
    LOG_INF("Combo updated: id=%d, enabled=%d, keycode=0x%04x",
            combo->id, combo->enabled, combo->keycode);

    /* TODO: Apply to ZMK combo runtime */
    return 0;
}

int wireless_config_get_automouse(struct wc_automouse_config *config)
{
    if (!config) {
        return -EINVAL;
    }

    *config = automouse_config;
    return 0;
}

int wireless_config_set_automouse(const struct wc_automouse_config *config)
{
    if (!config) {
        return -EINVAL;
    }

    automouse_config = *config;
    LOG_INF("Auto-mouse updated: enabled=%d, target=%d, timeout=%d, scroll=%d",
            config->enabled, config->target_layer, config->timeout_ms, config->scroll_layer);

    /* TODO: Apply to trackball/layer runtime */
    return 0;
}

/* Defined in settings_storage.c */
extern int wireless_config_settings_save(void);

int wireless_config_save(void)
{
#if IS_ENABLED(CONFIG_WIRELESS_CONFIG_PERSIST_SETTINGS)
    LOG_INF("Saving configuration to flash");
    return wireless_config_settings_save();
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

    /* Reset keymaps to transparent */
    memset(layer_keymaps, 0, sizeof(layer_keymaps));
    for (int i = 0; i < WC_NUM_LAYERS; i++) {
        layer_keymaps[i].layer_index = i;
    }

    /* Reset combos */
    memset(combos, 0, sizeof(combos));
    for (int i = 0; i < WC_MAX_COMBOS; i++) {
        combos[i].id = i;
        combos[i].layer_mask = 0xFF; /* Active on all layers by default */
    }

    /* Reset auto-mouse */
    automouse_config = (struct wc_automouse_config){
        .enabled = false,
        .target_layer = 4,
        .timeout_ms = 500,
        .scroll_layer = -1,
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
