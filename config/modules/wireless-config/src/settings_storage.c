/*
 * Copyright (c) 2026 minimal-keys
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include <stdio.h>

#include <wireless_config/wireless_config.h>

LOG_MODULE_DECLARE(wireless_config, CONFIG_WIRELESS_CONFIG_LOG_LEVEL);

#if IS_ENABLED(CONFIG_WIRELESS_CONFIG_PERSIST_SETTINGS)

#define SETTINGS_KEY_HOLDTAP   "wc/holdtap"
#define SETTINGS_KEY_TRACKBALL "wc/trackball"
#define SETTINGS_KEY_KEYMAP    "wc/keymap"
#define SETTINGS_KEY_COMBOS    "wc/combos"
#define SETTINGS_KEY_AUTOMOUSE "wc/automouse"

static int settings_set(const char *name, size_t len, settings_read_cb read_cb,
                        void *cb_arg)
{
    const char *next;
    int rc;

    if (settings_name_steq(name, "holdtap", &next) && !next) {
        struct wc_holdtap_config config;
        if (len != sizeof(config)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &config, sizeof(config));
        if (rc >= 0) {
            wireless_config_set_holdtap(&config);
            LOG_INF("Loaded hold-tap settings from flash");
        }
        return rc;
    }

    if (settings_name_steq(name, "trackball", &next) && !next) {
        struct wc_trackball_config config;
        if (len != sizeof(config)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &config, sizeof(config));
        if (rc >= 0) {
            wireless_config_set_trackball(&config);
            LOG_INF("Loaded trackball settings from flash");
        }
        return rc;
    }

    if (settings_name_steq(name, "keymap", &next) && next) {
        /* keymap/0, keymap/1, etc. */
        uint8_t layer_index = atoi(next);
        if (layer_index >= WC_NUM_LAYERS) {
            return -EINVAL;
        }
        struct wc_layer_keymap keymap;
        if (len != sizeof(keymap)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &keymap, sizeof(keymap));
        if (rc >= 0) {
            keymap.layer_index = layer_index;
            wireless_config_set_keymap(&keymap);
            LOG_INF("Loaded keymap layer %d from flash", layer_index);
        }
        return rc;
    }

    if (settings_name_steq(name, "combos", &next) && !next) {
        struct wc_combo_config combos[WC_MAX_COMBOS];
        if (len != sizeof(combos)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, combos, sizeof(combos));
        if (rc >= 0) {
            for (int i = 0; i < WC_MAX_COMBOS; i++) {
                wireless_config_set_combo(&combos[i]);
            }
            LOG_INF("Loaded combo settings from flash");
        }
        return rc;
    }

    if (settings_name_steq(name, "automouse", &next) && !next) {
        struct wc_automouse_config config;
        if (len != sizeof(config)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &config, sizeof(config));
        if (rc >= 0) {
            wireless_config_set_automouse(&config);
            LOG_INF("Loaded auto-mouse settings from flash");
        }
        return rc;
    }

    return -ENOENT;
}

static struct settings_handler wc_settings_handler = {
    .name = "wc",
    .h_set = settings_set,
};

int wireless_config_settings_init(void)
{
    int err = settings_subsys_init();
    if (err) {
        LOG_ERR("Settings init failed: %d", err);
        return err;
    }

    err = settings_register(&wc_settings_handler);
    if (err) {
        LOG_ERR("Settings register failed: %d", err);
        return err;
    }

    /* Load saved settings */
    settings_load_subtree("wc");

    LOG_INF("Wireless config settings initialized");
    return 0;
}

int wireless_config_settings_save(void)
{
    int err;
    struct wc_holdtap_config ht_config;
    struct wc_trackball_config tb_config;
    struct wc_automouse_config am_config;

    wireless_config_get_holdtap(&ht_config);
    wireless_config_get_trackball(&tb_config);
    wireless_config_get_automouse(&am_config);

    err = settings_save_one(SETTINGS_KEY_HOLDTAP, &ht_config, sizeof(ht_config));
    if (err) {
        LOG_ERR("Failed to save hold-tap settings: %d", err);
        return err;
    }

    err = settings_save_one(SETTINGS_KEY_TRACKBALL, &tb_config, sizeof(tb_config));
    if (err) {
        LOG_ERR("Failed to save trackball settings: %d", err);
        return err;
    }

    /* Save keymaps for each layer */
    for (int i = 0; i < WC_NUM_LAYERS; i++) {
        struct wc_layer_keymap keymap;
        wireless_config_get_keymap(i, &keymap);
        char key[32];
        snprintf(key, sizeof(key), "%s/%d", SETTINGS_KEY_KEYMAP, i);
        err = settings_save_one(key, &keymap, sizeof(keymap));
        if (err) {
            LOG_ERR("Failed to save keymap layer %d: %d", i, err);
            return err;
        }
    }

    /* Save all combos */
    struct wc_combo_config combos[WC_MAX_COMBOS];
    size_t count;
    wireless_config_get_all_combos(combos, &count);
    err = settings_save_one(SETTINGS_KEY_COMBOS, combos, sizeof(combos));
    if (err) {
        LOG_ERR("Failed to save combo settings: %d", err);
        return err;
    }

    /* Save auto-mouse */
    err = settings_save_one(SETTINGS_KEY_AUTOMOUSE, &am_config, sizeof(am_config));
    if (err) {
        LOG_ERR("Failed to save auto-mouse settings: %d", err);
        return err;
    }

    LOG_INF("Settings saved to flash");
    return 0;
}

SYS_INIT(wireless_config_settings_init, APPLICATION, 91);

#endif /* CONFIG_WIRELESS_CONFIG_PERSIST_SETTINGS */
