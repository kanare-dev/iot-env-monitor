# 配線構成書 — ESP32 + BME280 (I2C)

ブレッドボード試作段階の配線構成。

## 接続表

| BME280 ピン | ESP32 ピン | 信号 | 説明 |
|-------------|-----------|------|------|
| VDD | 3V3 | 電源 | 3.3V 供給 |
| GND | GND | GND | グランド |
| SDI | GPIO21 | SDA | I2C データ |
| SCK | GPIO22 | SCL | I2C クロック |
| CSB | 3V3 | モード選択 | I2C モード固定 |
| SDO | GND | アドレス | I2C アドレス 0x76 |

## I2C 設定

| 項目 | 値 |
|------|-----|
| SDA | GPIO21 |
| SCL | GPIO22 |
| アドレス | 0x76 |
| モード | ESP32 = マスター |

GPIO21/22 は ESP32 のデフォルト I2C ピン (Arduino / ESP-IDF 標準)。

## 物理配置

![ブレッドボード配線](../assets/breadboard.png)

```
ESP32 DevKitC
  │
  ├─ 3V3 ───── BME280 VDD
  ├─ GND ───── BME280 GND
  ├─ GPIO21 ── BME280 SDI (SDA)
  ├─ GPIO22 ── BME280 SCK (SCL)
  ├─ 3V3 ───── BME280 CSB
  └─ GND ───── BME280 SDO
```

## 回路図 (論理)

```
3.3V ──────────────┐
                   │
                BME280
                   │
GPIO21 ────────────┤ SDA
GPIO22 ────────────┤ SCL
                   │
GND ───────────────┘
```

## CSB / SDO ピンの役割

- **CSB → 3V3**: I2C モードを選択 (LOW にすると SPI モードになる)
- **SDO → GND**: I2C アドレスを 0x76 に設定 (3V3 に接続すると 0x77)

## 確認手順

配線完了後、ファームウェアを書き込んで I2C スキャンで `0x76` が検出されれば接続成功。

```bash
cd firmware/esp32
pio run --target upload
pio device monitor
```

期待される出力:

```
Scanning I2C bus...
  Found device at 0x76
Scan complete: 1 device(s) found

BME280 initialized successfully.

Temperature: 22.6 °C
Humidity:    35.0 %
Pressure:    1019.6 hPa
```

ファームウェア (`firmware/esp32/src/main.cpp`) は起動時に自動で I2C スキャンを実行し、
BME280 の初期化後に 5 秒ごとにセンサ値をシリアル出力する。

## 部品一覧

| 部品 | データシート / 製品ページ |
|------|------------------------|
| ESP32 DevKitC (ESP-WROOM-32) | [ESP32-WROOM-32 Datasheet (Espressif)](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf) |
| AE-BME280 (秋月電子) | [AE-BME280 製品ページ (秋月電子)](https://akizukidenshi.com/catalog/g/g109421/) / [BME280 Datasheet (Bosch)](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf) |
| ブレッドボード | — |
| ジャンパーワイヤ (オス-オス) × 6 本 | — |
| USB ケーブル (Micro-B or Type-C) | ボードに合わせる |
