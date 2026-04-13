/*
 * Copyright (c) 2026 minimal-keys
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

#include <wireless_config/wireless_config.h>

LOG_MODULE_DECLARE(wireless_config, CONFIG_WIRELESS_CONFIG_LOG_LEVEL);

#if IS_ENABLED(CONFIG_WIRELESS_CONFIG_PERSIST_SETTINGS)

#define SETTINGS_KEY_HOLDTAP   "wc/holdtap"
#define SETTINGS_KEY_TRACKBALL "wc/trackball"

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

    wireless_config_get_holdtap(&ht_config);
    wireless_config_get_trackball(&tb_config);

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

    LOG_INF("Settings saved to flash");
    return 0;
}

SYS_INIT(wireless_config_settings_init, APPLICATION);

#endif /* CONFIG_WIRELESS_CONFIG_PERSIST_SETTINGS */
