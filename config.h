// ============================================================
//  config.h  —  User Settings for ESP32 128x32 OLED Project
//  Fill in your own values here before flashing.
// ============================================================
#pragma once

// ---------- WiFi -------------------------------------------------
#define WIFI_SSID        "YourWiFiSSID"
#define WIFI_PASSWORD    "YourWiFiPassword"

// ---------- Fallback AP (Web Config Portal) ----------------------
#define AP_SSID          "ESP32-Display"
#define AP_PASSWORD      "12345678"

// ---------- NTP / Time -------------------------------------------
// Find your timezone offset list:
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define NTP_SERVER       "pool.ntp.org"
#define NTP_OFFSET_SEC   19800   // UTC+5:30 for India (change as needed)

// ---------- Weather (Open-Meteo — FREE, no API key, no signup) ----------
// Find your lat/lon at: https://www.latlong.net/
#define WEATHER_CITY     "Mumbai"          // Display label only (not sent to API)
#define WEATHER_LAT      "19.0760"         // Latitude  (e.g. 19.0760 for Mumbai)
#define WEATHER_LON      "72.8777"         // Longitude (e.g. 72.8777 for Mumbai)
// API docs: https://open-meteo.com/en/docs

// ---------- Quote API --------------------------------------------
// Uses free https://api.quotable.io/random — no key needed
#define QUOTE_API_URL    "http://api.quotable.io/random"

// ---------- MQTT -------------------------------------------------
#define MQTT_BROKER      "192.168.1.100"   // Your MQTT broker IP
#define MQTT_PORT        1883
#define MQTT_CLIENT_ID   "esp32-display"
#define MQTT_TOPIC       "home/display"    // Subscribe topic

// ---------- Scheduled Alarms (24-h HH:MM format) ----------------
// Add/remove entries as needed. Max 8.
const char* ALARM_TIMES[] = {
  "07:30",
  "13:00",
  "17:00",
  ""   // sentinel — do not remove this empty entry
};
#define ALARM_DURATION_MS 3000   // How long to flash/beep (ms)

// ---------- Hardware Pins ----------------------------------------
#define SDA_PIN          21
#define SCL_PIN          22
#define BUZZER_PIN       25    // Active buzzer (or LED)
#define LDR_PIN          34    // LDR voltage divider to GND  (ADC1)
#define BATT_PIN         35    // Battery voltage divider (ADC1)

// ---------- Brightness / LDR ------------------------------------
#define LDR_DARK_VAL     200   // ADC reading in total darkness
#define LDR_BRIGHT_VAL   3800  // ADC reading in bright light
#define CONTRAST_MIN     20    // u8g2 contrast in darkness (0-255)
#define CONTRAST_MAX     255   // u8g2 contrast in daylight  (0-255)

// ---------- Battery meter ----------------------------------------
// Resistor divider: BATT ---[R1=100k]---PIN---[R2=100k]--- GND
// Adjust divider ratio if using different resistors:
#define BATT_DIVIDER_RATIO  2.0f          // (R1+R2)/R2
#define BATT_FULL_VOLTS     4.2f
#define BATT_EMPTY_VOLTS    3.0f
#define BATT_ADC_REF        3.3f          // ESP32 ADC reference
#define BATT_ADC_MAX        4095.0f

// ---------- Display page carousel --------------------------------
#define PAGE_HOLD_MS     5000  // ms to show each page
#define SCROLL_SPEED_MS  80    // ms between scroll steps (quote ticker)

// ---------- Deep Sleep ------------------------------------------
#define IDLE_SLEEP_SEC   60    // Seconds of inactivity before sleep
#define SLEEP_DURATION_SEC  30 // How long to deep sleep (set 0 to disable)
