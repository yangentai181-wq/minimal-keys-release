/*
 * Copyright (c) 2026 minimal-keys
 * SPDX-License-Identifier: MIT
 *
 * ZMK event listener that forwards key press/release and layer change
 * events to the wireless config BLE service for real-time monitoring.
 */

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#include <wireless_config/wireless_config.h>

#if IS_ENABLED(CONFIG_WIRELESS_CONFIG_NOTIFY_KEY_EVENTS)

static uint8_t current_layer = 0;

static int on_position_state_changed(const zmk_event_t *eh)
{
    struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    struct wc_key_event event = {
        .position = ev->position,
        .pressed = ev->state,
        .layer = current_layer,
        .modifiers = 0,
    };

    wireless_config_notify_key(&event);

    return ZMK_EV_EVENT_BUBBLE;
}

static int on_layer_state_changed(const zmk_event_t *eh)
{
    struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);

    if (ev->state) {
        current_layer = ev->layer;
        wireless_config_notify_layer(ev->layer);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(wireless_config_key, on_position_state_changed);
ZMK_SUBSCRIPTION(wireless_config_key, zmk_position_state_changed);

ZMK_LISTENER(wireless_config_layer, on_layer_state_changed);
ZMK_SUBSCRIPTION(wireless_config_layer, zmk_layer_state_changed);

#endif /* CONFIG_WIRELESS_CONFIG_NOTIFY_KEY_EVENTS */
