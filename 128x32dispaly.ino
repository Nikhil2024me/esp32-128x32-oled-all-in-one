// ================================================================
//  128x32dispaly.ino
//  ESP32 + SSD1306 128×32 OLED — All-in-One Firmware
//
//  Features combined:
//    1.  Weather from OpenWeatherMap
//    2.  Seconds progress bar
//    3.  Auto-sliding page carousel (clock → weather → IP…)
//    4.  Web config portal (AP-mode fallback)
//    5.  MQTT message display
//    6.  Auto brightness via LDR
//    7.  Quote / fact ticker
//    8.  Scheduled alerts / alarms
//    9.  WiFi RSSI signal meter
//   10.  Deep sleep + battery meter
//
//  Required libraries (install via Library Manager):
//    - U8g2            (by olikraus)
//    - ArduinoJson     (by bblanchon)  v6+
//    - PubSubClient    (by Nick O'Leary)
//    - NTPClient       (by Fabrice Weinberg)
//    - Preferences     (built-in ESP32)
//    - WebServer       (built-in ESP32)
//    - WiFi            (built-in ESP32)
//    - HTTPClient      (built-in ESP32)
//
//  Board: ESP32 Dev Module (or any ESP32)
// ================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUDP.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <esp_sleep.h>
#include "config.h"

// ================================================================
//  Display
// ================================================================
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, SCL_PIN, SDA_PIN);

// ================================================================
//  NTP
// ================================================================
WiFiUDP   ntpUDP;
NTPClient ntpClient(ntpUDP, NTP_SERVER, NTP_OFFSET_SEC, 60000);

// ================================================================
//  MQTT
// ================================================================
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);
char         mqttMessage[64] = "Waiting for MQTT…";

// ================================================================
//  Web Config Portal
// ================================================================
WebServer    webServer(80);
Preferences  prefs;

// ================================================================
//  State
// ================================================================
struct WeatherData {
  float  tempC     = 0;
  float  humidity  = 0;
  float  windKph   = 0;
  char   desc[32]  = "---";
  bool   valid     = false;
};

struct QuoteData {
  char text[120]   = "Loading quote…";
  char author[48]  = "";
  bool valid       = false;
};

WeatherData weather;
QuoteData   quote;

uint8_t     currentPage       = 0;
uint8_t     numPages          = 7;      // pages 0-6
uint32_t    lastPageSwitch    = 0;
uint32_t    lastWeatherFetch  = 0;
uint32_t    lastQuoteFetch    = 0;
uint32_t    lastMQTTReconnect = 0;
uint32_t    lastActivity      = 0;

int         scrollOffset      = 0;      // for quote ticker
uint32_t    lastScrollStep    = 0;

bool        wifiConnected     = false;
bool        apModeActive      = false;

char        savedSSID[64]     = WIFI_SSID;
char        savedPass[64]     = WIFI_PASSWORD;

// ================================================================
//  Forward declarations
// ================================================================
void connectWiFi();
void startAPMode();
void setupWebPortal();
void fetchWeather();
void fetchQuote();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void drawPage(uint8_t page);
void drawClock();
void drawWeather();
void drawSecondsBar();
void drawQuoteTicker();
void drawMQTT();
void drawRSSI();
void drawAlarm();
void drawBatteryIcon(uint8_t x, uint8_t y);
void checkAlarms();
void updateBrightness();
void handleDeepSleep();
String getTimeString();
String getDateString();
float  readBattVoltage();
void   webHandleRoot();
void   webHandleSave();

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(115200);

  // Pins
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(LDR_PIN,  INPUT);
  pinMode(BATT_PIN, INPUT);

  // Display
  u8g2.begin();
  u8g2.setContrast(128);

  // Splash
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 14, "ESP32 128x32 OLED");
  u8g2.drawStr(22, 28, "All-in-One v1.0");
  u8g2.sendBuffer();
  delay(1500);

  // Load saved credentials
  prefs.begin("wifi", true);
  prefs.getString("ssid", savedSSID, sizeof(savedSSID));
  prefs.getString("pass", savedPass, sizeof(savedPass));
  prefs.end();

  // WiFi
  connectWiFi();

  if (wifiConnected) {
    ntpClient.begin();
    ntpClient.update();
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    fetchWeather();
    fetchQuote();
  } else {
    startAPMode();
    setupWebPortal();
  }

  lastActivity = millis();
}

// ================================================================
//  LOOP
// ================================================================
void loop() {
  uint32_t now = millis();

  // Web portal handling (AP mode)
  if (apModeActive) {
    webServer.handleClient();
    // Show AP info on display
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 7,  "WiFi Setup Mode");
    u8g2.drawStr(0, 17, "AP: " AP_SSID);
    u8g2.drawStr(0, 27, "Open 192.168.4.1");
    u8g2.sendBuffer();
    return;
  }

  // NTP
  ntpClient.update();

  // MQTT
  if (!mqttClient.connected() && (now - lastMQTTReconnect > 10000)) {
    lastMQTTReconnect = now;
    reconnectMQTT();
  }
  mqttClient.loop();

  // Periodic data refresh
  if (now - lastWeatherFetch > 600000UL) { // 10 min
    lastWeatherFetch = now;
    fetchWeather();
  }
  if (now - lastQuoteFetch > 3600000UL) { // 1 hour
    lastQuoteFetch = now;
    fetchQuote();
    scrollOffset = 0;
  }

  // Auto brightness
  updateBrightness();

  // Check alarms
  checkAlarms();

  // Page rotation
  if (now - lastPageSwitch >= PAGE_HOLD_MS) {
    lastPageSwitch = now;
    currentPage = (currentPage + 1) % numPages;
    if (currentPage == 3) scrollOffset = 0; // reset scroll on quote page
    lastActivity = now;
  }

  // Quote scroll step
  if (currentPage == 3 && (now - lastScrollStep >= SCROLL_SPEED_MS)) {
    lastScrollStep = now;
    scrollOffset++;
  }

  // Draw current page
  drawPage(currentPage);

  // Deep sleep check
  handleDeepSleep();

  delay(20);
}

// ================================================================
//  WiFi Connection
// ================================================================
void connectWiFi() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(0, 10, "Connecting WiFi…");
  u8g2.sendBuffer();

  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSSID, savedPass);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(300);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 10, "Connecting WiFi…");
    char dots[12] = "";
    int d = ((millis() - start) / 300) % 5;
    for (int i = 0; i < d; i++) strcat(dots, ".");
    u8g2.drawStr(0, 22, dots);
    u8g2.sendBuffer();
  }

  wifiConnected = (WiFi.status() == WL_CONNECTED);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);
  if (wifiConnected) {
    u8g2.drawStr(0, 10, "WiFi Connected!");
    u8g2.drawStr(0, 22, WiFi.localIP().toString().c_str());
  } else {
    u8g2.drawStr(0, 10, "WiFi Failed");
    u8g2.drawStr(0, 22, "Starting AP…");
  }
  u8g2.sendBuffer();
  delay(1200);
}

// ================================================================
//  AP Mode
// ================================================================
void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  apModeActive = true;
}

void setupWebPortal() {
  webServer.on("/", webHandleRoot);
  webServer.on("/save", HTTP_POST, webHandleSave);
  webServer.begin();
}

void webHandleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>ESP32 Display Setup</title>
<style>body{font-family:Arial;padding:20px;background:#1a1a2e;color:#eee;}
input{width:100%;padding:8px;margin:6px 0;border-radius:4px;border:none;}
button{background:#e94560;color:#fff;padding:10px 24px;border:none;border-radius:4px;cursor:pointer;}
h2{color:#e94560;}</style></head>
<body><h2>ESP32 Display Config</h2>
<form action='/save' method='POST'>
  <label>WiFi SSID</label><input name='ssid' placeholder='YourWiFi'><br>
  <label>WiFi Password</label><input name='pass' type='password'><br>
  <label>City (for weather)</label><input name='city' placeholder='Mumbai'><br>
  <label>Timezone offset (seconds)</label><input name='tz' placeholder='19800'><br>
  <button type='submit'>Save &amp; Restart</button>
</form></body></html>
)rawhtml";
  webServer.send(200, "text/html", html);
}

void webHandleSave() {
  String ssid = webServer.arg("ssid");
  String pass = webServer.arg("pass");

  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();

  webServer.send(200, "text/html",
    "<html><body style='font-family:Arial;background:#1a1a2e;color:#eee;padding:20px'>"
    "<h2 style='color:#e94560'>Saved! Restarting…</h2></body></html>");
  delay(1500);
  ESP.restart();
}

// ================================================================
//  Weather Fetch — Open-Meteo (FREE, no API key)
//  Docs: https://open-meteo.com/en/docs
// ================================================================
void fetchWeather() {
  if (!wifiConnected) return;
  HTTPClient http;
  char url[300];
  // current_weather gives temp + windspeed
  // hourly=relativehumidity_2m gives humidity for first hour
  snprintf(url, sizeof(url),
    "http://api.open-meteo.com/v1/forecast"
    "?latitude=%s&longitude=%s"
    "&current_weather=true"
    "&hourly=relativehumidity_2m,weathercode"
    "&forecast_days=1",
    WEATHER_LAT, WEATHER_LON);
  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();
  if (code == 200) {
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    if (!err) {
      weather.tempC    = doc["current_weather"]["temperature"];
      weather.windKph  = (float)doc["current_weather"]["windspeed"];  // already km/h
      // Humidity: first hourly value
      weather.humidity = doc["hourly"]["relativehumidity_2m"][0] | 0;
      // Map WMO weather code to short description
      int wcode = doc["current_weather"]["weathercode"] | 0;
      if      (wcode == 0)               strlcpy(weather.desc, "Clear",      sizeof(weather.desc));
      else if (wcode <= 3)               strlcpy(weather.desc, "Partly cld", sizeof(weather.desc));
      else if (wcode <= 48)              strlcpy(weather.desc, "Fog",        sizeof(weather.desc));
      else if (wcode <= 67)              strlcpy(weather.desc, "Rain",       sizeof(weather.desc));
      else if (wcode <= 77)              strlcpy(weather.desc, "Snow",       sizeof(weather.desc));
      else if (wcode <= 82)              strlcpy(weather.desc, "Showers",    sizeof(weather.desc));
      else if (wcode <= 99)              strlcpy(weather.desc, "Thunderstm", sizeof(weather.desc));
      else                               strlcpy(weather.desc, "---",        sizeof(weather.desc));
      weather.valid = true;
    }
  }
  http.end();
}

// ================================================================
//  Quote Fetch
// ================================================================
void fetchQuote() {
  if (!wifiConnected) return;
  HTTPClient http;
  http.begin(QUOTE_API_URL);
  http.setTimeout(5000);
  int code = http.GET();
  if (code == 200) {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, http.getStream());
    strlcpy(quote.text,   doc["content"] | "Be curious.", sizeof(quote.text));
    strlcpy(quote.author, doc["author"]  | "Unknown",    sizeof(quote.author));
    quote.valid = true;
  }
  http.end();
}

// ================================================================
//  MQTT
// ================================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  uint8_t len = min((unsigned int)63, length);
  memcpy(mqttMessage, payload, len);
  mqttMessage[len] = '\0';
  lastActivity = millis();
}

void reconnectMQTT() {
  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    mqttClient.subscribe(MQTT_TOPIC);
  }
}

// ================================================================
//  Auto Brightness
// ================================================================
void updateBrightness() {
  int raw = analogRead(LDR_PIN);
  raw = constrain(raw, LDR_DARK_VAL, LDR_BRIGHT_VAL);
  uint8_t contrast = map(raw, LDR_DARK_VAL, LDR_BRIGHT_VAL, CONTRAST_MIN, CONTRAST_MAX);
  u8g2.setContrast(contrast);
}

// ================================================================
//  Battery
// ================================================================
float readBattVoltage() {
  int raw = analogRead(BATT_PIN);
  float v = (raw / BATT_ADC_MAX) * BATT_ADC_REF * BATT_DIVIDER_RATIO;
  return v;
}

void drawBatteryIcon(uint8_t x, uint8_t y) {
  float v    = readBattVoltage();
  float pct  = (v - BATT_EMPTY_VOLTS) / (BATT_FULL_VOLTS - BATT_EMPTY_VOLTS);
  pct        = constrain(pct, 0.0f, 1.0f);
  uint8_t w  = (uint8_t)(pct * 10);
  u8g2.drawFrame(x, y, 12, 6);
  u8g2.drawBox(x + 12, y + 2, 2, 2);
  if (w > 0) u8g2.drawBox(x + 1, y + 1, w, 4);
}

// ================================================================
//  Time helpers
// ================================================================
String getTimeString() {
  uint32_t ep = ntpClient.getEpochTime();
  struct tm* t = localtime((time_t*)&ep);
  char buf[10];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
  return String(buf);
}

String getDateString() {
  uint32_t ep = ntpClient.getEpochTime();
  struct tm* t = localtime((time_t*)&ep);
  char buf[20];
  snprintf(buf, sizeof(buf), "%02d %s %04d",
    t->tm_mday,
    (const char*[]){"Jan","Feb","Mar","Apr","May","Jun",
                    "Jul","Aug","Sep","Oct","Nov","Dec"}[t->tm_mon],
    t->tm_year + 1900);
  return String(buf);
}

// ================================================================
//  Alarm Check
// ================================================================
void checkAlarms() {
  String now = getTimeString().substring(0, 5); // "HH:MM"
  for (int i = 0; ALARM_TIMES[i][0] != '\0'; i++) {
    if (now == String(ALARM_TIMES[i])) {
      // Flash display + beep buzzer
      for (int f = 0; f < 3; f++) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_10x20_tf);
        u8g2.drawStr(14, 24, "ALARM!");
        u8g2.sendBuffer();
        digitalWrite(BUZZER_PIN, HIGH);
        delay(300);
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
      }
      delay(ALARM_DURATION_MS);
      lastActivity = millis();
    }
  }
}

// ================================================================
//  Deep Sleep
// ================================================================
void handleDeepSleep() {
#if SLEEP_DURATION_SEC > 0
  if (millis() - lastActivity > (uint32_t)IDLE_SLEEP_SEC * 1000) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(14, 16, "Sleeping… zzz");
    u8g2.sendBuffer();
    delay(800);
    u8g2.setPowerSave(1);
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_DURATION_SEC * 1000000ULL);
    esp_deep_sleep_start();
  }
#endif
}

// ================================================================
//  RSSI bar helper
// ================================================================
void drawRSSIBars(uint8_t x, uint8_t y) {
  // 4 bars, each 3px wide, 1px gap, heights 4,7,10,13
  int rssi = WiFi.RSSI();  // e.g. -50 to -100
  int bars = 0;
  if (rssi > -55) bars = 4;
  else if (rssi > -65) bars = 3;
  else if (rssi > -75) bars = 2;
  else if (rssi > -85) bars = 1;

  uint8_t heights[] = {4, 7, 10, 13};
  for (int i = 0; i < 4; i++) {
    uint8_t bx = x + i * 4;
    uint8_t bh = heights[i];
    uint8_t by = y + 13 - bh;
    if (i < bars)
      u8g2.drawBox(bx, by, 3, bh);
    else
      u8g2.drawFrame(bx, by, 3, bh);
  }
}

// ================================================================
//  PAGE RENDERING
// ================================================================
void drawPage(uint8_t page) {
  u8g2.clearBuffer();

  switch (page) {

    // ----------------------------------------------------------
    // Page 0: Clock + Date + battery + RSSI
    // ----------------------------------------------------------
    case 0:
      u8g2.setFont(u8g2_font_10x20_tf);
      u8g2.drawStr(0, 22, getTimeString().c_str());
      u8g2.setFont(u8g2_font_5x7_tf);
      u8g2.drawStr(0, 31, getDateString().c_str());
      // RSSI top-right
      drawRSSIBars(110, 0);
      // Battery bottom-right
      drawBatteryIcon(115, 25);
      break;

    // ----------------------------------------------------------
    // Page 1: Weather
    // ----------------------------------------------------------
    case 1:
      u8g2.setFont(u8g2_font_6x10_tf);
      if (weather.valid) {
        char line1[32], line2[32];
        snprintf(line1, sizeof(line1), "%.1fC  Hum:%.0f%%", weather.tempC, weather.humidity);
        snprintf(line2, sizeof(line2), "Wind:%.1fkph  %s", weather.windKph, weather.desc);
        u8g2.drawStr(0, 10, WEATHER_CITY);
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(0, 21, line1);
        u8g2.drawStr(0, 31, line2);
      } else {
        u8g2.drawStr(0, 16, "Fetching weather…");
      }
      break;

    // ----------------------------------------------------------
    // Page 2: Seconds progress bar
    // ----------------------------------------------------------
    case 2: {
      u8g2.setFont(u8g2_font_10x20_tf);
      u8g2.drawStr(22, 22, getTimeString().c_str());
      uint32_t ep = ntpClient.getEpochTime();
      struct tm* t = localtime((time_t*)&ep);
      uint8_t barW = map(t->tm_sec, 0, 59, 0, 127);
      u8g2.drawBox(0, 29, barW, 3);
      u8g2.drawFrame(0, 29, 128, 3);
      break;
    }

    // ----------------------------------------------------------
    // Page 3: Quote ticker (scrolling)
    // ----------------------------------------------------------
    case 3: {
      u8g2.setFont(u8g2_font_5x7_tf);
      u8g2.drawStr(0, 8, "Quote of the day:");
      // Scrolling text on bottom row
      int textLen = strlen(quote.text) * 6; // approx px per char @ 5x7
      int sx = 128 - scrollOffset;
      if (sx < -textLen) scrollOffset = 0; // wrap
      u8g2.drawStr(sx, 22, quote.text);
      // Author on last row (static)
      char authLine[50];
      snprintf(authLine, sizeof(authLine), "— %s", quote.author);
      u8g2.drawStr(0, 31, authLine);
      break;
    }

    // ----------------------------------------------------------
    // Page 4: MQTT message display
    // ----------------------------------------------------------
    case 4:
      u8g2.setFont(u8g2_font_5x7_tf);
      u8g2.drawStr(0, 8,  "MQTT:");
      u8g2.drawStr(0, 8 + (MQTT_BROKER[0] ? 0 : 0), MQTT_BROKER);
      u8g2.setFont(u8g2_font_6x10_tf);
      // Word-wrap: show up to 2 lines of mqttMessage
      {
        char line1[22], line2[22];
        strncpy(line1, mqttMessage, 21); line1[21] = '\0';
        if (strlen(mqttMessage) > 21) {
          strncpy(line2, mqttMessage + 21, 21); line2[21] = '\0';
        } else {
          line2[0] = '\0';
        }
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(0, 8,  "MQTT msg:");
        u8g2.drawStr(0, 20, line1);
        u8g2.drawStr(0, 31, line2);
      }
      break;

    // ----------------------------------------------------------
    // Page 5: WiFi RSSI signal meter
    // ----------------------------------------------------------
    case 5: {
      u8g2.setFont(u8g2_font_5x7_tf);
      int rssi = WiFi.RSSI();
      char rline[32];
      snprintf(rline, sizeof(rline), "RSSI: %d dBm", rssi);
      u8g2.drawStr(0, 8,  rline);
      u8g2.drawStr(0, 18, WiFi.localIP().toString().c_str());
      drawRSSIBars(88, 16);
      // Warn if weak
      if (rssi < -75) {
        u8g2.drawStr(0, 31, "Weak signal!");
      } else {
        u8g2.drawStr(0, 31, "Signal OK");
      }
      break;
    }

    // ----------------------------------------------------------
    // Page 6: Deep sleep + battery info
    // ----------------------------------------------------------
    case 6: {
      float v   = readBattVoltage();
      float pct = constrain((v - BATT_EMPTY_VOLTS) / (BATT_FULL_VOLTS - BATT_EMPTY_VOLTS), 0, 1) * 100;
      u8g2.setFont(u8g2_font_5x7_tf);
      char bline[32];
      snprintf(bline, sizeof(bline), "Batt: %.2fV  %.0f%%", v, pct);
      u8g2.drawStr(0, 8,  bline);
      drawBatteryIcon(0, 14);
      char sline[32];
      uint32_t idleSec = (millis() - lastActivity) / 1000;
      snprintf(sline, sizeof(sline), "Sleep in: %lus", (unsigned long)(IDLE_SLEEP_SEC - idleSec));
      u8g2.drawStr(0, 31, sline);
      break;
    }
  }

  u8g2.sendBuffer();
}
