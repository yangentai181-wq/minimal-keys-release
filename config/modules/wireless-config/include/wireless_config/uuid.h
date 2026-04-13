/*
 * Copyright (c) 2026 minimal-keys
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/bluetooth/uuid.h>

/**
 * Custom BLE UUIDs for wireless configuration service
 * Base UUID: xxxxxxxx-0196-6107-c967-c5cfb1c2482a (same base as ZMK Studio)
 *
 * Using 0x1000 range to avoid collision with ZMK Studio (0x0000-0x0001)
 */

#define WC_BT_UUID(num) BT_UUID_128_ENCODE(num, 0x0196, 0x6107, 0xc967, 0xc5cfb1c2482a)

/* Wireless Config Service UUID: 00001000-0196-6107-c967-c5cfb1c2482a */
#define WC_BT_SERVICE_UUID      WC_BT_UUID(0x00001000)

/* Config Read/Write Characteristic: 00001001-0196-6107-c967-c5cfb1c2482a */
#define WC_BT_CONFIG_CHRC_UUID  WC_BT_UUID(0x00001001)

/* Event Notification Characteristic: 00001002-0196-6107-c967-c5cfb1c2482a */
#define WC_BT_EVENT_CHRC_UUID   WC_BT_UUID(0x00001002)
