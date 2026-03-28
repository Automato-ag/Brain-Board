/*
 * BrainBoard_v110.ino
 *
 * Automato Brain Board — Unified Firmware v1.1.0
 *
 * All boards run identical firmware. Roles (gateway / peer) are
 * determined dynamically at runtime — not assigned by the user.
 *
 * Changelog:
 *   v1.1.0 — Unified firmware. All boards are full peers.
 *             MSG_HELLO auto-discovery beacon (burst + settle cadence).
 *             Peer table: boards with matching Network Name auto-register.
 *             Dynamic gateway election: WiFi board with best RSSI wins.
 *             networkname.local mDNS advertised by gateway.
 *             1-hop relay: non-WiFi boards push sensor data to gateway.
 *             GET /peers endpoint: full peer list as JSON.
 *             POST /time endpoint: manual time sync from browser device.
 *             Plugin hooks: customSetup(), customLoop(), customDataJSON().
 *             NTP time sync on WiFi connect; hourly re-sync.
 *
 * Required libraries (Arduino Library Manager):
 *   - Adafruit SHTC3 Library        (search "Adafruit SHTC3")
 *   - Adafruit TSL2591              (search "Adafruit TSL2591")
 *   - Adafruit BusIO                (install when prompted)
 *   - Adafruit Unified Sensor       (install when prompted)
 *   - SparkFun TCA9534 GPIO Expander (search "SparkFun TCA9534")
 *   Built-in: LittleFS, Update, FS, Preferences, ESPmDNS, time.h
 *
 * Arduino IDE settings:
 *   Board:            ESP32C6 Dev Module
 *   Partition Scheme: Custom  (partitions.csv in sketch folder)
 *   USB CDC On Boot:  Enabled  (CRITICAL — resets each session)
 *
 * FIRST FLASH:
 *   1. Flash this sketch (Sketch → Upload).
 *   2. Connect to Automato-XXXX WiFi, open http://192.168.4.1/setup.
 *   3. Enter WiFi credentials and Network Name, click Save & Connect.
 *   4. Upload webapp: http://<board>.local/update → Webapp tab.
 *
 * RESET WIFI CREDENTIALS:
 *   - Hold Boot button (IO9) for 5 seconds at startup, OR
 *   - Use Reset WiFi Credentials button in Settings tab.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <FS.h>
#include <LittleFS.h>
#include <Update.h>
#include <time.h>
#include "Adafruit_SHTC3.h"
#include <Adafruit_TSL2591.h>
#include <SparkFun_TCA9534.h>

// ─────────────────────────────────────────────
// Hardware pin definitions
// ─────────────────────────────────────────────
#define PIN_SDA    6
#define PIN_SCL    7
#define PIN_LED_B  23
#define PIN_LED_R  22
#define PIN_BOOT   9

// ─────────────────────────────────────────────
// Version
// ─────────────────────────────────────────────
#define FIRMWARE_VERSION  "1.1.0"
#define WEBAPP_VERSION    "1.1.0"

// ─────────────────────────────────────────────
// NVS
// ─────────────────────────────────────────────
Preferences prefs;

// ─────────────────────────────────────────────
// WiFi / provisioning state
// ─────────────────────────────────────────────
char  wifiSSID[64]     = "";
char  wifiPassword[64] = "";
char  boardName[32]    = "";
char  apSSID[32]       = "";
char  mdnsName[32]     = "";
char  networkName[33]  = "";
char  networkPass[33]  = "";
bool  wifiConnected    = false;
bool  hasCredentials   = false;
uint8_t myMac[6]       = {};

// SoftAP management
uint8_t       softApMode      = 0;     // 0=auto (off when WiFi up), 1=always on
bool          softApActive    = true;  // current SoftAP state
unsigned long softApTempUntil = 0;     // millis() when temp window expires; 0=inactive

// Channel seek (non-WiFi boards — find mesh channel non-blocking)
volatile bool channelLocked     = false; // set by recv callback when matching beacon heard
uint8_t       meshChannel       = 1;     // current locked channel
uint8_t       seekChannelIdx    = 0;     // index into seek channel sequence
unsigned long seekLastAdvanceMs = 0;     // last channel advance timestamp
unsigned long seekStartMs       = 0;     // when seek began (for periodic fast-beacon kick)

// ─────────────────────────────────────────────
// Filesystem state
// ─────────────────────────────────────────────
bool lfsOk = false;
char webappVersion[32] = WEBAPP_VERSION;

// ─────────────────────────────────────────────
// ESP-NOW packet types
// All packets begin with a uint8_t type discriminator.
// ─────────────────────────────────────────────
#define MSG_SCAN_REQUEST  0x01
#define MSG_SCAN_RESPONSE 0x02
#define MSG_HELLO         0x03   // discovery beacon
#define MSG_SENSOR        0x04   // sensor data relay (non-gateway → gateway)

// ─────────────────────────────────────────────
// HelloPayload — broadcast by every board periodically.
// Boards with matching networkName auto-register as ESP-NOW peers.
// ─────────────────────────────────────────────
typedef struct {
  uint8_t  type;              // MSG_HELLO
  char     networkName[33];   // must match to join
  char     boardName[33];     // individual board identity
  uint8_t  mac[6];            // sender MAC (redundant with recv_info, but explicit)
  bool     hasWiFi;           // does this board have active WiFi router connection?
  int8_t   rssi;              // WiFi RSSI (-127 if no WiFi) — used for gateway election
  uint32_t unixTime;          // current unix timestamp (0 if unknown)
  bool     timeValid;         // true if unixTime should be trusted
  uint8_t  timeSyncSource;    // 0=none, 1=peer-inherited, 2=manual/browser, 3=NTP/RTC
  uint32_t secondsSinceSync;  // seconds since last authoritative time sync
} HelloPayload;

// ─────────────────────────────────────────────
// SensorBroadcastPayload — sent by non-gateway boards to gateway
// ─────────────────────────────────────────────
typedef struct {
  uint8_t  type;       // MSG_SENSOR
  uint8_t  mac[6];     // sender MAC
  float    tempC;
  float    tempF;
  float    humidity;
  float    lux;
  uint16_t visible;
  uint16_t infrared;
  bool     shtcOk;
  bool     tslOk;
  uint32_t uptime;     // seconds
} SensorBroadcastPayload;

// ─────────────────────────────────────────────
// Remote I2C scan packets (retained from v0.9)
// ─────────────────────────────────────────────
typedef struct { uint8_t type; } ScanRequestPayload;

typedef struct {
  uint8_t type;
  uint8_t count;
  uint8_t addrs[32];
} ScanResponsePayload;

// ─────────────────────────────────────────────
// Peer table
// Tracks all boards that have responded to our Network Name.
// ─────────────────────────────────────────────
#define MAX_PEERS        20    // ESP-NOW hard limit
#define PEER_TIMEOUT_MS  90000 // 3 missed 30-sec beacons → offline

typedef struct {
  uint8_t  mac[6];
  char     boardName[33];
  bool     active;             // false = slot unused or timed out
  bool     hasWiFi;
  int8_t   rssi;
  unsigned long lastSeen;      // millis()

  // Time info received in their beacon
  uint32_t unixTime;
  bool     timeValid;
  uint8_t  timeSyncSource;
  uint32_t secondsSinceSync;

  // Sensor data (from MSG_SENSOR packets)
  float    tempC, tempF, humidity, lux;
  uint16_t visible, infrared;
  bool     shtcOk, tslOk;
  uint32_t sensorUptime;
  unsigned long sensorLastUpdated; // millis(), 0 if never received

  // I2C scan results
  uint8_t  scanAddrs[32];
  uint8_t  scanCount;
  bool     scanReady;
} PeerInfo;

PeerInfo peers[MAX_PEERS];
uint8_t  peerCount = 0;

// Broadcast MAC — used to send beacons to all boards in range
uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ─────────────────────────────────────────────
// Gateway state
// ─────────────────────────────────────────────
bool    isGateway     = false;
bool    wasGateway    = false;  // to detect transitions
uint8_t gatewayMac[6] = {};
bool    gatewayKnown  = false;

// ─────────────────────────────────────────────
// Beacon timing — burst + settle
// ─────────────────────────────────────────────
#define BEACON_FAST_MS      5000   // 5 sec during fast phase
#define BEACON_SLOW_MS      30000  // 30 sec in steady state
#define BEACON_FAST_DURATION_MS 60000  // fast phase duration on trigger

unsigned long beaconLastSent    = 0;
unsigned long beaconFastStartMs = 0;
bool          beaconFastMode    = true;  // start in fast mode

// ─────────────────────────────────────────────
// Sensor broadcast timing (non-gateway → gateway)
// ─────────────────────────────────────────────
#define SENSOR_BROADCAST_MS  30000  // every 30 sec

unsigned long sensorLastBroadcast = 0;

// ─────────────────────────────────────────────
// Time state
// ─────────────────────────────────────────────
bool          timeValid       = false;
uint8_t       timeSyncSource  = 0;    // 0=none, 1=peer, 2=manual/browser, 3=NTP
unsigned long lastSyncMillis  = 0;

#define NTP_RESYNC_MS  3600000  // re-sync NTP hourly
unsigned long lastNtpSync = 0;

// ─────────────────────────────────────────────
// Hardware objects
// ─────────────────────────────────────────────
Adafruit_SHTC3   shtc3;
Adafruit_TSL2591 tsl(2591);
TCA9534          gpio0;
WebServer        server(80);
DNSServer        dnsServer;

// ─────────────────────────────────────────────
// Local sensor state
// ─────────────────────────────────────────────
float    b1_tempC    = 0.0;
float    b1_tempF    = 0.0;
float    b1_humidity = 0.0;
float    b1_lux      = 0.0;
uint16_t b1_visible  = 0;
uint16_t b1_infrared = 0;
bool     b1_shtcOk   = false;
bool     b1_tslOk    = false;
float    b1_dieTemp  = 0.0;

// ─────────────────────────────────────────────
// Relay state
// ─────────────────────────────────────────────
bool gpioOk         = false;
bool relayState     = false;
bool manualOverride = true;
#define NUM_RELAYS 1

void applyRelay() {
  if (!gpioOk) return;
  gpio0.digitalWrite(0, relayState ? HIGH : LOW);
}

void forceAllRelaysOff() {
  relayState = false;
  if (gpioOk) gpio0.digitalWrite(0, LOW);
}

// ─────────────────────────────────────────────
// Rule engine — stubs (v1.2)
// ─────────────────────────────────────────────
#define MAX_CONDITIONS 16
#define MAX_RULES      16

struct Condition {
  char    source[48];  // e.g. "local.tempC" or "peer:AA:BB:CC:DD:EE:FF.tempC"
  uint8_t condOp;      // 0=above, 1=below, 2=equals
  float   threshold;
};

struct Rule {
  char      name[32];
  uint8_t   priority;
  uint8_t   op;                        // 0=AND, 1=OR, 2=NOT, 3=XOR
  bool      relayTargets[NUM_RELAYS];
  bool      relayAction;
  Condition conditions[MAX_CONDITIONS];
  uint8_t   conditionCount;
  bool      enabled;
};

Rule    rules[MAX_RULES];
uint8_t ruleCount = 0;

bool evalCondition(uint8_t ri, uint8_t ci) {
  const Condition& c = rules[ri].conditions[ci];
  float val = 0.0;
  bool  found = false;
  if      (strcmp(c.source, "local.tempC")    == 0) { val = b1_tempC;    found = true; }
  else if (strcmp(c.source, "local.tempF")    == 0) { val = b1_tempF;    found = true; }
  else if (strcmp(c.source, "local.humidity") == 0) { val = b1_humidity; found = true; }
  else if (strcmp(c.source, "local.lux")      == 0) { val = b1_lux;      found = true; }
  if (!found) return false;
  switch (c.condOp) {
    case 0: return val >  c.threshold;
    case 1: return val <  c.threshold;
    case 2: return fabsf(val - c.threshold) < 0.01f;
    default: return false;
  }
}

bool evalRule(uint8_t ri) {
  const Rule& r = rules[ri];
  if (!r.enabled || r.conditionCount == 0) return false;
  uint8_t trueCount = 0;
  for (uint8_t ci = 0; ci < r.conditionCount; ci++)
    if (evalCondition(ri, ci)) trueCount++;
  switch (r.op) {
    case 0: return trueCount == r.conditionCount;
    case 1: return trueCount > 0;
    case 2: return trueCount == 0;
    case 3: return trueCount == 1;
    default: return false;
  }
}

void evaluateRules() {
  if (manualOverride) return;
  for (uint8_t ri = 0; ri < NUM_RELAYS; ri++) {
    bool    found        = false;
    bool    winnerAction = false;
    uint8_t winnerPrio   = 255;
    for (uint8_t i = 0; i < ruleCount; i++) {
      if (!rules[i].enabled)          continue;
      if (!rules[i].relayTargets[ri]) continue;
      if (!evalRule(i))               continue;
      if (rules[i].priority < winnerPrio) {
        winnerPrio   = rules[i].priority;
        winnerAction = rules[i].relayAction;
        found        = true;
      }
    }
    if (ri == 0) relayState = found ? winnerAction : false;
  }
  applyRelay();
}

// ─────────────────────────────────────────────
// Plugin hooks — weak functions
// User implements these in a separate custom.ino file.
// Arduino concatenates all .ino files at compile time.
// ─────────────────────────────────────────────
void   __attribute__((weak)) customSetup()             {}
void   __attribute__((weak)) customLoop()              {}
String __attribute__((weak)) customDataJSON()          { return "{}"; }

// ─────────────────────────────────────────────
// Utility: MAC to string
// ─────────────────────────────────────────────
void macToStr(const uint8_t* mac, char* buf, size_t len) {
  snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool macEquals(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

// ─────────────────────────────────────────────
// Time functions
// ─────────────────────────────────────────────
uint32_t getUnixTime() {
  if (!timeValid) return 0;
  time_t now;
  time(&now);
  return (uint32_t)now;
}

uint32_t getSecondsSinceSync() {
  if (!timeValid || timeSyncSource == 0) return UINT32_MAX;
  return (uint32_t)((millis() - lastSyncMillis) / 1000UL);
}

void applyUnixTime(uint32_t unixTime, uint8_t source, uint32_t peerSecondsSinceSync) {
  time_t t = (time_t)unixTime;
  struct timeval tv = { .tv_sec = t };
  settimeofday(&tv, NULL);
  timeValid      = true;
  timeSyncSource = source;
  // Back-calculate when the source was actually synced
  lastSyncMillis = millis() - (peerSecondsSinceSync * 1000UL);
  Serial.printf("Time applied: unix=%u  source=%u  peerAge=%us\n",
                unixTime, source, peerSecondsSinceSync);
}

void ntpSync() {
  if (!wifiConnected) return;
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  // Wait up to 3 seconds for NTP response
  struct tm timeinfo;
  unsigned long start = millis();
  while (!getLocalTime(&timeinfo, 100) && millis() - start < 3000) {}
  if (getLocalTime(&timeinfo, 0)) {
    timeValid      = true;
    timeSyncSource = 3;  // NTP
    lastSyncMillis = millis();
    lastNtpSync    = millis();
    Serial.printf("NTP sync OK: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    Serial.println("NTP sync failed (no response).");
  }
}

void maybeSyncTimeFromPeer(uint32_t peerUnixTime, bool peerTimeValid,
                           uint8_t peerSyncSource, uint32_t peerSecondsSinceSync) {
  if (!peerTimeValid || peerUnixTime == 0) return;
  bool shouldSync =
    !timeValid ||
    (peerSyncSource > timeSyncSource) ||
    (peerSyncSource == timeSyncSource && peerSecondsSinceSync < getSecondsSinceSync());
  if (!shouldSync) return;
  // We are one hop away from the source — cap our source at "peer-inherited"
  uint8_t adoptedSource = (peerSyncSource >= 3) ? 3 : peerSyncSource;
  applyUnixTime(peerUnixTime, adoptedSource, peerSecondsSinceSync);
}

// ─────────────────────────────────────────────
// Peer management
// ─────────────────────────────────────────────

// Returns index of peer matching mac, or -1 if not found.
int findPeer(const uint8_t* mac) {
  for (int i = 0; i < MAX_PEERS; i++) {
    if (peers[i].active && macEquals(peers[i].mac, mac)) return i;
  }
  return -1;
}

// Returns index of first unused slot, or -1 if full.
int freePeerSlot() {
  for (int i = 0; i < MAX_PEERS; i++)
    if (!peers[i].active) return i;
  return -1;
}

// Recounts active peers.
void recountPeers() {
  peerCount = 0;
  for (int i = 0; i < MAX_PEERS; i++)
    if (peers[i].active) peerCount++;
}

// Register mac as an ESP-NOW unicast peer if not already registered.
void ensureEspNowPeer(const uint8_t* mac) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_err_t err = esp_now_add_peer(&peer);
  if (err == ESP_OK) {
    char ms[18]; macToStr(mac, ms, sizeof(ms));
    Serial.printf("ESP-NOW peer registered: %s\n", ms);
  }
}

// Process a received HelloPayload — update or add peer, sync time.
void processHello(const HelloPayload* h) {
  // Ignore beacons from ourselves
  if (macEquals(h->mac, myMac)) return;

  // Ignore boards on a different Network Name
  if (strcmp(h->networkName, networkName) != 0) return;

  // Signal channel scan that a matching beacon was heard on this channel
  channelLocked = true;

  // Register as ESP-NOW peer so we can send to them
  ensureEspNowPeer(h->mac);

  // Find or create peer slot
  int idx = findPeer(h->mac);
  if (idx < 0) {
    idx = freePeerSlot();
    if (idx < 0) {
      Serial.println("Peer table full — cannot add new peer.");
      return;
    }
    memset(&peers[idx], 0, sizeof(PeerInfo));
    memcpy(peers[idx].mac, h->mac, 6);
    peers[idx].active = true;
    char ms[18]; macToStr(h->mac, ms, sizeof(ms));
    Serial.printf("New peer discovered: %s  name=%s\n", ms, h->boardName);
  }

  // Update peer state
  strncpy(peers[idx].boardName, h->boardName, sizeof(peers[idx].boardName) - 1);
  peers[idx].hasWiFi          = h->hasWiFi;
  peers[idx].rssi             = h->rssi;
  peers[idx].lastSeen         = millis();
  peers[idx].unixTime         = h->unixTime;
  peers[idx].timeValid        = h->timeValid;
  peers[idx].timeSyncSource   = h->timeSyncSource;
  peers[idx].secondsSinceSync = h->secondsSinceSync;

  recountPeers();

  // Sync time from this peer if they have a better source
  maybeSyncTimeFromPeer(h->unixTime, h->timeValid,
                        h->timeSyncSource, h->secondsSinceSync);
}

// Mark peers as inactive if we haven't heard from them within timeout.
void timeoutPeers() {
  bool changed = false;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].active) continue;
    if (millis() - peers[i].lastSeen > PEER_TIMEOUT_MS) {
      char ms[18]; macToStr(peers[i].mac, ms, sizeof(ms));
      Serial.printf("Peer timed out: %s  name=%s\n", ms, peers[i].boardName);
      peers[i].active = false;
      changed = true;
    }
  }
  if (changed) recountPeers();
}

// ─────────────────────────────────────────────
// Gateway election
// Any board with active WiFi is a candidate.
// Highest RSSI wins. Lowest MAC breaks ties.
// ─────────────────────────────────────────────
void startNetworkMdns();
void stopNetworkMdns();

void electGateway() {
  int8_t   bestRssi = -128;
  uint8_t  bestMac[6] = {};
  bool     found = false;

  // Include self as a candidate
  if (wifiConnected) {
    int8_t myRssi = (int8_t)WiFi.RSSI();
    bestRssi = myRssi;
    memcpy(bestMac, myMac, 6);
    found = true;
  }

  // Include active peers
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].active) continue;
    if (!peers[i].hasWiFi) continue;
    int8_t r = peers[i].rssi;
    bool betterRssi = r > bestRssi;
    bool tiebreak   = (r == bestRssi) && memcmp(peers[i].mac, bestMac, 6) < 0;
    if (!found || betterRssi || tiebreak) {
      bestRssi = r;
      memcpy(bestMac, peers[i].mac, 6);
      found = true;
    }
  }

  wasGateway = isGateway;
  isGateway  = found && macEquals(bestMac, myMac);

  if (found) {
    memcpy(gatewayMac, bestMac, 6);
    gatewayKnown = true;
  } else {
    gatewayKnown = false;
  }

  if (isGateway && !wasGateway) {
    Serial.println("Gateway elected: this board.");
    startNetworkMdns();
    beaconFastStartMs = millis();   // re-enter fast mode to announce new role
    beaconFastMode    = true;
  } else if (!isGateway && wasGateway) {
    Serial.println("Gateway role transferred to another board.");
    stopNetworkMdns();
    beaconFastStartMs = millis();
    beaconFastMode    = true;
  }
}

// Advertise networkName.local in addition to boardname.local
void startNetworkMdns() {
  char netMdns[33];
  strncpy(netMdns, networkName, sizeof(netMdns));
  for (int i = 0; netMdns[i]; i++) {
    netMdns[i] = tolower((unsigned char)netMdns[i]);
    if (netMdns[i] == ' ') netMdns[i] = '-';
  }
  MDNS.end();
  if (MDNS.begin(netMdns)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf(">>> Gateway URL: http://%s.local (board name URL disabled while gateway)\n", netMdns);
  } else {
    Serial.printf("Gateway mDNS begin failed for %s.local\n", netMdns);
  }
}

void stopNetworkMdns() {
  // ESPmDNS doesn't expose a remove-hostname API in Arduino.
  // Restart mDNS with just the board name.
  MDNS.end();
  if (MDNS.begin(mdnsName)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS restarted (board only): http://%s.local\n", mdnsName);
  }
}

// ─────────────────────────────────────────────
// Beacon
// ─────────────────────────────────────────────
void sendBeacon() {
  HelloPayload h = {};
  h.type            = MSG_HELLO;
  strncpy(h.networkName, networkName, sizeof(h.networkName) - 1);
  strncpy(h.boardName,   boardName[0] ? boardName : mdnsName, sizeof(h.boardName) - 1);
  memcpy(h.mac, myMac, 6);
  h.hasWiFi         = wifiConnected;
  h.rssi            = wifiConnected ? (int8_t)WiFi.RSSI() : -127;
  h.unixTime        = getUnixTime();
  h.timeValid       = timeValid;
  h.timeSyncSource  = timeSyncSource;
  h.secondsSinceSync = getSecondsSinceSync();

  esp_now_send(broadcastMac, (uint8_t*)&h, sizeof(h));
}

// ─────────────────────────────────────────────
// Sensor broadcast (non-gateway → gateway)
// ─────────────────────────────────────────────
void sendSensorToGateway() {
  if (isGateway) return;          // gateway doesn't relay to itself
  if (!gatewayKnown) return;      // no gateway elected yet
  if (!esp_now_is_peer_exist(gatewayMac)) return;

  SensorBroadcastPayload s = {};
  s.type     = MSG_SENSOR;
  memcpy(s.mac, myMac, 6);
  s.tempC    = b1_tempC;
  s.tempF    = b1_tempF;
  s.humidity = b1_humidity;
  s.lux      = b1_lux;
  s.visible  = b1_visible;
  s.infrared = b1_infrared;
  s.shtcOk   = b1_shtcOk;
  s.tslOk    = b1_tslOk;
  s.uptime   = millis() / 1000;

  esp_now_send(gatewayMac, (uint8_t*)&s, sizeof(s));
}

// ─────────────────────────────────────────────
// ESP-NOW receive callback
// ─────────────────────────────────────────────
void onDataReceived(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len < 1) return;
  uint8_t msgType = data[0];

  switch (msgType) {

    case MSG_HELLO: {
      if (len < (int)sizeof(HelloPayload)) return;
      const HelloPayload* h = (const HelloPayload*)data;
      processHello(h);
      electGateway();
      break;
    }

    case MSG_SENSOR: {
      if (len < (int)sizeof(SensorBroadcastPayload)) return;
      const SensorBroadcastPayload* s = (const SensorBroadcastPayload*)data;
      int idx = findPeer(s->mac);
      if (idx < 0) return;  // don't accept sensor data from unregistered peers
      peers[idx].tempC             = s->tempC;
      peers[idx].tempF             = s->tempF;
      peers[idx].humidity          = s->humidity;
      peers[idx].lux               = s->lux;
      peers[idx].visible           = s->visible;
      peers[idx].infrared          = s->infrared;
      peers[idx].shtcOk            = s->shtcOk;
      peers[idx].tslOk             = s->tslOk;
      peers[idx].sensorUptime      = s->uptime;
      peers[idx].sensorLastUpdated = millis();
      char ms[18]; macToStr(s->mac, ms, sizeof(ms));
      Serial.printf("Sensor data from %s: %.1f°C  %.1f%%  %.1f lux\n",
                    ms, s->tempC, s->humidity, s->lux);
      break;
    }

    case MSG_SCAN_RESPONSE: {
      if (len < (int)sizeof(ScanResponsePayload)) return;
      const ScanResponsePayload* resp = (const ScanResponsePayload*)data;
      int idx = findPeer(info->src_addr);
      if (idx < 0) return;
      peers[idx].scanCount = min((int)resp->count, 32);
      memcpy(peers[idx].scanAddrs, resp->addrs, peers[idx].scanCount);
      peers[idx].scanReady = true;
      Serial.printf("Remote scan response from peer: %d device(s)\n", peers[idx].scanCount);
      break;
    }

    case MSG_SCAN_REQUEST: {
      // This board received a scan request — perform local I2C scan and reply
      uint8_t addrs[32];
      uint8_t count = 0;
      for (uint8_t addr = 0x08; addr <= 0x77 && count < 32; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) addrs[count++] = addr;
      }
      ScanResponsePayload resp = {};
      resp.type  = MSG_SCAN_RESPONSE;
      resp.count = count;
      memcpy(resp.addrs, addrs, count);
      esp_now_send(info->src_addr, (uint8_t*)&resp, sizeof(resp));
      Serial.printf("I2C scan requested by peer — found %d device(s), replied.\n", count);
      break;
    }

    default:
      break;
  }
}

// ─────────────────────────────────────────────
// Read local sensors
// ─────────────────────────────────────────────
void readLocalSensors() {
  sensors_event_t hum_evt, temp_evt;
  shtc3.getEvent(&hum_evt, &temp_evt);
  if (!isnan(temp_evt.temperature)) {
    b1_tempC    = temp_evt.temperature;
    b1_tempF    = b1_tempC * 9.0 / 5.0 + 32.0;
    b1_humidity = hum_evt.relative_humidity;
    b1_shtcOk   = true;
  } else {
    b1_shtcOk = false;
  }
  if (b1_tslOk) {
    uint32_t lum = tsl.getFullLuminosity();
    b1_infrared  = lum >> 16;
    b1_visible   = lum & 0xFFFF;
    float lux    = tsl.calculateLux(b1_visible, b1_infrared);
    b1_lux       = (lux < 0) ? 0 : lux;
  }
  b1_dieTemp = temperatureRead();
}

// ─────────────────────────────────────────────
// HTTP: /data
// ─────────────────────────────────────────────
void handleData() {
  readLocalSensors();

  const char* gainLabel = "MED (25x)";
  switch (tsl.getGain()) {
    case TSL2591_GAIN_LOW:  gainLabel = "LOW (1x)";    break;
    case TSL2591_GAIN_MED:  gainLabel = "MED (25x)";   break;
    case TSL2591_GAIN_HIGH: gainLabel = "HIGH (428x)"; break;
    case TSL2591_GAIN_MAX:  gainLabel = "MAX (9876x)"; break;
  }

  // Merge customDataJSON() output into response
  String customJson = customDataJSON();
  if (customJson == "null" || customJson.length() == 0) customJson = "{}";

  char json[1400];
  snprintf(json, sizeof(json),
    "{"
      "\"b1\":{"
        "\"tempC\":%.2f,\"tempF\":%.2f,\"humidity\":%.2f,"
        "\"lux\":%.2f,\"visible\":%u,\"infrared\":%u,"
        "\"shtcOk\":%s,\"tslOk\":%s,\"gain\":\"%s\","
        "\"uptime\":%lu,\"dieTemp\":%.1f"
      "},"
      "\"relay\":{"
        "\"state\":%s,\"manualOverride\":%s,\"gpioOk\":%s"
      "},"
      "\"network\":{"
        "\"isGateway\":%s,\"peerCount\":%u"
      "},"
      "\"custom\":%s"
    "}",
    b1_tempC, b1_tempF, b1_humidity,
    b1_lux, b1_visible, b1_infrared,
    b1_shtcOk ? "true" : "false",
    b1_tslOk  ? "true" : "false",
    gainLabel, millis() / 1000, b1_dieTemp,
    relayState     ? "true" : "false",
    manualOverride ? "true" : "false",
    gpioOk         ? "true" : "false",
    isGateway  ? "true" : "false",
    peerCount,
    customJson.c_str()
  );

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ─────────────────────────────────────────────
// HTTP: /peers  GET
// Returns all active peers as JSON array
// ─────────────────────────────────────────────
void handlePeers() {
  String json = "{\"peers\":[";
  bool first = true;
  char macStr[18];
  char gatewayMacStr[18];
  macToStr(gatewayMac, gatewayMacStr, sizeof(gatewayMacStr));

  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].active) continue;
    if (!first) json += ",";
    first = false;

    macToStr(peers[i].mac, macStr, sizeof(macStr));
    bool stale = (peers[i].sensorLastUpdated == 0) ||
                 (millis() - peers[i].sensorLastUpdated > 90000);

    char buf[512];
    snprintf(buf, sizeof(buf),
      "{"
        "\"mac\":\"%s\","
        "\"boardName\":\"%s\","
        "\"hasWiFi\":%s,"
        "\"rssi\":%d,"
        "\"lastSeenMs\":%lu,"
        "\"tempC\":%.2f,\"tempF\":%.2f,\"humidity\":%.2f,\"lux\":%.2f,"
        "\"shtcOk\":%s,\"tslOk\":%s,"
        "\"sensorStale\":%s,"
        "\"timeValid\":%s,"
        "\"timeSyncSource\":%u"
      "}",
      macStr,
      peers[i].boardName,
      peers[i].hasWiFi ? "true" : "false",
      peers[i].rssi,
      millis() - peers[i].lastSeen,
      peers[i].tempC, peers[i].tempF,
      peers[i].humidity, peers[i].lux,
      peers[i].shtcOk ? "true" : "false",
      peers[i].tslOk  ? "true" : "false",
      stale ? "true" : "false",
      peers[i].timeValid ? "true" : "false",
      peers[i].timeSyncSource
    );
    json += buf;
  }

  json += "],";
  json += "\"isGateway\":";
  json += isGateway ? "true" : "false";
  json += ",\"gatewayMac\":\"";
  json += gatewayKnown ? gatewayMacStr : "";
  json += "\",\"peerCount\":";
  json += peerCount;
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ─────────────────────────────────────────────
// HTTP: /time  POST
// Sets board time from browser's clock.
// Body params: unixtime=<unix_timestamp>  source=<string>
// ─────────────────────────────────────────────
void handleTimePost() {
  if (!server.hasArg("unixtime") || server.arg("unixtime").length() == 0) {
    server.send(400, "application/json", "{\"error\":\"missing unixtime\"}");
    return;
  }

  uint32_t t = (uint32_t)server.arg("unixtime").toInt();
  if (t < 1700000000UL) {  // sanity check — reject obviously wrong timestamps
    server.send(400, "application/json", "{\"error\":\"timestamp out of range\"}");
    return;
  }

  String src = server.arg("source");  // "manual", "browser", etc.

  applyUnixTime(t, 2, 0);  // source=2 (manual/browser), age=0 (just set)

  // Persist last sync metadata to NVS
  prefs.begin("automato", false);
  prefs.putULong("lastTimeSync", t);
  prefs.putString("timeSyncSrc", src.length() > 0 ? src : "manual");
  prefs.end();

  // Trigger fast beacon to propagate new time to peers immediately
  beaconFastStartMs = millis();
  beaconFastMode    = true;

  Serial.printf("Time set manually: unix=%u  source=%s\n", t, src.c_str());
  server.send(200, "application/json", "{\"ok\":true}");
}

// ─────────────────────────────────────────────
// HTTP: /remote-scan  (retained from v0.9)
// POST → send I2C scan request to first active peer
// GET  → return first peer's last scan results
// ─────────────────────────────────────────────
void handleRemoteScan() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  if (server.method() == HTTP_POST) {
    // Find first active peer
    int idx = -1;
    for (int i = 0; i < MAX_PEERS; i++) {
      if (peers[i].active) { idx = i; break; }
    }
    if (idx < 0) {
      server.send(200, "application/json",
        "{\"ok\":false,\"error\":\"No peer boards found\"}");
      return;
    }
    peers[idx].scanReady = false;
    ScanRequestPayload req = {MSG_SCAN_REQUEST};
    esp_now_send(peers[idx].mac, (uint8_t*)&req, sizeof(req));
    server.send(200, "application/json", "{\"ok\":true}");
    Serial.println("Remote I2C scan requested.");
    return;
  }

  // GET — return results from first active peer with scanReady
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].active || !peers[i].scanReady) continue;
    String json = "{\"ready\":true,\"devices\":[";
    for (int j = 0; j < peers[i].scanCount; j++) {
      if (j > 0) json += ",";
      json += peers[i].scanAddrs[j];
    }
    json += "]}";
    server.send(200, "application/json", json);
    return;
  }
  server.send(200, "application/json", "{\"ready\":false}");
}

// ─────────────────────────────────────────────
// HTTP: /relay
// ─────────────────────────────────────────────
void handleRelay() {
  if (server.hasArg("override") && server.arg("override") == "auto") {
    manualOverride = false;
    forceAllRelaysOff();
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json",
      "{\"ok\":true,\"manualOverride\":false,\"state\":false}");
    return;
  }
  if (server.hasArg("state")) {
    manualOverride = true;
    relayState     = (server.arg("state") == "1");
    applyRelay();
    char resp[80];
    snprintf(resp, sizeof(resp),
      "{\"ok\":true,\"manualOverride\":true,\"state\":%s}",
      relayState ? "true" : "false");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", resp);
    return;
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(400, "application/json",
    "{\"ok\":false,\"error\":\"Missing state or override arg\"}");
}

// ─────────────────────────────────────────────
// HTTP: /relay/status
// ─────────────────────────────────────────────
void handleRelayStatus() {
  char resp[100];
  snprintf(resp, sizeof(resp),
    "{\"state\":%s,\"manualOverride\":%s,\"gpioOk\":%s}",
    relayState     ? "true" : "false",
    manualOverride ? "true" : "false",
    gpioOk         ? "true" : "false");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", resp);
}

// ─────────────────────────────────────────────
// HTTP: /i2c-scan
// ─────────────────────────────────────────────
void handleI2CScan() {
  String json = "{\"devices\":[";
  bool   first = true;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      if (!first) json += ",";
      json += String(addr);
      first = false;
      Serial.printf("I2C scan: found 0x%02X (%d)\n", addr, addr);
    }
  }
  json += "]}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
  Serial.println(first ? "I2C scan complete — no devices found."
                       : "I2C scan complete.");
}

// ─────────────────────────────────────────────
// HTTP: /version
// ─────────────────────────────────────────────
void handleVersion() {
  char timeBuf[32] = "unknown";
  if (timeValid) {
    struct tm ti;
    if (getLocalTime(&ti, 0))
      strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &ti);
  }

  char json[640];
  snprintf(json, sizeof(json),
    "{"
      "\"firmware\":\"%s\",\"webapp\":\"%s\",\"lfs\":%s,"
      "\"wifiConnected\":%s,\"hasCredentials\":%s,"
      "\"apSSID\":\"%s\",\"mdns\":\"%s.local\","
      "\"networkName\":\"%s\","
      "\"isGateway\":%s,\"peerCount\":%u,"
      "\"timeValid\":%s,\"timeSyncSource\":%u,"
      "\"boardTime\":\"%s\","
      "\"softApMode\":%u,\"softApActive\":%s"
    "}",
    FIRMWARE_VERSION, webappVersion,
    lfsOk          ? "true" : "false",
    wifiConnected  ? "true" : "false",
    hasCredentials ? "true" : "false",
    apSSID, mdnsName, networkName,
    isGateway  ? "true" : "false",
    peerCount,
    timeValid  ? "true" : "false",
    timeSyncSource,
    timeBuf,
    softApMode,
    softApActive ? "true" : "false"
  );
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ─────────────────────────────────────────────
// HTTP: /softap  POST
// ─────────────────────────────────────────────
void handleSoftApPost() {
  String mode = server.arg("mode");
  uint8_t m = (mode == "on") ? 1 : 0;
  prefs.begin("automato", false);
  prefs.putUChar("softap", m);
  prefs.end();
  softApMode = m;
  if (m == 1) {
    enableSoftAP();
  } else if (wifiConnected && softApTempUntil == 0) {
    disableSoftAP();
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

// ─────────────────────────────────────────────
// HTTP: /setup  GET
// ─────────────────────────────────────────────
void handleSetupGet() {
  String html = F(
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Automato Setup</title>"
    "<style>"
    "body{font-family:monospace;background:#090b0e;color:#dde3ee;margin:0;padding:24px;}"
    "h1{color:#00e5ff;font-size:1.4rem;margin-bottom:4px;}"
    "p{color:#4a5568;font-size:0.8rem;margin-bottom:24px;}"
    ".card{background:#10141a;border:1px solid #1a2030;border-radius:10px;padding:24px;max-width:400px;}"
    "label{display:block;font-size:0.65rem;text-transform:uppercase;letter-spacing:0.1em;color:#4a5568;margin-bottom:5px;margin-top:16px;}"
    "input{width:100%;box-sizing:border-box;background:#090b0e;border:1px solid #1a2030;border-radius:6px;"
    "padding:10px;color:#dde3ee;font-family:monospace;font-size:0.85rem;outline:none;}"
    "input:focus{border-color:rgba(0,229,255,0.4);}"
    "button{margin-top:20px;width:100%;padding:12px;background:#00e5ff;color:#000;border:none;"
    "border-radius:6px;font-family:monospace;font-size:0.85rem;font-weight:700;cursor:pointer;}"
    ".note{font-size:0.6rem;color:#4a5568;margin-top:12px;line-height:1.7;}"
    "</style></head><body>"
    "<h1>Automato Setup</h1>"
    "<p>Connect your board to your WiFi network.</p>"
    "<div class='card'>"
    "<form method='POST' action='/setup'>"
    "<label>WiFi Network Name (SSID)</label>"
    "<input type='text' name='ssid' placeholder='Your network name' required>"
    "<label>WiFi Password</label>"
    "<input type='password' name='pass' placeholder='Your password'>"
    "<label>Board Name <span style='color:#4a5568'>(optional)</span></label>"
    "<input type='text' name='name' placeholder='e.g. greenhouse-north' maxlength='31'>"
    "<div class='note'>Sets your board address: boardname.local<br>"
    "Leave blank to use default: automato-XXXX.local</div>"
    "<hr style='border:none;border-top:1px solid #1a2030;margin:20px 0;'>"
    "<label>Automato Network Name</label>"
    "<input type='text' name='netname' id='nname' placeholder='e.g. my-garden' maxlength='32'>"
    "<label>Network Password <span style='color:#4a5568'>(recommended)</span></label>"
    "<input type='password' name='netpass' placeholder='Network password' maxlength='32'>"
    "<div class='note'>All boards with the same Network Name find each other automatically.</div>"
    "<button type='submit'>Save &amp; Connect</button>"
    "</form>"
    "<script>"
    "fetch('/version').then(r=>r.json()).then(d=>{"
    "  if(d.networkName)document.getElementById('nname').value=d.networkName;"
    "});"
    "</script>"
    "</div></body></html>"
  );
  server.send(200, "text/html", html);
}

// ─────────────────────────────────────────────
// HTTP: /settings  POST
// ─────────────────────────────────────────────
void handleSettingsPost() {
  String name    = server.arg("name");
  String netname = server.arg("netname");
  String netpass = server.arg("netpass");
  name.trim(); netname.trim();
  prefs.begin("automato", false);
  if (name.length()    > 0) prefs.putString("name",    name);
  if (netname.length() > 0) prefs.putString("netname", netname);
  prefs.putString("netpass", netpass);
  prefs.end();
  Serial.printf("Settings saved. Name: %s  Network: %s\n",
                name.length()    > 0 ? name.c_str()    : "(unchanged)",
                netname.length() > 0 ? netname.c_str() : "(unchanged)");
  server.send(200, "application/json", "{\"ok\":true}");
}

// ─────────────────────────────────────────────
// HTTP: /setup  POST
// ─────────────────────────────────────────────
void handleSetupPost() {
  if (!server.hasArg("ssid") || server.arg("ssid").length() == 0) {
    server.send(400, "text/plain", "SSID required.");
    return;
  }
  String ssid    = server.arg("ssid");
  String pass    = server.arg("pass");
  String bname   = server.arg("name");
  String netname = server.arg("netname");
  String netpass = server.arg("netpass");
  bname.trim(); netname.trim();
  prefs.begin("automato", false);
  prefs.putString("ssid",    ssid);
  prefs.putString("pass",    pass);
  prefs.putString("name",    bname);
  if (netname.length() > 0) prefs.putString("netname", netname);
  prefs.putString("netpass", netpass);
  prefs.end();
  Serial.printf("Credentials saved. SSID: %s  Name: %s  Network: %s\n",
                ssid.c_str(),
                bname.length()   > 0 ? bname.c_str()   : "(default)",
                netname.length() > 0 ? netname.c_str() : "(unchanged)");
  String html = F(
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Automato Setup</title>"
    "<style>body{font-family:monospace;background:#090b0e;color:#dde3ee;margin:0;padding:24px;}"
    "h1{color:#2ddf82;}p{color:#4a5568;font-size:0.8rem;}"
    ".card{background:#10141a;border:1px solid #1a2030;border-radius:10px;padding:24px;max-width:400px;}"
    "</style></head><body><h1>Saved!</h1><div class='card'>"
    "<p>Connecting to your network now.</p>"
    "<p style='margin-top:12px;color:#00e5ff;' id='msg'>Rebooting in 3 seconds...</p>"
    "</div><script>var s=3,el=document.getElementById('msg');"
    "var iv=setInterval(function(){s--;if(s>0){el.textContent='Rebooting in '+s+'...';}else"
    "{el.textContent='Rebooting... reconnect to your WiFi.';clearInterval(iv);}},1000);</script>"
    "</body></html>"
  );
  server.send(200, "text/html", html);
  delay(3000);
  ESP.restart();
}

// ─────────────────────────────────────────────
// HTTP: /wifi/reset  POST
// ─────────────────────────────────────────────
void handleWifiReset() {
  // Remove only WiFi credentials — preserve board name, network name,
  // network password, SoftAP mode, and time sync metadata.
  prefs.begin("automato", false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.end();
  Serial.println("WiFi credentials cleared. Rebooting.");
  server.send(200, "application/json",
    "{\"ok\":true,\"message\":\"Credentials cleared. Rebooting.\"}");
  delay(500);
  ESP.restart();
}

// ─────────────────────────────────────────────
// HTTP: /  (serve index.html from LittleFS)
// ─────────────────────────────────────────────
void handleRoot() {
  if (!lfsOk) {
    server.send(503, "text/plain",
      "LittleFS unavailable. Upload webapp via http://192.168.4.1/update");
    return;
  }
  File f = LittleFS.open("/index.html", "r");
  if (!f) {
    server.send(404, "text/plain",
      "Webapp not found. Upload via /update → Webapp tab.");
    return;
  }
  server.streamFile(f, "text/html");
  f.close();
}

// ─────────────────────────────────────────────
// OTA update page — served from PROGMEM
// ─────────────────────────────────────────────
static const char UPDATE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Automato OTA Update</title>
<style>
body{font-family:monospace;background:#090b0e;color:#dde3ee;margin:0;padding:24px;}
h1{color:#00e5ff;font-size:1.4rem;margin-bottom:4px;}
.card{background:#10141a;border:1px solid #1a2030;border-radius:10px;padding:24px;max-width:440px;margin-bottom:16px;}
h3{color:#dde3ee;font-size:0.85rem;margin-bottom:8px;}
p{color:#4a5568;font-size:0.75rem;margin-bottom:12px;}
input[type=file]{color:#dde3ee;font-size:0.8rem;margin-bottom:12px;}
button{padding:10px 20px;background:#00e5ff;color:#000;border:none;border-radius:6px;
  font-family:monospace;font-size:0.85rem;font-weight:700;cursor:pointer;}
.ok{color:#2ddf82;margin-top:12px;display:none;}
.err{color:#ff4d6d;margin-top:12px;display:none;}
a{color:#4a5568;}
</style></head><body>
<h1>OTA Update</h1>
<div class="card">
  <h3>Firmware</h3>
  <p>Compiled .bin file. Replaces firmware without touching webapp.</p>
  <form id="fw-form">
    <input type="file" id="fw-file" accept=".bin">
    <button type="submit">Upload Firmware</button>
  </form>
</div>
<div class="card">
  <h3>Webapp</h3>
  <p>LittleFS filesystem image. Updates dashboard without reflashing firmware.</p>
  <form id="fs-form">
    <input type="file" id="fs-file" accept=".bin">
    <button type="submit">Upload Webapp</button>
  </form>
</div>
<div id="msg"></div>
<p><a href="/">← Back to dashboard</a></p>
<script>
function upload(url, fileInput) {
  const file = fileInput.files[0];
  if (!file) { showMsg('No file selected.', false); return; }
  const xhr = new XMLHttpRequest();
  xhr.open('POST', url, true);
  xhr.onload = () => {
    if (xhr.status === 200) {
      let secs = 10;
      function tick() {
        showMsg(xhr.responseText + ' Rebooting. Returning in ' + secs + 's...', true);
        if (secs-- > 0) setTimeout(tick, 1000);
        else window.location.href = '/';
      }
      tick();
    } else { showMsg('Error: ' + xhr.responseText, false); }
  };
  xhr.onerror = () => showMsg('Upload failed.', false);
  const fd = new FormData();
  fd.append('file', file, file.name);
  showMsg('Uploading...', true);
  xhr.send(fd);
}
function showMsg(text, ok) {
  const el = document.getElementById('msg');
  el.textContent = text;
  el.className = ok ? 'ok' : 'err';
  el.style.display = 'block';
}
document.getElementById('fw-form').onsubmit = e => {
  e.preventDefault(); upload('/update/firmware', document.getElementById('fw-file'));
};
document.getElementById('fs-form').onsubmit = e => {
  e.preventDefault(); upload('/update/filesystem', document.getElementById('fs-file'));
};
</script>
</body></html>
)rawhtml";

void handleUpdatePage() {
  server.send_P(200, "text/html", UPDATE_HTML);
}

void handleUpdateFirmware() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA firmware: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
      Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true))
      Serial.printf("OTA firmware OK: %u bytes. Rebooting.\n", upload.totalSize);
    else Update.printError(Serial);
  }
}

void handleUpdateFirmwareDone() {
  if (Update.hasError()) {
    server.send(500, "text/plain",
      String("Firmware update failed: ") + Update.errorString());
  } else {
    server.send(200, "text/plain", "Firmware updated.");
    delay(500); ESP.restart();
  }
}

void handleUpdateFilesystem() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA filesystem: %s\n", upload.filename.c_str());
    LittleFS.end();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
      Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true))
      Serial.printf("OTA filesystem OK: %u bytes. Rebooting.\n", upload.totalSize);
    else Update.printError(Serial);
  }
}

void handleUpdateFilesystemDone() {
  if (Update.hasError()) {
    server.send(500, "text/plain",
      String("Filesystem update failed: ") + Update.errorString());
    lfsOk = LittleFS.begin(false);
  } else {
    server.send(200, "text/plain", "Webapp updated.");
    delay(500); ESP.restart();
  }
}

// ─────────────────────────────────────────────
// Channel seek helpers
// ─────────────────────────────────────────────

// Restart the SoftAP on a specific channel.
// Keeping the AP active (rather than disabling it) is critical — when both
// STA and AP are idle, the ESP32-C6 radio enters a passive state and stops
// receiving ESP-NOW frames even though esp_wifi_set_channel() reports success.
// The AP keeps the radio in active RX mode on the current channel.
void setSeekChannel(uint8_t ch) {
  meshChannel = ch;
  const char* pw = (strlen(networkPass) > 0) ? networkPass : nullptr;
  dnsServer.stop();
  WiFi.softAP(apSSID, pw, ch, false, 4);
  delay(100);
  dnsServer.start(53, "*", WiFi.softAPIP());
  softApActive = true;
}

void startChannelSeek() {
  channelLocked     = false;
  seekChannelIdx    = 0;
  seekLastAdvanceMs = millis();
  seekStartMs       = millis();
  setSeekChannel(1);   // (re)start AP on ch 1 — radio stays active for RX
  Serial.println("Channel seek: started.");
}

// ─────────────────────────────────────────────
// SoftAP helpers
// ─────────────────────────────────────────────
void enableSoftAP() {
  if (softApActive) return;
  const char* pw = (strlen(networkPass) > 0) ? networkPass : nullptr;
  // Use meshChannel so the AP starts on the same channel as the mesh.
  // When WiFi is connected the radio follows the router anyway, so
  // meshChannel (default 1) is a safe fallback in that case too.
  WiFi.softAP(apSSID, pw, meshChannel, false, 4);
  delay(200);
  dnsServer.start(53, "*", WiFi.softAPIP());
  softApActive = true;
  Serial.printf("SoftAP: enabled on ch %u.\n", meshChannel);
}

void disableSoftAP() {
  if (!softApActive) return;
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  softApActive = false;
  Serial.println("SoftAP: disabled.");
}

// ─────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_LED_R, OUTPUT); digitalWrite(PIN_LED_R, LOW);
  pinMode(PIN_LED_B, OUTPUT); digitalWrite(PIN_LED_B, LOW);

  Serial.printf("\n=== Automato Brain Board v%s ===\n", FIRMWARE_VERSION);

  // ── LittleFS ────────────────────────────────
  Serial.print("LittleFS... ");
  lfsOk = LittleFS.begin(false);
  if (!lfsOk) {
    Serial.print("mount failed, formatting... ");
    LittleFS.format();
    lfsOk = LittleFS.begin(false);
  }
  if (lfsOk) {
    if (!LittleFS.exists("/version.txt")) {
      File f = LittleFS.open("/version.txt", "w");
      if (f) { f.println(WEBAPP_VERSION); f.close(); }
    }
    File vf = LittleFS.open("/version.txt", "r");
    if (vf) {
      String v = vf.readStringUntil('\n'); v.trim();
      v.toCharArray(webappVersion, sizeof(webappVersion));
      vf.close();
    }
    Serial.printf("OK (firmware v%s / webapp v%s)\n",
                  FIRMWARE_VERSION, webappVersion);
  } else {
    Serial.println("FAILED");
  }

  // ── I2C + Hardware ──────────────────────────
  Wire.begin(PIN_SDA, PIN_SCL);

  Serial.print("TCA9534... ");
  if (gpio0.begin(Wire, 0x27)) {
    gpio0.pinMode(0, GPIO_OUT);
    gpio0.digitalWrite(0, LOW);
    gpioOk = true;
    Serial.println("OK");
  } else {
    Serial.println("FAILED — relay unavailable");
  }

  Serial.print("SHTC3... ");
  if (shtc3.begin()) { b1_shtcOk = true; Serial.println("OK"); }
  else Serial.println("FAILED");

  Serial.print("TSL2591... ");
  if (tsl.begin()) {
    tsl.setGain(TSL2591_GAIN_MED);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
    b1_tslOk = true;
    Serial.println("OK");
  } else Serial.println("FAILED");

  // ── Plugin hook ─────────────────────────────
  customSetup();

  // ── Boot button ─────────────────────────────
  delay(1500);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  Serial.print("Boot button check... ");
  if (digitalRead(PIN_BOOT) == LOW) {
    Serial.print("held, waiting 5s... ");
    unsigned long held = millis();
    while (digitalRead(PIN_BOOT) == LOW && millis() - held < 5000) {
      delay(100);
      digitalWrite(PIN_LED_R, (millis() / 200) % 2);
    }
    if (millis() - held >= 5000) {
      // Blink both LEDs rapidly until button is released
      while (digitalRead(PIN_BOOT) == LOW) {
        digitalWrite(PIN_LED_R, (millis() / 100) % 2);
        digitalWrite(PIN_LED_B, (millis() / 100) % 2);
        delay(20);
      }
      digitalWrite(PIN_LED_R, LOW);
      digitalWrite(PIN_LED_B, LOW);
      prefs.begin("automato", false);
      prefs.clear();
      prefs.end();
      Serial.println("credentials cleared. Rebooting.");
      delay(500);
      ESP.restart();
    }
  }
  digitalWrite(PIN_LED_R, LOW);
  Serial.println("OK");

  // ── Load NVS ────────────────────────────────
  prefs.begin("automato", true);
  String ssid    = prefs.getString("ssid",    "");
  String pass    = prefs.getString("pass",    "");
  String bname   = prefs.getString("name",    "");
  String netname = prefs.getString("netname", "");
  String netpass = prefs.getString("netpass", "");
  softApMode     = prefs.getUChar ("softap",  0);
  prefs.end();

  ssid.toCharArray(wifiSSID,     sizeof(wifiSSID));
  pass.toCharArray(wifiPassword, sizeof(wifiPassword));
  bname.toCharArray(boardName,   sizeof(boardName));
  netpass.toCharArray(networkPass, sizeof(networkPass));
  hasCredentials = (strlen(wifiSSID) > 0);

  // ── WiFi ────────────────────────────────────
  WiFi.mode(WIFI_AP_STA);
  delay(100);

  // Capture MAC after radio init
  WiFi.macAddress(myMac);

  String mac     = WiFi.macAddress();
  String macSufx = mac.substring(12, 14) + mac.substring(15, 17);
  macSufx.toUpperCase();
  macSufx.replace(":", "");

  // Network Name
  if (netname.length() > 0) {
    netname.toCharArray(networkName, sizeof(networkName));
  } else {
    String defaultNet = String("automato-") + macSufx;
    defaultNet.toLowerCase();
    defaultNet.toCharArray(networkName, sizeof(networkName));
  }
  Serial.printf("Network Name: %s\n", networkName);

  // Board name → AP SSID + mDNS
  if (strlen(boardName) > 0) {
    snprintf(apSSID, sizeof(apSSID), "%s", boardName);
    String mn = bname; mn.toLowerCase(); mn.replace(" ", "-");
    mn.toCharArray(mdnsName, sizeof(mdnsName));
  } else {
    snprintf(apSSID,   sizeof(apSSID),   "Automato-%s", macSufx.c_str());
    snprintf(mdnsName, sizeof(mdnsName), "automato-%s", macSufx.c_str());
    for (int i = 0; mdnsName[i]; i++) mdnsName[i] = tolower(mdnsName[i]);
  }

  // SoftAP
  {
    const char* apPw = (strlen(networkPass) > 0) ? networkPass : nullptr;
    WiFi.softAP(apSSID, apPw, 1, false, 4);
  }
  delay(200);
  Serial.printf("SoftAP: %s  IP=%s\n",
                apSSID, WiFi.softAPIP().toString().c_str());
  dnsServer.start(53, "*", WiFi.softAPIP());

  Serial.printf("Board MAC: %s\n", WiFi.macAddress().c_str());

  // STA connection
  if (hasCredentials) {
    Serial.printf("Connecting to WiFi: %s\n", wifiSSID);
    WiFi.begin(wifiSSID, wifiPassword);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
      delay(500); Serial.print("."); tries++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.printf("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("Dashboard: http://%s.local\n", mdnsName);
      digitalWrite(PIN_LED_B, HIGH);
      if (MDNS.begin(mdnsName)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("mDNS: http://%s.local\n", mdnsName);
      }
      // NTP on first connect
      ntpSync();
      // Auto mode: shut down SoftAP now that WiFi is connected
      if (softApMode == 0) disableSoftAP();
    } else {
      Serial.println("WiFi failed. Running in AP mode.");
      for (int i = 0; i < 6; i++) {
        digitalWrite(PIN_LED_R, HIGH); delay(200);
        digitalWrite(PIN_LED_R, LOW);  delay(200);
      }
    }
  } else {
    Serial.printf("No WiFi credentials. Connect to %s, open http://192.168.4.1/setup\n",
                  apSSID);
  }

  // ── ESP-NOW ─────────────────────────────────
  // Standard 802.11 on both interfaces.
  // TODO: LR (long-range) mode validation pending — LR was found to prevent
  // ESP-NOW discovery when STA is disconnected (disconnected-STA + LR is
  // untested on ESP32-C6). Re-evaluate LR after core mesh is validated.
  esp_wifi_set_protocol(WIFI_IF_STA,
    WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_protocol(WIFI_IF_AP,
    WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAILED.");
  } else {
    esp_now_register_recv_cb(onDataReceived);
    // Register broadcast MAC so we can send beacons to all boards in range
    esp_now_peer_info_t bp = {};
    memcpy(bp.peer_addr, broadcastMac, 6);
    bp.channel = 0;
    bp.encrypt = false;
    esp_now_add_peer(&bp);
    Serial.println("ESP-NOW ready.");
  }

  // ── Channel seek (non-WiFi boards) ──────────
  if (!wifiConnected) startChannelSeek();

  // Initial gateway election (self only at startup)
  electGateway();

  // ── Web server ──────────────────────────────
  server.on("/",                     handleRoot);
  server.on("/data",                 handleData);
  server.on("/version",              handleVersion);
  server.on("/peers",                handlePeers);
  server.on("/time",      HTTP_POST, handleTimePost);
  server.on("/relay",                handleRelay);
  server.on("/relay/status",         handleRelayStatus);
  server.on("/i2c-scan",             handleI2CScan);
  server.on("/remote-scan",          handleRemoteScan);
  server.on("/settings",  HTTP_POST, handleSettingsPost);
  server.on("/softap",    HTTP_POST, handleSoftApPost);
  server.on("/setup",     HTTP_GET,  handleSetupGet);
  server.on("/setup",     HTTP_POST, handleSetupPost);
  server.on("/wifi/reset",HTTP_POST, handleWifiReset);
  server.on("/update",    HTTP_GET,  handleUpdatePage);
  server.on("/update/firmware",  HTTP_POST,
    handleUpdateFirmwareDone, handleUpdateFirmware);
  server.on("/update/filesystem", HTTP_POST,
    handleUpdateFilesystemDone, handleUpdateFilesystem);
  server.onNotFound([](){
    server.sendHeader("Location", "http://192.168.4.1/setup", true);
    server.send(302, "text/plain", "");
  });
  server.begin();

  Serial.println("Web server started.");
  Serial.println("  GET  /           → dashboard");
  Serial.println("  GET  /data       → sensor + relay + network JSON");
  Serial.println("  GET  /peers      → peer list JSON");
  Serial.println("  GET  /version    → firmware + webapp + time info");
  Serial.println("  POST /time       → set board time from browser");
  Serial.println("  POST /settings   → save board name / network name");
  Serial.println("  POST /setup      → save WiFi credentials");
  Serial.println("  POST /wifi/reset → clear credentials + reboot");
  Serial.println("  GET  /update     → OTA update UI");

  // Beacon starts in fast mode
  beaconFastStartMs = millis();
  beaconFastMode    = true;
  sendBeacon();
  beaconLastSent = millis();
}

// ─────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  customLoop();

  unsigned long now = millis();

  // ── Beacon timer (burst + settle) ───────────
  {
    bool inFast = beaconFastMode &&
                  (now - beaconFastStartMs < BEACON_FAST_DURATION_MS);
    if (!inFast && beaconFastMode) {
      beaconFastMode = false;
      Serial.println("Beacon: fast phase complete, switching to 30s steady state.");
    }
    unsigned long interval = inFast ? BEACON_FAST_MS : BEACON_SLOW_MS;
    if (now - beaconLastSent >= interval) {
      sendBeacon();
      beaconLastSent = now;
    }
  }

  // ── Peer timeout check (every 10s) ──────────
  {
    static unsigned long lastTimeoutCheck = 0;
    if (now - lastTimeoutCheck >= 10000) {
      lastTimeoutCheck = now;
      uint8_t prevCount = peerCount;
      timeoutPeers();
      electGateway();
      if (prevCount > 0 && peerCount == 0) {
        // All peers lost — re-enter fast beacon so others can find us quickly
        beaconFastMode    = true;
        beaconFastStartMs = now;
        Serial.println("All peers lost — fast beacon restarted.");
      }
      if (!wifiConnected && prevCount > 0 && peerCount == 0) {
        // Non-WiFi board lost all peers — resume channel seek
        startChannelSeek();
      }
    }
  }

  // ── Non-blocking channel seek (non-WiFi boards seeking mesh) ──
  if (!wifiConnected && !channelLocked) {
    const uint8_t seekChannels[] = {1, 6, 11, 2, 3, 4, 5, 7, 8, 9, 10};
    if (softApTempUntil > 0) {
      // Temp SoftAP window active — pause seek so AP stays on a stable channel
      seekLastAdvanceMs = now;
    } else if (now - seekLastAdvanceMs >= 3000) {
      // Advance to next channel — restart AP on new channel to move the radio
      seekChannelIdx = (seekChannelIdx + 1) % sizeof(seekChannels);
      setSeekChannel(seekChannels[seekChannelIdx]);
      seekLastAdvanceMs = now;
      Serial.printf("Channel seek: ch %u\n", meshChannel);
    }
    // Every 5 min without locking: re-trigger fast beacon
    if (now - seekStartMs >= 300000UL) {
      beaconFastMode    = true;
      beaconFastStartMs = now;
      seekStartMs       = now;
      Serial.println("Channel seek: 5-min kick — fast beacon restarted.");
    }
  }

  // ── Sensor broadcast to gateway (non-gateway boards) ───
  {
    if (!isGateway && gatewayKnown &&
        now - sensorLastBroadcast >= SENSOR_BROADCAST_MS) {
      readLocalSensors();
      sendSensorToGateway();
      sensorLastBroadcast = now;
    }
  }

  // ── NTP hourly re-sync ───────────────────────
  if (wifiConnected && now - lastNtpSync >= NTP_RESYNC_MS) {
    ntpSync();
  }

  // ── Rule engine ─────────────────────────────
  // evaluateRules();  // enabled in v1.2

  // ── Heartbeat LED ────────────────────────────
  {
    static unsigned long lastBlink = 0;
    if (now - lastBlink > 5000 && wifiConnected) {
      lastBlink = now;
      digitalWrite(PIN_LED_B, LOW); delay(60); digitalWrite(PIN_LED_B, HIGH);
    }
  }

  // ── Boot button short press → temp SoftAP ────
  {
    static bool          btnWasDown = false;
    static unsigned long btnDownMs  = 0;
    bool btnDown = (digitalRead(PIN_BOOT) == LOW);
    if (btnDown && !btnWasDown) {
      btnDownMs  = now;
      btnWasDown = true;
    } else if (!btnDown && btnWasDown) {
      unsigned long held = now - btnDownMs;
      if (held >= 50 && held < 5000) {
        softApTempUntil = now + 600000UL;  // 10 minutes
        enableSoftAP();
        Serial.println("SoftAP: 10-min field access window started.");
      }
      btnWasDown = false;
    }
  }

  // ── SoftAP temp window expiry ─────────────────
  if (softApTempUntil > 0 && now >= softApTempUntil) {
    softApTempUntil = 0;
    if (softApMode == 0 && wifiConnected) disableSoftAP();
  }
}
