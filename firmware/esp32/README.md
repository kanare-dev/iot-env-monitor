# ESP32 Firmware — Environment Monitor

BME280 から温度・湿度・気圧を読み取り、Wi-Fi 経由で Prometheus メトリクスとして公開する。

## Setup

```bash
cp src/config.example.h src/config.h
# config.h を編集して Wi-Fi SSID / パスワード / 静的 IP を設定

pip install platformio
pio run --target upload
pio device monitor
```

## Wi-Fi 要件

- **2.4 GHz のみ** (ESP32 は 5 GHz 非対応)
- **WPA2** (WPA3 非対応)

## HTTP エンドポイント

| パス | Content-Type | 内容 |
| ------ | ------------- | ------ |
| `GET /` | `text/html` | ブラウザ用ダッシュボード (5 秒自動更新) |
| `GET /metrics` | `text/plain` | Prometheus exposition format |

## Prometheus メトリクス

| メトリクス | 型 | 単位 | 説明 |
| ----------- | ----- | ------ | ------ |
| `env_temperature_celsius` | gauge | °C | 温度 |
| `env_humidity_percent` | gauge | % | 相対湿度 |
| `env_pressure_hpa` | gauge | hPa | 気圧 |
| `env_bad_reads_total` | counter | — | 破棄された異常読み取りの累計 |

## センサ値バリデーション

BME280 は I2C バスエラーやレジスタ読み取りタイミングにより、不正な値を返すことがある。
特にブレッドボード環境では接触不良による突発的な異常値が発生しやすい。

既知の現象:

- 湿度が瞬間的に 100% になる (レジスタが `0xFFFF` を返す)
- I2C Error 263 発生時に NaN が返る
- 起動直後の初回読み取りが不安定

### 範囲チェック (BME280 仕様範囲)

| 項目 | 最小 | 最大 |
| ------ | ------ | ------ |
| 温度 | -40 °C | 85 °C |
| 湿度 | 0 % | 100 % |
| 気圧 | 300 hPa | 1100 hPa |

### 変化率チェック (1 読み取り間隔 = 5 秒あたり)

| 項目 | 最大変化量 |
| ------ | ----------- |
| 温度 | ±5 °C |
| 湿度 | ±10 % |
| 気圧 | ±20 hPa |

室内環境で 5 秒間にこれらの閾値を超える変動は物理的にあり得ないため、超えた場合はセンサのエラーとみなす。

### 異常値検出時の動作

1. 読み取り値を破棄し、前回の正常値を維持
2. シリアルに `[WARN] Bad reading discarded` を出力
3. `env_bad_reads_total` カウンターをインクリメント

`env_bad_reads_total` が継続的に増加する場合、配線の接触不良またはセンサの劣化が疑われる。

## 初期化シーケンス

```plaintext
1. I2C バスを初期化 (GPIO21=SDA, GPIO22=SCL)
2. I2C スキャン (0x76 の検出を確認)
3. I2C バスをリセット (スキャン後の残留状態をクリア)
4. BME280 初期化 (最大 3 回リトライ)
5. Wi-Fi 接続 (静的 IP、最大 20 秒タイムアウト)
6. HTTP サーバ起動
```

I2C スキャン直後に `Wire.end()` → `Wire.begin()` でバスをリセットしている理由:
スキャンが 127 デバイスに対して連続でトランザクションを発行するため、
バスの状態が不安定になり BME280 の初期化が失敗することがある。

## ファイル構成

```plaintext
esp32/
├── platformio.ini       # ボード・ライブラリ定義
└── src/
    ├── main.cpp           # ファームウェア本体
    ├── config.h           # Wi-Fi / ネットワーク設定 (gitignored)
    └── config.example.h   # config.h のテンプレート
```

## config.h の設定項目

```cpp
#define WIFI_SSID    "your-ssid"        // Wi-Fi SSID (2.4 GHz)
#define WIFI_PASS    "your-password"    // Wi-Fi パスワード

#define STATIC_IP    192, 168, 11, 100   // ESP32 の固定 IP
#define GATEWAY      192, 168, 11, 1     // ゲートウェイ
#define SUBNET       255, 255, 255, 0    // サブネットマスク
#define DNS_PRIMARY  192, 168, 11, 1     // DNS サーバ
```
