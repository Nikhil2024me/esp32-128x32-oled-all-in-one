# ESP32 128×32 OLED — All-in-One Display

A single Arduino sketch for **ESP32 + SSD1306 128×32 OLED** that combines 10 features into one auto-rotating display dashboard.

![display demo](https://raw.githubusercontent.com/you/esp32-128x32-oled-all-in-one/main/docs/demo.gif)

---

## ✨ Features

| # | Feature | Notes |
|---|---------|-------|
| 1 | **Weather** | Temperature, humidity, wind — via Open-Meteo (free, no key) |
| 2 | **Seconds progress bar** | Thin bar fills 0→128 px over 60 s |
| 3 | **Auto-sliding carousel** | Pages rotate every 5 s automatically |
| 4 | **Web config portal** | AP-mode fallback: connect phone → set WiFi credentials |
| 5 | **MQTT message display** | Shows last received MQTT message |
| 6 | **Auto brightness (LDR)** | ADC → `u8g2.setContrast()` |
| 7 | **Quote / fact ticker** | Scrolling quote via quotable.io (free, no key) |
| 8 | **Scheduled alarms** | Flash display + buzzer at configured times |
| 9 | **WiFi RSSI meter** | Bar graph + dBm reading + IP address |
| 10 | **Deep sleep + battery** | Sleeps after idle timeout, ADC battery %, countdown |

---

## 📦 Libraries Required

Install all via **Arduino IDE → Library Manager**:

| Library | Author |
|---------|--------|
| U8g2 | olikraus |
| ArduinoJson (v6+) | bblanchon |
| PubSubClient | Nick O'Leary |
| NTPClient | Fabrice Weinberg |

Built-in ESP32 libraries used: `WiFi`, `HTTPClient`, `WebServer`, `Preferences`, `esp_sleep`.

---

## 🔌 Wiring

```
SSD1306 128x32          ESP32
──────────────────────────────
VCC  ──────────────►  3.3V
GND  ──────────────►  GND
SCL  ──────────────►  GPIO 22  (I²C Clock)
SDA  ──────────────►  GPIO 21  (I²C Data)

Optional hardware:
LDR + 10kΩ to GND  ──►  GPIO 34  (auto brightness)
Battery divider    ──►  GPIO 35  (battery meter)
Active buzzer      ──►  GPIO 25  (alarm beep)
```

**Battery divider** (for 3.7 V LiPo):
```
BATT+ ──[100kΩ]──┬── GPIO35
                 [100kΩ]
                 │
                GND
```

---

## ⚙️ Setup

### 1. Clone & open sketch
```bash
git clone https://github.com/YOUR_USERNAME/esp32-128x32-oled-all-in-one
```
Open `128x32dispaly/128x32dispaly.ino` in Arduino IDE.

### 2. Edit `config.h`
| Setting | What to change |
|---------|---------------|
| `WIFI_SSID` / `WIFI_PASSWORD` | Your home WiFi |
| `WEATHER_LAT` / `WEATHER_LON` | Your location (find at latlong.net) |
| `WEATHER_CITY` | Display label for your city |
| `MQTT_BROKER` | Your MQTT broker IP (or leave default) |
| `ALARM_TIMES[]` | Times to trigger alarms |
| `NTP_OFFSET_SEC` | Timezone offset in seconds |

**No API keys needed** — weather uses [Open-Meteo](https://open-meteo.com), quotes use [quotable.io](https://quotable.io).

### 3. Select board & flash
- Board: **ESP32 Dev Module**
- Upload speed: 921600
- Click **Upload**

### 4. First boot — WiFi config
If WiFi credentials are wrong or blank, the ESP32 starts in **AP mode**:
1. Connect your phone to WiFi: `ESP32-Display` (password: `12345678`)
2. Open browser → `192.168.4.1`
3. Enter your WiFi SSID + password → Save
4. Device restarts and connects automatically

---

## 📄 Page Descriptions

| Page | Content |
|------|---------|
| **0 — Clock** | Big time + date, RSSI bars (top-right), battery icon |
| **1 — Weather** | City, temp °C, humidity %, wind km/h, condition |
| **2 — Seconds bar** | Clock + thin progress bar showing seconds elapsed |
| **3 — Quote ticker** | Scrolling inspirational quote + author |
| **4 — MQTT** | Last MQTT message received on configured topic |
| **5 — RSSI** | dBm reading, IP address, signal strength bars |
| **6 — Battery** | Voltage, %, sleep countdown timer |

---

## 🔧 Hardware

- **ESP32** (any variant — DevKit, WROOM, etc.)
- **SSD1306 128×32 I²C OLED** (0.91″ module)
- *(Optional)* LDR for auto brightness
- *(Optional)* 100kΩ×2 voltage divider for battery meter
- *(Optional)* Active buzzer for alarms

---

## 📜 License

MIT — free to use, modify, and distribute.
