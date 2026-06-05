/*
 * Copyright (c) 2026 minimal-keys
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <zmk/keymap.h>
#include <zmk/behavior.h>

#include <wireless_config/wireless_config.h>

LOG_MODULE_REGISTER(wireless_config, CONFIG_WIRELESS_CONFIG_LOG_LEVEL);

/*
 * Keymap bridge: translate between the web app's u16 keycode scheme and ZMK's
 * runtime behavior bindings (the same runtime keymap that ZMK Studio edits).
 *
 * Web u16 scheme (see KeymapEditor.tsx getKeycodeLabel):
 *   0x0000               -> &trans (transparent)
 *   (kc & 0xF000)==0x2000 -> &lt  layer=(kc>>8)&0x0F  tap=kc&0xFF
 *   (kc & 0xFF00)==0x1000 -> &mo  layer=kc&0xFF
 *   (kc & 0xFF00)==0x1100 -> &tg  layer=kc&0xFF
 *   otherwise            -> &kp  (HID keyboard-page usage = kc & 0xFF)
 *
 * Limitation: plain keys assume the HID keyboard usage page (0x07). Consumer /
 * other pages and behaviors outside {kp,mo,lt,tg,trans} round-trip as 0x0000.
 */
#define WC_KB_PAGE 0x07u
#define WC_KEYCODE_FROM_USAGE(u) (((uint32_t)WC_KB_PAGE << 16) | ((u) & 0xFFu))
#define WC_USAGE_FROM_KEYCODE(kc) ((uint16_t)((kc) & 0xFFu))

/* Resolve standard ZMK behavior device names at compile time. */
#define WC_DEV(label) DEVICE_DT_NAME(DT_NODELABEL(label))
static const char *const WC_DEV_KP = WC_DEV(kp);
static const char *const WC_DEV_MO = WC_DEV(mo);
static const char *const WC_DEV_LT = WC_DEV(lt);
static const char *const WC_DEV_TG = WC_DEV(tg);
static const char *const WC_DEV_TRANS = WC_DEV(trans);

static struct zmk_behavior_binding wc_make_binding(const char *dev, uint32_t p1, uint32_t p2)
{
    struct zmk_behavior_binding b = {0};
    b.behavior_dev = dev;
    b.param1 = p1;
    b.param2 = p2;
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_LOCAL_IDS_IN_BINDINGS)
    b.local_id = dev ? zmk_behavior_get_local_id(dev) : 0;
#endif
    return b;
}

/* web u16 keycode -> ZMK binding */
static struct zmk_behavior_binding wc_keycode_to_binding(uint16_t kc)
{
    if (kc == 0x0000) {
        return wc_make_binding(WC_DEV_TRANS, 0, 0);
    }
    if ((kc & 0xF000u) == 0x2000u) {
        return wc_make_binding(WC_DEV_LT, (kc >> 8) & 0x0Fu,
                               WC_KEYCODE_FROM_USAGE(kc & 0xFFu));
    }
    if ((kc & 0xFF00u) == 0x1000u) {
        return wc_make_binding(WC_DEV_MO, kc & 0xFFu, 0);
    }
    if ((kc & 0xFF00u) == 0x1100u) {
        return wc_make_binding(WC_DEV_TG, kc & 0xFFu, 0);
    }
    return wc_make_binding(WC_DEV_KP, WC_KEYCODE_FROM_USAGE(kc & 0xFFu), 0);
}

/* ZMK binding -> web u16 keycode */
static uint16_t wc_binding_to_keycode(const struct zmk_behavior_binding *b)
{
    const char *dev = b ? b->behavior_dev : NULL;

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_LOCAL_IDS_IN_BINDINGS)
    if (dev == NULL && b != NULL) {
        dev = zmk_behavior_find_behavior_name_from_local_id(b->local_id);
    }
#endif
    if (dev == NULL) {
        return 0x0000;
    }
    if (strcmp(dev, WC_DEV_KP) == 0) {
        return WC_USAGE_FROM_KEYCODE(b->param1);
    }
    if (strcmp(dev, WC_DEV_MO) == 0) {
        return 0x1000u | (b->param1 & 0xFFu);
    }
    if (strcmp(dev, WC_DEV_TG) == 0) {
        return 0x1100u | (b->param1 & 0xFFu);
    }
    if (strcmp(dev, WC_DEV_LT) == 0) {
        return 0x2000u | ((b->param1 & 0x0Fu) << 8) | WC_USAGE_FROM_KEYCODE(b->param2);
    }
    /* &trans, &none, and any unrepresentable behavior collapse to 0x0000. */
    return 0x0000;
}

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

/* Keymap is owned by ZMK's runtime keymap (see keymap bridge above); we do not
 * keep a local copy. Combos / hold-tap / trackball remain local stubs. */

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

    zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(layer_index);
    if (id == ZMK_KEYMAP_LAYER_ID_INVAL) {
        return -EINVAL;
    }

    keymap->layer_index = layer_index;
    for (uint8_t pos = 0; pos < WC_NUM_KEYS; pos++) {
        const struct zmk_behavior_binding *b =
            zmk_keymap_get_layer_binding_at_idx(id, pos);
        keymap->keycodes[pos] = wc_binding_to_keycode(b);
    }
    return 0;
}

int wireless_config_set_keymap(const struct wc_layer_keymap *keymap)
{
    if (!keymap || keymap->layer_index >= WC_NUM_LAYERS) {
        return -EINVAL;
    }

    zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(keymap->layer_index);
    if (id == ZMK_KEYMAP_LAYER_ID_INVAL) {
        return -EINVAL;
    }

    int last_err = 0;
    for (uint8_t pos = 0; pos < WC_NUM_KEYS; pos++) {
        struct zmk_behavior_binding b = wc_keycode_to_binding(keymap->keycodes[pos]);
        int ret = zmk_keymap_set_layer_binding_at_idx(id, pos, b);
        if (ret < 0) {
            last_err = ret;
            LOG_WRN("set binding failed: layer=%d pos=%d err=%d",
                    keymap->layer_index, pos, ret);
        }
    }
    LOG_INF("Keymap applied to ZMK runtime: layer=%d", keymap->layer_index);
    return last_err;
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
    /* Persist the ZMK runtime keymap (owned by ZMK, same path as Studio). */
    int km_err = zmk_keymap_save_changes();
    if (km_err < 0) {
        LOG_ERR("zmk_keymap_save_changes failed: %d", km_err);
    }

#if IS_ENABLED(CONFIG_WIRELESS_CONFIG_PERSIST_SETTINGS)
    LOG_INF("Saving configuration to flash");
    int err = wireless_config_settings_save();
    return err ? err : km_err;
#else
    return km_err;
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

    /* Reset ZMK runtime keymap back to the compiled-in defaults. */
    zmk_keymap_reset_settings();

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
