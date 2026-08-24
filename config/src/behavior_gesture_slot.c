/* SPDX-License-Identifier: MIT */

#define DT_DRV_COMPAT minimal_keys_behavior_gesture_slot

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>

#define GESTURE_LAYER_INDEX 9

static bool is_allowed_slot(uint32_t position) {
    return position == 7 || position == 18 || position == 20 || position == 31;
}

static int invoke_slot(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event, bool pressed) {
    if (!is_allowed_slot(binding->param1)) {
        return -EINVAL;
    }

    zmk_keymap_layer_id_t layer_id = zmk_keymap_layer_index_to_id(GESTURE_LAYER_INDEX);
    if (layer_id == ZMK_KEYMAP_LAYER_ID_INVAL) {
        return -ENODEV;
    }

    const struct zmk_behavior_binding *target =
        zmk_keymap_get_layer_binding_at_idx(layer_id, binding->param1);
    if (!target) {
        return -ENOENT;
    }
    if (strcmp(target->behavior_dev, binding->behavior_dev) == 0) {
        return -ELOOP;
    }

    event.layer = layer_id;
    event.position = binding->param1;
    return zmk_behavior_invoke_binding(target, event, pressed);
}

static int gesture_slot_pressed(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    return invoke_slot(binding, event, true);
}

static int gesture_slot_released(struct zmk_behavior_binding *binding,
                                 struct zmk_behavior_binding_event event) {
    return invoke_slot(binding, event, false);
}

static const struct behavior_driver_api behavior_gesture_slot_driver_api = {
    .binding_pressed = gesture_slot_pressed,
    .binding_released = gesture_slot_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_gesture_slot_driver_api);
