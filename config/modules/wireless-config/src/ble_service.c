/*
 * Copyright (c) 2026 minimal-keys
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#include <wireless_config/wireless_config.h>
#include <wireless_config/uuid.h>

LOG_MODULE_DECLARE(wireless_config, CONFIG_WIRELESS_CONFIG_LOG_LEVEL);

/* Message framing (compatible with ZMK Studio protocol) */
#define MSG_SOF 0xAB
#define MSG_ESC 0xAC
#define MSG_EOF 0xAD

/* Receive buffer */
#define RX_BUF_SIZE 256
static uint8_t rx_buffer[RX_BUF_SIZE];
static size_t rx_len = 0;
static bool rx_escaped = false;
static bool rx_in_frame = false;

/* Transmit buffer */
#define TX_BUF_SIZE 256
static uint8_t tx_buffer[TX_BUF_SIZE];

/* Protocol version */
#define PROTOCOL_VERSION_MAJOR 1
#define PROTOCOL_VERSION_MINOR 0

/* Forward declarations */
static void process_message(const uint8_t *data, size_t len);
static ssize_t send_response(uint8_t cmd, uint8_t status, const uint8_t *data, size_t len);

/* Notification state */
static bool config_notify_enabled = false;
static bool event_notify_enabled = false;
static struct bt_conn *active_conn = NULL;

static void config_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    config_notify_enabled = (value == BT_GATT_CCC_INDICATE);
    LOG_INF("Config notifications %s", config_notify_enabled ? "enabled" : "disabled");
}

static void event_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    event_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Event notifications %s", event_notify_enabled ? "enabled" : "disabled");
}

static ssize_t read_config(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           void *buf, uint16_t len, uint16_t offset)
{
    LOG_DBG("Config read: len=%d, offset=%d", len, offset);

    /* Return protocol version on read */
    uint8_t version[] = {PROTOCOL_VERSION_MAJOR, PROTOCOL_VERSION_MINOR};
    return bt_gatt_attr_read(conn, attr, buf, len, offset, version, sizeof(version));
}

static ssize_t write_config(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    const uint8_t *data = buf;

    LOG_DBG("Config write: len=%d", len);

    active_conn = conn;

    /* Process framed message */
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];

        if (!rx_in_frame) {
            if (byte == MSG_SOF) {
                rx_in_frame = true;
                rx_len = 0;
                rx_escaped = false;
            }
            continue;
        }

        if (byte == MSG_EOF) {
            /* End of frame - process message */
            process_message(rx_buffer, rx_len);
            rx_in_frame = false;
            rx_len = 0;
            continue;
        }

        if (byte == MSG_ESC) {
            rx_escaped = true;
            continue;
        }

        if (rx_escaped) {
            rx_escaped = false;
        }

        if (rx_len < RX_BUF_SIZE) {
            rx_buffer[rx_len++] = byte;
        }
    }

    return len;
}

/* GATT Service Definition */
BT_GATT_SERVICE_DEFINE(wireless_config_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(WC_BT_SERVICE_UUID)),

    /* Config characteristic (Read/Write/Indicate) */
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(WC_BT_CONFIG_CHRC_UUID),
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE,
                           BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
                           read_config, write_config, NULL),
    BT_GATT_CCC(config_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),

    /* Event characteristic (Notify only) */
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(WC_BT_EVENT_CHRC_UUID),
                           BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE,
                           NULL, NULL, NULL),
    BT_GATT_CCC(event_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
);

/* Frame and send data */
static size_t frame_data(uint8_t *out, size_t out_size,
                         const uint8_t *data, size_t len)
{
    size_t pos = 0;

    if (pos < out_size) out[pos++] = MSG_SOF;

    for (size_t i = 0; i < len && pos < out_size - 2; i++) {
        uint8_t byte = data[i];
        if (byte == MSG_SOF || byte == MSG_ESC || byte == MSG_EOF) {
            out[pos++] = MSG_ESC;
        }
        out[pos++] = byte;
    }

    if (pos < out_size) out[pos++] = MSG_EOF;

    return pos;
}

static ssize_t send_response(uint8_t cmd, uint8_t status, const uint8_t *data, size_t len)
{
    if (!active_conn || !config_notify_enabled) {
        return -ENOTCONN;
    }

    /* Build response: [cmd][status][data...] */
    uint8_t response[64];
    size_t resp_len = 0;

    response[resp_len++] = cmd;
    response[resp_len++] = status;

    if (data && len > 0 && resp_len + len < sizeof(response)) {
        memcpy(&response[resp_len], data, len);
        resp_len += len;
    }

    /* Frame the response */
    size_t frame_len = frame_data(tx_buffer, sizeof(tx_buffer), response, resp_len);

    /* Send via GATT indicate */
    struct bt_gatt_indicate_params params = {
        .attr = &wireless_config_svc.attrs[1],
        .data = tx_buffer,
        .len = frame_len,
    };

    return bt_gatt_indicate(active_conn, &params);
}

static void process_message(const uint8_t *data, size_t len)
{
    if (len < 1) {
        return;
    }

    uint8_t cmd = data[0];
    const uint8_t *payload = data + 1;
    size_t payload_len = len - 1;

    LOG_DBG("Processing cmd=0x%02x, payload_len=%d", cmd, payload_len);

    switch (cmd) {
    case WC_CMD_GET_VERSION: {
        uint8_t version[] = {PROTOCOL_VERSION_MAJOR, PROTOCOL_VERSION_MINOR};
        send_response(cmd, WC_STATUS_OK, version, sizeof(version));
        break;
    }

    case WC_CMD_HOLDTAP: {
        if (payload_len == 0) {
            /* Get current config */
            struct wc_holdtap_config config;
            wireless_config_get_holdtap(&config);
            send_response(cmd, WC_STATUS_OK, (uint8_t *)&config, sizeof(config));
        } else if (payload_len >= sizeof(struct wc_holdtap_config)) {
            /* Set new config */
            const struct wc_holdtap_config *config =
                (const struct wc_holdtap_config *)payload;
            int ret = wireless_config_set_holdtap(config);
            send_response(cmd, ret == 0 ? WC_STATUS_OK : WC_STATUS_ERROR, NULL, 0);
        } else {
            send_response(cmd, WC_STATUS_INVALID_PARAM, NULL, 0);
        }
        break;
    }

    case WC_CMD_TRACKBALL: {
        if (payload_len == 0) {
            /* Get current config */
            struct wc_trackball_config config;
            wireless_config_get_trackball(&config);
            send_response(cmd, WC_STATUS_OK, (uint8_t *)&config, sizeof(config));
        } else if (payload_len >= sizeof(struct wc_trackball_config)) {
            /* Set new config */
            const struct wc_trackball_config *config =
                (const struct wc_trackball_config *)payload;
            int ret = wireless_config_set_trackball(config);
            send_response(cmd, ret == 0 ? WC_STATUS_OK : WC_STATUS_ERROR, NULL, 0);
        } else {
            send_response(cmd, WC_STATUS_INVALID_PARAM, NULL, 0);
        }
        break;
    }

    case WC_CMD_SAVE: {
        int ret = wireless_config_save();
        send_response(cmd, ret == 0 ? WC_STATUS_OK : WC_STATUS_ERROR, NULL, 0);
        break;
    }

    case WC_CMD_RESET: {
        int ret = wireless_config_reset();
        send_response(cmd, ret == 0 ? WC_STATUS_OK : WC_STATUS_ERROR, NULL, 0);
        break;
    }

    default:
        LOG_WRN("Unknown command: 0x%02x", cmd);
        send_response(cmd, WC_STATUS_INVALID_CMD, NULL, 0);
        break;
    }
}

/* Public API for sending key events */
void wireless_config_notify_key(const struct wc_key_event *event)
{
#if IS_ENABLED(CONFIG_WIRELESS_CONFIG_NOTIFY_KEY_EVENTS)
    if (!active_conn || !event_notify_enabled) {
        return;
    }

    uint8_t data[5];
    data[0] = WC_EVT_KEY;
    data[1] = event->position;
    data[2] = event->pressed ? 1 : 0;
    data[3] = event->layer;
    data[4] = event->modifiers;

    size_t frame_len = frame_data(tx_buffer, sizeof(tx_buffer), data, sizeof(data));

    bt_gatt_notify(active_conn, &wireless_config_svc.attrs[4], tx_buffer, frame_len);
#endif
}

void wireless_config_notify_layer(uint8_t active_layer)
{
#if IS_ENABLED(CONFIG_WIRELESS_CONFIG_NOTIFY_KEY_EVENTS)
    if (!active_conn || !event_notify_enabled) {
        return;
    }

    uint8_t data[2];
    data[0] = WC_EVT_LAYER_CHANGE;
    data[1] = active_layer;

    size_t frame_len = frame_data(tx_buffer, sizeof(tx_buffer), data, sizeof(data));

    bt_gatt_notify(active_conn, &wireless_config_svc.attrs[4], tx_buffer, frame_len);
#endif
}
