# 右手USB Studio RPC / Raw HID 契約の検証手順

作成日: 2026-07-04
関連: minimal-keys-studio `docs/handoffs/2026-07-03-minimal-keys-studio-connection-troubleshooting.md`

## 結論（実機で確定済み）

**USB CDC ACM は Studio RPC endpoint として正しく機能している。**
「USBシリアルは開けるのに Studio RPC 無応答」の根本原因はファーム設定の欠陥ではなく、
**ZMK Studio RPC が「選択中の出力エンドポイント」の transport でしか待ち受けない**仕様による。

- 根拠コード: `hyhy-masa/zmk@957c4b0c app/src/studio/rpc.c` の
  `refresh_selected_transport()`（L271-306）。
  `zmk_endpoints_selected().transport` に一致する transport だけ `rx_start()` され、
  他方は `rx_stop()` される。出力先がBLEのとき、USB CDC は開けても RPC frame は読まれない。
- 実機差分実験（同一UF2・同一ケーブル）:
  - 2026-07-02/03: Mac と BLE 接続中（出力先BLE）→ CDC へ `core.getDeviceInfo` frame
    送信で **0 byte 応答**（baud 9600/115200/12500、DTR/RTS 全て無効果）
  - 2026-07-04: BLE ペアリング解除後（出力先USB）→ 同じ frame
    `ab08011a020801ad` に対し即応答:
    `ab0a1e08011a1a0a180a0c6d696e696d616c2d6b6579731208f2a88ebccbc3757aad`
    = `Response{requestResponse{requestId:1, core{getDeviceInfo{name:"minimal-keys",
serialNumber:f2a88ebccbc3757a}}}}`。9600 でも同一応答（CDCはbaud非依存）。

## 静的検証（snippet と overlay の関係）

`studio-rpc-usb-uart` snippet（`hyhy-masa/zmk@957c4b0c app/snippets/studio-rpc-usb-uart/`）:

```yaml
# snippet.yml
append:
  DTS_EXTRA_CPPFLAGS: -DZMK_BEHAVIORS_KEEP_ALL
  EXTRA_DTC_OVERLAY_FILE: studio-rpc-usb-uart.overlay
  EXTRA_CONF_FILE: studio-rpc-usb-uart.conf
```

```dts
/* studio-rpc-usb-uart.overlay: 自前の CDC-ACM ノードを zephyr_udc0 に生やす */
/ { chosen { zmk,studio-rpc-uart = &snippet_studio_rpc_usb_uart; }; };
&zephyr_udc0 {
    snippet_studio_rpc_usb_uart: snippet_studio_rpc_usb_uart {
        compatible = "zephyr,cdc-acm-uart";
    };
};
```

- `&xiao_serial { status = "disabled"; };`（minimal-keys_R.overlay）は **衝突しない**。
  `xiao_serial` は XIAO の物理UARTピンのノードであり、snippet は USB device controller
  (`zephyr_udc0`) 配下に**独自の** CDC-ACM ノードを作る。
- Kconfig 側は `ZMK_STUDIO_TRANSPORT_UART` が
  `default y if $(dt_chosen_enabled,zmk,studio-rpc-uart)` で自動有効化
  （app/src/studio/Kconfig）。snippet conf は `CONFIG_ZMK_USB=y / USB_CDC_ACM=y /
UART_INTERRUPT_DRIVEN=y` 等を足す。
- `CONFIG_ZMK_STUDIO_TRANSPORT_BLE=y`（minimal-keys_R.conf）とUARTは**共存**する。
  ただし上記のとおり、待ち受けは常にどちらか片方のみ。

## 実機に入っているUF2の同定

2026-07-04 実機（macOS）:

- `ioreg -p IOUSB`: `minimal-keys@…` idVendor 0x1d50 / idProduct 0x615e /
  serial `F2A88EBCCBC3757A`
- `/dev/cu.usbmodem1101` 出現 = snippet の CDC-ACM が生えている
- `hidutil list`: usagePage 65376(0xff60) / usage 97(0x61) の vendor HID あり
  = `raw_hid_adapter` が右手ファームに入っている
- CDC への RPC プローブ応答（上記）= `studio-rpc-usb-uart` 有効

→ 実機の右手UF2は build.yaml の
`minimal-keys_R rgbled_adapter raw_hid_adapter + studio-rpc-usb-uart` 産物で確定。
artifact 名は今回 `minimal-keys_R-usb-studio-raw-hid` に明示した。

## 今回のファーム側修正

1. `config/minimal-keys.keymap` L6(Bluetoothレイヤー) 右手 Row1 に
   `&out OUT_USB / &out OUT_BLE / &out OUT_TOG` を追加。
   出力先がBLEに固定されて USB Studio RPC が無応答になった際、
   キーボード単体でUSBへ戻せる脱出口（従来は `&out` が一切なく、
   BLEペアリング削除以外に手段がなかった）。
2. `build.yaml` に artifact-name を付与（UF2の取り違え防止）。

## ビルド後の確認手順（GitHub Actions artifact / ローカルwestビルド共通）

ビルドディレクトリで:

```bash
# 1. .config で transport が両方有効なこと
grep -E "ZMK_STUDIO(_RPC)?=|ZMK_STUDIO_TRANSPORT_(UART|BLE)=|USB_CDC_ACM=" build/zephyr/.config
# 期待: CONFIG_ZMK_STUDIO=y / CONFIG_ZMK_STUDIO_RPC=y /
#       CONFIG_ZMK_STUDIO_TRANSPORT_UART=y / CONFIG_ZMK_STUDIO_TRANSPORT_BLE=y /
#       CONFIG_USB_CDC_ACM=y

# 2. 最終 devicetree に chosen と CDC ノードがあること
grep -A2 "zmk,studio-rpc-uart" build/zephyr/zephyr.dts
grep -B2 -A4 "snippet_studio_rpc_usb_uart" build/zephyr/zephyr.dts

# 3. Raw HID (右手のみ)
grep -E "ZMK_RAW_HID|RAW_HID" build/zephyr/.config
```

GitHub Actions では build log の `west build` コマンド行に
`-S studio-rpc-usb-uart` が入っていることも確認する。

## 実機での接続検証手順（フラッシュ後）

```bash
# 0. ポート占有確認
ls /dev/cu.usbmodem*
lsof /dev/cu.usbmodem*          # 出力なし = 誰も掴んでいない

# 1. USB記述子と Raw HID interface
ioreg -p IOUSB -l -w0 | grep -A8 "minimal-keys@"
hidutil list | grep -i minimal   # usagePage 65376 / usage 97 の行があること

# 2. Studio RPC プローブ（無応答なら出力先がBLEの可能性が最有力）
python3 minimal-keys-studio/scripts/studio_rpc_probe.py /dev/cu.usbmodemXXXX
# 応答例: ab0a1e08011a1a0a...ad（getDeviceInfo応答）

# 3. 無応答の場合の切り分け（順に）
#   a. L6(BTレイヤー)の &out OUT_USB を押して再プローブ
#   b. blueutil --paired --format json で他ホストとのBLE接続を確認・切断
#   c. それでも無応答なら初めてファーム焼き直しを検討
```

Raw HID の受信確認はブラウザで minimal-keys Studio の「右手USBで接続」→
モニターにキー押下が映ることで行う（Python でやる場合は `pip install hidapi` 後、
vendor_id=0x1d50, product_id=0x615e, usage_page=0xff60 を open して
先頭バイト 0xf1/0xff/0xf2/0xf3 のレポートを読む）。

## 運用上の注意

- 出力先（endpoint preference）は settings に永続化される
  （`endpoints/preferred`）。settings_reset を焼くとUSB優先のデフォルトへ戻る。
- アプリ側は「Serialは開けたがRPC無応答」を `serial_open_but_rpc_unavailable`
  状態として区別し、モニター（Raw HID）は継続利用できる設計になっている。
