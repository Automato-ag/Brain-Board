# Changelog

All notable changes to Brain Board firmware are documented here.

---

## Host Firmware

### v0.8.1 — 2026-03-20
- Added `GET /i2c-scan` endpoint — scans I2C bus (SDA IO6, SCL IO7), returns JSON array of found addresses
- Added tab navigation shell to dashboard webapp — fixed top bar with 5 tabs
- New tabs: Dashboard | I2C Scanner | Devices | Rules | Settings
- Added I2C Scanner tab — manual scan button, address map grid (0x08–0x77), 125+ device database with identification
- Sidebar (Relay + Agri Data) now only visible on Dashboard tab
- Connection status pill moved into tab nav bar
- TSL2591 (0x29) and SHTC3 (0x70) labeled as Automato Onboard
- TCA9534 (0x27) correctly labeled as external Qwiic board — not onboard Brain Board PCB
- DS3231 (0x68) pre-labeled in database as reserved for next Brain Board revision
- Devices, Rules, and Settings tabs present as placeholder shells
- Firmware version bumped to 0.8.1

### v0.8.0 — WiFi Provisioning + mDNS
- **No more hardcoded credentials** — WiFi SSID and password entered via captive portal, stored in NVS
- **AP+STA mode** — Board 1 always runs SoftAP (`Automato-XXXX`) alongside WiFi connection; setup page always reachable at `192.168.4.1`
- **Captive portal** — phones and laptops auto-open setup page when connecting to AP
- **mDNS** — board reachable at `boardname.local` (or `automato-XXXX.local` if no name set)
- **Custom board name** — set during provisioning; used as AP SSID and mDNS hostname; validated (letters, numbers, spaces, hyphens only)
- **Boot button credential reset** — hold IO9 for 5 seconds at startup to clear stored credentials
- **Setup UI** — show/hide password toggle; countdown redirect after save; inline validation warning
- **Captive portal redirect** — `onNotFound` redirects any unknown URL to `/setup`
- **LR protocol fix** — `WIFI_PROTOCOL_LR` moved to STA interface only; AP stays on standard 802.11b/g/n

### v0.7.0 — LittleFS + OTA
- Dashboard HTML moved from PROGMEM to LittleFS — firmware and webapp update independently
- Self-seeding first boot — firmware writes its own LittleFS if not present; no external flash tool needed
- `/update` OTA browser UI served from PROGMEM (always available even if LittleFS fails)
- `/update/firmware` POST endpoint — flashes new `.bin` over the air
- `/update/filesystem` POST endpoint — flashes new LittleFS image over the air
- `/version` endpoint returns current firmware/webapp version string
- Custom partition table: dual OTA slots + LittleFS

### v0.6.1 — TCA9534 Address Fix
- Fixed TCA9534 I2C address from 0x20 to 0x27
- SparkFun Qwiic GPIO board has all address jumpers bridged by default (A0=1, A1=1, A2=1)
- Discovered via I2C scanner diagnostic

### v0.6 — Relay Control
- Added relay control via SparkFun Qwiic GPIO (TCA9534)
- Relay defaults OFF at boot, sensor failure, and connectivity loss — always
- Added `/relay` and `/relay/status` HTTP endpoints
- Added collapsible Relay Control section to dashboard sidebar
- Manual ON/OFF toggle always overrides automation rules
- Rule engine data structures (`Rule`, `Condition`, `evaluateRules`) stubbed for v0.9

### v0.5 — Agri Data Sidebar
- Added Agri Data sidebar with location search (city, zip, lat/lon)
- Metric/Imperial units toggle
- Parameter dropdown grouped by category with source labels
- Active parameters displayed as chips with live values
- External data refreshes every 60 seconds
- All API calls browser-side — board code unchanged
- Sources: Open-Meteo (weather, soil, solar, UV, ET0, VPD), Sunrise-Sunset.org (sun/moon), Open-Meteo AQI (PM2.5, pollen)

### v0.4 — WiFi Fix
- Fixed WiFi connection failure caused by LR protocol being set on STA interface before connecting
- LR now enabled on STA interface only, after WiFi connects

### v0.3 — ESP-NOW Callback Fix
- Fixed ESP-NOW send callback signature for ESP-IDF v5.5+
- wifi_tx_info_t* replaces uint8_t* mac in send callback

### v0.2 — Two-Board Support
- Added ESP-NOW LR two-board support
- Split into Host and Remote sketches
- Shared SensorPayload struct

### v0.1 — Initial Release
- Single-board sensor web dashboard
- SHTC3 temperature/humidity
- TSL2591 ambient light
- Dark theme dashboard with live updates

---

## Remote Firmware

### v0.5 — Channel Scan
- **Automatic channel detection** — scans channels 1-13 at startup, sends ping on each, locks to whichever channel Host ACKs on; no more hardcoded channel
- **Re-scan on timeout** — if no ACK received for 30 seconds, re-scans all channels automatically
- **LR protocol fix** — WIFI_PROTOCOL_LR moved to STA interface only (matches Host v0.8 fix)
- Serial output includes locked channel: Sending > Temp: 24.1C  Hum: 45.0%  Lux: 88.3  ch:6

### v0.4 — WiFi Fix
- Aligned with Host ESP-NOW LR setup order fix

### v0.3 — Callback Fix
- Fixed send callback signature for ESP-IDF v5.5+

### v0.2 — Initial Remote Sketch
- Reads SHTC3 + TSL2591
- Sends SensorPayload to Host MAC every 3 seconds via ESP-NOW LR

---

## Tools

### I2C_Scanner — v1.0
- Generic I2C bus scanner for Brain Board V2.0
- Scans all 127 possible addresses
- Labels known onboard devices (SHTC3, TSL2591) and all 8 possible TCA9534 addresses
- Any unknown device reported with its address
- Brain Board specific: Wire.begin(6, 7) for SDA=IO6, SCL=IO7
