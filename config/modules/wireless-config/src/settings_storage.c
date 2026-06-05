/*
 * Copyright (c) 2026 minimal-keys
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

#include <wireless_config/wireless_config.h>

LOG_MODULE_DECLARE(wireless_config, CONFIG_WIRELESS_CONFIG_LOG_LEVEL);

#if IS_ENABLED(CONFIG_WIRELESS_CONFIG_PERSIST_SETTINGS)

#define SETTINGS_KEY_HOLDTAP   "wc/holdtap"
#define SETTINGS_KEY_TRACKBALL "wc/trackball"
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

    /* NOTE: keymap persistence is owned by ZMK's runtime keymap (settings key
     * "keymap/...", same path ZMK Studio uses). We intentionally do NOT load or
     * save keymaps here, otherwise a stale wireless-config copy would clobber the
     * ZMK keymap on every boot. See wireless_config.c keymap bridge. */

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

    /* Keymap is persisted by ZMK (zmk_keymap_save_changes), not here. */

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
