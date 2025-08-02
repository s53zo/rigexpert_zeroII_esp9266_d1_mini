# Zero-II ↔ ESP8266 MQTT Bridge

A **head-less Wi-Fi gateway** that turns the **RigExpert AA Zero-II** antenna analyser into a network-connected sensor.  
Send a frequency (Hz) to an MQTT topic, the ESP8266 measures and replies with a JSON block that contains R, X, SWR, RL, |Z| and phase.  
No LCD, no rotary encoder – perfect for Node-RED, InfluxDB/Grafana, Python scripts, …

| File | Purpose |
|------|---------|
| **`zeroii_mqtt.ino`** | main sketch – Wi-Fi, captive web portal, OTA, MQTT, I²C bridge |
| **`eeprom_load_save.h`** | 96 B helpers that keep Wi-Fi / MQTT credentials in EEPROM |
| **`wifi_web_ota_blob.h`** | minimal captive portal & OTA update page |

---

## 1 Hardware

| Qty | Part | Notes |
|-----|------|-------|
| 1 | LOLIN / Wemos **D1 mini** (ESP-8266-12F) | 3.3 V logic |
| 1 | **RigExpert Zero II** PCB | flashed with factory firmware 1.0
| 1 | **4-ch MOSFET level-shifter** | I²C + RST lines (5 V ⇄ 3.3 V) |
| – | 5 V supply, SMA OSL kit | (for later calibration) |

Zero-II pad ⇄ Shifter HV LV ⇄ D1 mini pin
────────────────────────────────────────────────────
SCL (A) ──► HV1 D1 (GPIO5 / SCL)
SDA (B) ──► HV2 D2 (GPIO4 / SDA)
RST (pad 6) ◄── HV3 D0 (GPIO16 - open-drain)
5 V / GND ─────────────── common rails

* slide-switch on Zero-II ➜ **EXT** (power from 5 V rail)  
* UART pins are **not** wired – the sketch uses I²C only.

---

## 2 Software

* **Arduino IDE ≥ 2.2** • **ESP8266 core ≥ 3.1.2**  
* Libraries via Library Manager  
  * *PubSubClient*   (Nick O’Leary)  
  * *RigExpertZeroII_I2C* (fork by *gerner*)  
  * *ArduinoJson* ≥ 6.21

### Build steps

1. Open **`zeroii_mqtt.ino`**  
2. Board ➜ **LOLIN (Wemos) D1 mini**  
3. Compile & Flash @115 200 Bd

### First-time setup

1. On boot ESP tries STA for 30 s. If it fails → AP **ESP8266_Setup** (no pwd).  
2. Browse to `192.168.4.1` → fill SSID, pass, MQTT host/port, station name → **Save & Reboot**.  
3. Settings persist in EEPROM. OTA firmware upload is always at **`/update`**.

---

## 3 MQTT API

| Topic | Direction | Payload |
|-------|-----------|---------|
| `matrigs/zeroii/cmd` | → bridge | frequency in **Hz** (ASCII) range 100 000 … 1 000 000 000 |
| `matrigs/zeroii/data` | ← bridge | JSON reply (`f,R,X,SWR,RL,Zmag,phase`) |
| `matrigs/zeroii/debug` | ← bridge | verbose log (auto-off 60 s after boot) |

Example data payload:

```json
{
  "f": 14200000,
  "R": 52.73,
  "X": 13.84,
  "SWR": 1.304,
  "RL": 15.94,
  "Zmag": 54.52,
  "phase": 14.71
}

