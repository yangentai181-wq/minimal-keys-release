# wireless-config ランタイム本実装 計画 / 診断記録

最終更新: 2026-06-05 / ブランチ: `feat/wireless-config-runtime`（off `feature/raw-hid-monitor`）

## 背景（なぜ専用Webアプリのエディタが「全く動かない」のか）

専用Web設定アプリ `~/repos/minimal-keys-config`（React+TS+Vite, Web Bluetooth）が
キーボードへ繋ぐ BLE GATT 設定サービス `00001000-0196-...` について、診断の結果:

1. **設定サービスのファームが2系統に分岐し、実キーマップと同居していなかった**
   - `main`（@3f0cc81）: wireless-config モジュール=有（packed/NOTIFY/接続安定化済）だが素ZMK v0.3.0・実キーマップ無し
   - `feature/raw-hid-monitor` / `feature/scroll-on-space-hold`（実運用ファーム）: 実キーマップ・raw-hid 有だが **wireless-config=無**
   - → 実機に焼かれているファームに設定サービスが無い → 接続初期通信が成立せず全エディタ不動
2. **設定サービスの中身がスタブだった**
   - `wireless_config.c` の set/get_keymap 等は RAM 配列に置くだけで `TODO: Apply to ZMK runtime`。
     ZMK ランタイムへ一切反映されず、get はゼロ（透過）を返すだけ。
3. キーボードは既に `CONFIG_ZMK_STUDIO=y`（公式ランタイム編集）有効。自作スタックはこれを不完全に再発明していた。

## 方針（岩根決定: 本実装する）

実運用ブランチへ wireless-config を統合し、**keymap を ZMK ランタイム API に本接続**する。

### スコープ

- ✅ **keymap エディタ**: 本実装。ZMK Studio が使う runtime keymap API を利用
  - `zmk_keymap_layer_index_to_id` / `zmk_keymap_get_layer_binding_at_idx`
  - `zmk_keymap_set_layer_binding_at_idx` / `zmk_keymap_save_changes` / `zmk_keymap_reset_settings`
- ❌ **hold-tap timing / combo**: ZMK では原則コンパイル時定義。ランタイム変更 API 無し → スコープ外（ファーム側はスタブのまま温存・Web側UIで無効化）
- ❌/⚠️ **trackball CPI / scale / scroll**: PMW3610 `set_cpi` は static 非公開、scale/invert/scroll は入力プロセッサのコンパイル時設定 → 現状ランタイム不可。スコープ外

## 実装内容（このブランチ）

- `config/modules/wireless-config/**` を `main` から取り込み
- ルート `zephyr/module.yml` にモジュール登録（cmake/kconfig）
- `minimal-keys_R.conf` に `CONFIG_WIRELESS_CONFIG=y` 等を追記（既存 raw-hid/scroll 設定は温存）
- `wireless_config.c`:
  - keymap 変換ブリッジ追加（web u16 ⇔ `zmk_behavior_binding`）
    - `0x0000`→`&trans` / `0x2000|layer<<8|tap`→`&lt` / `0x1000|layer`→`&mo` / `0x1100|layer`→`&tg` / その他→`&kp`（HIDキーボードページ0x07仮定）
    - `CONFIG_ZMK_BEHAVIOR_LOCAL_IDS_IN_BINDINGS` 時は `local_id` も設定
  - get/set_keymap を ZMK ランタイムに接続、save で `zmk_keymap_save_changes()`、reset で `zmk_keymap_reset_settings()`
- `settings_storage.c`: keymap の二重永続化を除去（ZMK が所有。さもないと boot 時に ZMK キーマップを上書きする）

### 既知の限界 / round-trip

- 平キーは HID キーボードページ(0x07)固定。Consumer 等他ページ・`{kp,mo,lt,tg,trans}` 以外の behavior は `0x0000` に潰れる
- ZMK include 伝播（`<zmk/keymap.h>` がモジュールライブラリから見えるか）は CI ビルドで要検証

## 検証

- GitHub Actions で R/L ビルド（production west.yml = hyhy-masa fork e4253ecd51）
- ビルド緑 → R 半分(central)を uf2 で再フラッシュ → Web アプリ接続 → keymap load/edit/apply/save 通し確認

## 関連

- Web アプリ: `~/repos/minimal-keys-config`（別途 framing バグ修正・acceptAllDevices 除去・未対応エディタ無効化）
- 診断時の Codex 委譲ログ: タイムアウトで最終レポート欠落、ファーム照合は Claude 側で実体確認・補正
