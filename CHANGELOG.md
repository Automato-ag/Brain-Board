# Changelog

All notable changes to Brain Board firmware are documented here.

---

## Tools

### I2C_Scanner — v1.0
- Generic I2C bus scanner for Brain Board V2.0
- Scans all 127 possible addresses
- Labels all known onboard devices (SHTC3, TSL2591) and all 8 possible TCA9534 addresses
- Any unknown device reported with its address
- Brain Board specific: Wire.begin(6, 7) for SDA=IO6, SCL=IO7

---

## Host Firmware

### v0.8.0 — WiFi Provisioning + mDNS
- **No more hardcoded credentials** — WiFi SSID and password are entered via captive portal and stored in NVS (survive reboots and OTA updates)
- **AP+STA mode** — Board 1 always runs a SoftAP (`Automato-XXXX`) alongside its WiFi connection, so the setup page is always reachable at `192.168.4.1`
- **Captive portal** — phones and laptops auto-open the setup page when connecting to the AP
- **mDNS** — board is reachable at `boardname.local` (or `automato-XXXX.local` if no name is set)
- **Custom board name** — set during provisioning; used as AP SSID suffix and mDNS hostname; validated (letters, numbers, spaces, hyphens only)
- **Boot button credential reset** — hold IO9 for 5 seconds at startup to clear stored credentials and re-enter provisioning mode
- **Setup UI** — show/hide password toggle; countdown redirect after save; inline validation warning
- **Captive portal redirect** — `onNotFound` handler redirects any unknown URL to `/setup`
- **LR protocol fix** — `WIFI_PROTOCOL_LR` moved to STA interface only; AP stays on standard 802.11b/g/n so phones and laptops can see the AP

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
- Sources: Open-Meteo (weather, soil, solar, UV, ET₀, VPD), Sunrise-Sunset.org (sun/moon), Open-Meteo AQI (PM2.5, pollen)

### v0.4 — WiFi Fix
- Fixed WiFi connection failure caused by LR protocol being set on STA interface before connecting
- LR now enabled on AP interface only, after WiFi connects

### v0.3 — ESP-NOW Callback Fix
- Fixed ESP-NOW send callback signature for ESP-IDF v5.5+
- `wifi_tx_info_t*` replaces `uint8_t* mac` in send callback

### v0.2 — Two-Board Support
- Added ESP-NOW LR two-board support
- Split into Host and Remote sketches
- Shared `SensorPayload` struct

### v0.1 — Initial Release
- Single-board sensor web dashboard
- SHTC3 temperature/humidity
- TSL2591 ambient light
- Dark theme dashboard with live updates

---

## Remote Firmware

### v0.5 — Channel Scan
- **Automatic channel detection** — scans channels 1–13 at startup, sends a ping on each, locks to whichever channel Board 1 ACKs on; no more hardcoded channel
- **Re-scan on timeout** — if no ACK received for 30 seconds, re-scans all channels automatically
- **LR protocol fix** — `WIFI_PROTOCOL_LR` moved to STA interface only (matches Host v0.8 fix)
- Serial output includes locked channel: `Sending → Temp: 24.1°C  Hum: 45.0%  Lux: 88.3  ch:6`

### v0.4 — WiFi Fix (matches Host v0.4)
- Aligned with Host ESP-NOW LR setup order fix

### v0.3 — Callback Fix (matches Host v0.3)
- Fixed send callback signature for ESP-IDF v5.5+

### v0.2 — Initial Remote Sketch
- Reads SHTC3 + TSL2591
- Sends `SensorPayload` to Host MAC every 3 seconds via ESP-NOW LR
