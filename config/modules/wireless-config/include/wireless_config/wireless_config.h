/*
 * Copyright (c) 2026 minimal-keys
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @file wireless_config.h
 * @brief Wireless configuration service for minimal-keys keyboard
 *
 * Provides BLE-based runtime configuration for:
 * - Combo settings
 * - Hold-tap timing
 * - Trackball settings
 * - Layer configuration
 */

/* Protocol command types */
enum wireless_config_cmd {
    WC_CMD_GET_VERSION      = 0x00,
    WC_CMD_KEYMAP           = 0x01,
    WC_CMD_COMBO            = 0x02,
    WC_CMD_HOLDTAP          = 0x03,
    WC_CMD_TRACKBALL        = 0x04,
    WC_CMD_SCROLL           = 0x05,
    WC_CMD_LAYER            = 0x06,
    WC_CMD_SAVE             = 0x07,
    WC_CMD_RESET            = 0x08,
    WC_CMD_AUTOMOUSE        = 0x09,
    /* Events (notifications) */
    WC_EVT_KEY              = 0x10,
    WC_EVT_LAYER_CHANGE     = 0x11,
};

/* Protocol response status */
enum wireless_config_status {
    WC_STATUS_OK            = 0x00,
    WC_STATUS_ERROR         = 0x01,
    WC_STATUS_INVALID_CMD   = 0x02,
    WC_STATUS_INVALID_PARAM = 0x03,
};

/* Hold-tap configuration */
struct wc_holdtap_config {
    uint16_t tapping_term_ms;
    uint16_t quick_tap_ms;
    uint16_t require_prior_idle_ms;
    uint8_t flavor;  /* 0=hold-preferred, 1=balanced, 2=tap-preferred */
};

/* Trackball configuration */
struct wc_trackball_config {
    uint16_t cpi;
    uint8_t x_scale;
    uint8_t y_scale;
    bool invert_x;
    bool invert_y;
    uint16_t scroll_tick;
    bool invert_scroll_x;
    bool invert_scroll_y;
};

/* Keyboard layout constants */
#define WC_NUM_KEYS     43
#define WC_NUM_LAYERS   8
#define WC_MAX_COMBOS   16
#define WC_MAX_COMBO_KEYS 4

/* Layer keymap (single layer) */
struct wc_layer_keymap {
    uint8_t layer_index;
    uint16_t keycodes[WC_NUM_KEYS];
};

/* Combo configuration */
struct wc_combo_config {
    uint8_t id;              /* Combo slot index (0-15) */
    bool enabled;
    uint8_t keys[WC_MAX_COMBO_KEYS]; /* Key positions */
    uint8_t key_count;       /* Number of keys in combo */
    uint16_t keycode;        /* Output keycode */
    uint16_t term_ms;        /* Time window to press all keys */
    uint16_t require_prior_idle_ms;
    uint8_t layer_mask;      /* Bitmask of active layers (0xFF = all) */
};

/* Auto-mouse configuration */
struct wc_automouse_config {
    bool enabled;
    uint8_t target_layer;    /* Layer to activate on trackball movement */
    uint16_t timeout_ms;     /* Time before returning to previous layer */
    int8_t scroll_layer;     /* Layer for scroll mode (-1 = disabled) */
};

/* Key event notification */
struct wc_key_event {
    uint8_t position;
    bool pressed;
    uint8_t layer;
    uint8_t modifiers;
};

/**
 * Initialize the wireless configuration service
 * @return 0 on success, negative error code on failure
 */
int wireless_config_init(void);

/**
 * Get current hold-tap configuration
 */
int wireless_config_get_holdtap(struct wc_holdtap_config *config);

/**
 * Set hold-tap configuration
 */
int wireless_config_set_holdtap(const struct wc_holdtap_config *config);

/**
 * Get current trackball configuration
 */
int wireless_config_get_trackball(struct wc_trackball_config *config);

/**
 * Set trackball configuration
 */
int wireless_config_set_trackball(const struct wc_trackball_config *config);

/**
 * Get keymap for a specific layer
 */
int wireless_config_get_keymap(uint8_t layer_index, struct wc_layer_keymap *keymap);

/**
 * Set keymap for a specific layer
 */
int wireless_config_set_keymap(const struct wc_layer_keymap *keymap);

/**
 * Get combo configuration by ID
 */
int wireless_config_get_combo(uint8_t combo_id, struct wc_combo_config *combo);

/**
 * Get all combo configurations
 */
int wireless_config_get_all_combos(struct wc_combo_config *combos, size_t *count);

/**
 * Set combo configuration
 */
int wireless_config_set_combo(const struct wc_combo_config *combo);

/**
 * Get auto-mouse configuration
 */
int wireless_config_get_automouse(struct wc_automouse_config *config);

/**
 * Set auto-mouse configuration
 */
int wireless_config_set_automouse(const struct wc_automouse_config *config);

/**
 * Save current configuration to flash
 */
int wireless_config_save(void);

/**
 * Reset configuration to defaults
 */
int wireless_config_reset(void);

/**
 * Notify a key event to connected clients
 */
void wireless_config_notify_key(const struct wc_key_event *event);

/**
 * Notify a layer change to connected clients
 */
void wireless_config_notify_layer(uint8_t active_layer);
