/*
 * BrainBoard_Remote_v0.4.ino
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
 *   1. Flash BrainBoard_Host_(v0.0).ino to Board 1 first.
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
// MAC address of Board 1 (the host)
// Replace with the actual MAC printed by Board 1 at startup
// ─────────────────────────────────────────────
uint8_t HOST_MAC_ADDRESS[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

// How often to send sensor data (milliseconds)
#define SEND_INTERVAL_MS 3000

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

// ─────────────────────────────────────────────
// Hardware
// ─────────────────────────────────────────────
Adafruit_SHTC3   shtc3;
Adafruit_TSL2591 tsl(2591);
SensorPayload    payload;

bool shtcReady = false;
bool tslReady  = false;

// ─────────────────────────────────────────────
// ESP-NOW send callback
// ESP-IDF v5.5+ changed the first arg to wifi_tx_info_t*
// ─────────────────────────────────────────────
void onDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    digitalWrite(23, LOW); delay(80); digitalWrite(23, HIGH); // quick blue blink = sent OK
    Serial.println("Sent OK");
  } else {
    digitalWrite(22, LOW); delay(80); digitalWrite(22, HIGH); // quick red blink = fail
    Serial.println("Send FAILED — is Board 1 in range and powered on?");
  }
}

// ─────────────────────────────────────────────
// Read sensors into payload struct
// ─────────────────────────────────────────────
void readSensors() {
  // SHTC3
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

  // TSL2591
  if (tslReady) {
    uint32_t lum    = tsl.getFullLuminosity();
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

  pinMode(22, OUTPUT); digitalWrite(22, HIGH); // Red LED off
  pinMode(23, OUTPUT); digitalWrite(23, HIGH); // Blue LED on = running

  Serial.println("\n=== Brain Board Remote Sensor Node (Board 2) ===");

  // ── I2C & Sensors ────────────────────────
  Wire.begin(SDA, SCL); // IO6, IO7

  Serial.print("SHTC3... ");
  if (shtc3.begin()) {
    shtcReady = true;
    Serial.println("OK");
  } else {
    Serial.println("FAILED");
  }

  Serial.print("TSL2591... ");
  if (tsl.begin()) {
    tsl.setGain(TSL2591_GAIN_MED);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
    tslReady = true;
    Serial.println("OK");
  } else {
    Serial.println("FAILED");
  }

  // ── WiFi in AP+STA mode (required for ESP-NOW) ───
  // LR is enabled on AP interface only — setting it on STA can cause
  // issues with peer discovery. Board 2 has no router to connect to,
  // but keeping STA in normal mode ensures ESP-NOW peer resolution works.
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);

  Serial.print("My MAC address (Board 2): ");
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
    while (true) { digitalWrite(22, LOW); delay(200); digitalWrite(22, HIGH); delay(200); }
  }

  esp_now_register_send_cb(onDataSent);

  // ── Register Board 1 as peer ─────────────
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, HOST_MAC_ADDRESS, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Failed to add peer. Check HOST_MAC_ADDRESS.");
  } else {
    Serial.println("Peer (Board 1) registered.");
  }

  Serial.printf("Sending sensor data every %d ms.\n", SEND_INTERVAL_MS);
}

// ─────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────
void loop() {
  static unsigned long lastSend = 0;
  unsigned long now = millis();

  if (now - lastSend >= SEND_INTERVAL_MS) {
    lastSend = now;
    readSensors();

    Serial.printf("Sending → Temp: %.1f°C  Hum: %.1f%%  Lux: %.1f\n",
                  payload.tempC, payload.humidity, payload.lux);

    esp_err_t result = esp_now_send(HOST_MAC_ADDRESS,
                                    (uint8_t*)&payload,
                                    sizeof(payload));
    if (result != ESP_OK) {
      Serial.printf("esp_now_send error: %d\n", result);
    }
  }
}
