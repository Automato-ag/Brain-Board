/*
 * BrainBoard_Remote_v0.5.ino
 *
 * Automato Brain Board V2.0 — Remote Sensor Node (Board 2)
 *
 * Changelog:
 *   v0.1 — Initial single-board sensor web dashboard (Host only)
 *   v0.2 — Added two-board ESP-NOW LR support; this Remote sketch created
 *   v0.3 — Fixed ESP-NOW send callback signature for ESP-IDF v5.5+
 *           (wifi_tx_info_t* replaces uint8_t* mac in send cb)
 *   v0.4 — Matched Host v0.4 LR fix: enable LR on AP interface only,
 *           keep STA in normal mode for router compatibility
 *   v0.5 — Channel scan at startup (Option A):
 *           Board 2 scans channels 1–13, sends a ping on each,
 *           locks to whichever channel Board 1 ACKs on.
 *           Re-scans after RESCAN_TIMEOUT_MS of failed sends.
 *           Fixed LR protocol: moved to STA interface (same fix as Host v0.8).
 *
 * This board is powered only (no WiFi router needed).
 * It reads its onboard SHTC3 + TSL2591 sensors and
 * broadcasts the data via ESP-NOW in LR mode to Board 1 (the host).
 *
 * Required libraries:
 *   - Adafruit SHTC3 Library   (search "Adafruit SHTC3")
 *   - Adafruit TSL2591         (search "Adafruit TSL2591")
 *   - Adafruit BusIO           (install when prompted)
 *   - Adafruit Unified Sensor  (install when prompted)
 *
 * BEFORE FLASHING:
 *   1. Flash BrainBoard_Host_v08 to Board 1 first.
 *   2. Open Serial Monitor on Board 1 and note its MAC address
 *      printed at startup (format: AA:BB:CC:DD:EE:FF).
 *   3. Paste that MAC address into HOST_MAC_ADDRESS below.
 *   4. Flash this sketch to Board 2.
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include "Adafruit_SHTC3.h"
#include <Adafruit_TSL2591.h>

// ─────────────────────────────────────────────
// Hardware pin definitions
// ─────────────────────────────────────────────
#define PIN_SDA   6    // I2C data  — Brain Board V2.0
#define PIN_SCL   7    // I2C clock — Brain Board V2.0
#define PIN_LED_B 23   // Blue LED
#define PIN_LED_R 22   // Red LED

// ─────────────────────────────────────────────
// MAC address of Board 1 (the host)
// Replace with the actual MAC printed by Board 1 at startup
// ─────────────────────────────────────────────
uint8_t HOST_MAC_ADDRESS[] = {0xE4, 0xB3, 0x23, 0x89, 0x7E, 0x20};

// How often to send sensor data (milliseconds)
#define SEND_INTERVAL_MS   3000

// How long to wait for an ACK on each channel during scan (milliseconds)
#define SCAN_ACK_WAIT_MS   300

// After this many ms of consecutive send failures, trigger a re-scan
#define RESCAN_TIMEOUT_MS  30000

// ─────────────────────────────────────────────
// Shared data structure — must be identical on both boards
// ─────────────────────────────────────────────
typedef struct {
  float    tempC;
  float    tempF;
  float    humidity;
  float    lux;
  uint16_t visible;
  uint16_t infrared;
  bool     shtcOk;
  bool     tslOk;
  uint32_t uptime;        // seconds
} SensorPayload;

// Ping packet type — sent during channel scan, Host ignores it gracefully
// (it's smaller than SensorPayload so Host's onDataReceived length check
//  will reject it, but the ESP-NOW ACK still comes back at the radio level)
typedef struct {
  uint8_t type;   // 0xFF = ping
} PingPayload;

// ─────────────────────────────────────────────
// Hardware
// ─────────────────────────────────────────────
Adafruit_SHTC3   shtc3;
Adafruit_TSL2591 tsl(2591);
SensorPayload    payload;

bool shtcReady = false;
bool tslReady  = false;

// ─────────────────────────────────────────────
// Channel scan state
// ─────────────────────────────────────────────
volatile bool    lastSendOk      = false;  // set by send callback
int              lockedChannel   = -1;     // -1 = not yet found
unsigned long    lastSuccessMs   = 0;      // millis() of last successful send

// ─────────────────────────────────────────────
// ESP-NOW send callback
// ─────────────────────────────────────────────
void onDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
  lastSendOk = (status == ESP_NOW_SEND_SUCCESS);
  if (lastSendOk) {
    digitalWrite(PIN_LED_B, LOW); delay(80); digitalWrite(PIN_LED_B, HIGH);
  } else {
    digitalWrite(PIN_LED_R, LOW); delay(80); digitalWrite(PIN_LED_R, HIGH);
  }
}

// ─────────────────────────────────────────────
// Set peer to a specific channel and re-register
// ─────────────────────────────────────────────
void setPeerChannel(uint8_t ch) {
  esp_now_del_peer(HOST_MAC_ADDRESS);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, HOST_MAC_ADDRESS, 6);
  peer.channel = ch;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

// ─────────────────────────────────────────────
// Scan channels 1–13, return the first one Board 1 ACKs on
// Returns -1 if no channel responds
// ─────────────────────────────────────────────
int scanForHost() {
  Serial.println("Scanning channels 1–13 for Board 1...");
  PingPayload ping = {0xFF};

  for (int ch = 1; ch <= 13; ch++) {
    Serial.printf("  Channel %d... ", ch);

    // Switch WiFi channel
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    setPeerChannel(ch);

    lastSendOk = false;
    esp_now_send(HOST_MAC_ADDRESS, (uint8_t*)&ping, sizeof(ping));

    // Wait up to SCAN_ACK_WAIT_MS for the callback to fire
    unsigned long t = millis();
    while (millis() - t < SCAN_ACK_WAIT_MS) {
      if (lastSendOk) break;
      delay(10);
    }

    if (lastSendOk) {
      Serial.printf("ACK! Locking to channel %d.\n", ch);
      return ch;
    } else {
      Serial.println("no response.");
    }
  }
  return -1;
}

// ─────────────────────────────────────────────
// Read sensors into payload struct
// ─────────────────────────────────────────────
void readSensors() {
  sensors_event_t hum_evt, temp_evt;
  shtc3.getEvent(&hum_evt, &temp_evt);
  if (!isnan(temp_evt.temperature)) {
    payload.tempC    = temp_evt.temperature;
    payload.tempF    = payload.tempC * 9.0 / 5.0 + 32.0;
    payload.humidity = hum_evt.relative_humidity;
    payload.shtcOk   = true;
  } else {
    payload.shtcOk = false;
  }

  if (tslReady) {
    uint32_t lum     = tsl.getFullLuminosity();
    payload.infrared = lum >> 16;
    payload.visible  = lum & 0xFFFF;
    float lux        = tsl.calculateLux(payload.visible, payload.infrared);
    payload.lux      = (lux < 0) ? 0 : lux;
    payload.tslOk    = true;
  } else {
    payload.tslOk = false;
  }

  payload.uptime = millis() / 1000;
}

// ─────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_LED_R, OUTPUT); digitalWrite(PIN_LED_R, HIGH);
  pinMode(PIN_LED_B, OUTPUT); digitalWrite(PIN_LED_B, HIGH);

  Serial.println("\n=== Automato Brain Board Remote v0.5.0 ===");

  // ── I2C & Sensors ────────────────────────
  Wire.begin(PIN_SDA, PIN_SCL);

  Serial.print("SHTC3... ");
  if (shtc3.begin()) { shtcReady = true; Serial.println("OK"); }
  else                {                  Serial.println("FAILED"); }

  Serial.print("TSL2591... ");
  if (tsl.begin()) {
    tsl.setGain(TSL2591_GAIN_MED);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
    tslReady = true;
    Serial.println("OK");
  } else {
    Serial.println("FAILED");
  }

  // ── WiFi ─────────────────────────────────
  // LR on STA only — AP stays standard 802.11 (same fix as Host v0.8)
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_protocol(WIFI_IF_STA,
    WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);

  Serial.print("Board 2 MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Targeting Board 1 MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", HOST_MAC_ADDRESS[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  // ── Init ESP-NOW ─────────────────────────
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAILED. Halting.");
    while (true) {
      digitalWrite(PIN_LED_R, LOW); delay(200);
      digitalWrite(PIN_LED_R, HIGH); delay(200);
    }
  }
  esp_now_register_send_cb(onDataSent);

  // Register peer on ch 1 initially — scanForHost() will update it
  setPeerChannel(1);

  // ── Channel scan ─────────────────────────
  while (lockedChannel == -1) {
    lockedChannel = scanForHost();
    if (lockedChannel == -1) {
      Serial.println("No response on any channel. Retrying in 5s...");
      // Slow red blink while waiting
      for (int i = 0; i < 5; i++) {
        digitalWrite(PIN_LED_R, LOW); delay(500);
        digitalWrite(PIN_LED_R, HIGH); delay(500);
      }
    }
  }

  lastSuccessMs = millis();
  Serial.printf("Ready. Sending every %d ms on channel %d.\n",
                SEND_INTERVAL_MS, lockedChannel);
}

// ─────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────
void loop() {
  static unsigned long lastSend = 0;
  unsigned long now = millis();

  // Re-scan if we haven't had a successful send in RESCAN_TIMEOUT_MS
  if (lockedChannel != -1 && (now - lastSuccessMs) > RESCAN_TIMEOUT_MS) {
    Serial.println("No successful sends for 30s — re-scanning channels.");
    lockedChannel = -1;
    while (lockedChannel == -1) {
      lockedChannel = scanForHost();
      if (lockedChannel == -1) {
        Serial.println("No response. Retrying in 5s...");
        for (int i = 0; i < 5; i++) {
          digitalWrite(PIN_LED_R, LOW); delay(500);
          digitalWrite(PIN_LED_R, HIGH); delay(500);
        }
      }
    }
    lastSuccessMs = millis();
    Serial.printf("Re-locked to channel %d.\n", lockedChannel);
  }

  if (now - lastSend >= SEND_INTERVAL_MS) {
    lastSend = now;
    readSensors();

    Serial.printf("Sending → Temp: %.1f°C  Hum: %.1f%%  Lux: %.1f  ch:%d\n",
                  payload.tempC, payload.humidity, payload.lux, lockedChannel);

    lastSendOk = false;
    esp_err_t result = esp_now_send(HOST_MAC_ADDRESS,
                                    (uint8_t*)&payload,
                                    sizeof(payload));
    if (result != ESP_OK) {
      Serial.printf("esp_now_send error: %d\n", result);
    } else {
      // Wait briefly for callback so we can update lastSuccessMs
      unsigned long t = millis();
      while (millis() - t < 300 && !lastSendOk) delay(10);
      if (lastSendOk) lastSuccessMs = millis();
    }
  }
}
