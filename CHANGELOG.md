# Changelog

All notable changes to Brain Board firmware are documented here.

---

## v1.1 — 2026-04-05

Two-board hardware test confirmed 2026-03-28. All items below shipped and tested.

### Firmware (`BrainBoard_v110.ino`)

- **Unified firmware** — `BrainBoard_Host` and `BrainBoard_Remote` merged into single `BrainBoard` firmware; all boards are identical peers; role (gateway / relay / isolated) determined dynamically at runtime
- **MSG_HELLO beacon** — boards broadcast Network Name + board name + MAC + hasWiFi on startup and periodically; matching boards auto-register as ESP-NOW peers without manual MAC entry
- **Dynamic gateway election** — board with active WiFi router connection becomes gateway; strongest RSSI wins if multiple candidates; transfers automatically when boards move
- **`networkname.local` mDNS** — gateway advertises Automato Network Name as mDNS hostname; user bookmark works regardless of which physical board is currently gateway
- **1-hop relay** — non-WiFi boards seek mesh channel via channel scan, lock on first matching MSG_HELLO, relay sensor data to gateway via ESP-NOW
- **NTP time sync** — gateway syncs time on WiFi connect; `POST /time` endpoint allows browser to sync board time
- **Plugin hook architecture** — `customSetup()`, `customLoop()`, `customDataJSON()` weak functions; user adds `custom.ino` to sketch folder without modifying base firmware
- **`/proxy` endpoint** — board-side HTTPS relay for external API calls; 7-domain allowlist (Open-Meteo forecast + ERA5 archive, Geocoding, AQI, Sunrise-Sunset, USNO, NWS); enables agri data when browser has no direct internet (SoftAP field access)
- **`/peers` endpoint** — returns full peer list as JSON including board names, MACs, roles, RSSI, and last-seen timestamps
- **`/prefs` GET/POST** — LittleFS-backed dashboard preference storage; dual-write to NVS as backup (survives LittleFS uploads); GET auto-restores from NVS if file missing
- **SoftAP captive portal fix** — `onNotFound` redirects to `/` (dashboard) when provisioned; `/setup` when not yet provisioned; phone connecting to board SoftAP in the field auto-opens dashboard
- **SoftAP password** — `WiFi.softAP()` called with Network Password; SoftAP is secured when Network Password is set

### Webapp (`data/index.html`)

**Dashboard**
- Board role indicator (Gateway / Relay / Isolated) in tab nav bar
- Board 2 column populated from `/peers` response — board name, sensor readings, link status, last-seen timestamp
- Card visibility per board — toggle individual sensor cards on/off; persisted to `/prefs`

**Settings tab redesign** — three-level navigation:
- Display & Behavior: units toggle (metric/imperial), refresh rate selector (2.5/5/10/30s), auto-sync time on connect, font scale controls, tooltip toggle
- Agri Data Sources: per-source and per-parameter enable/disable with bulk checkboxes
- Board Settings: master-detail layout; local board editable; peer board read-only; DLI crop preset selector and GDD/chill season settings per board

**Network tab** — live peer map; board roles, relay paths, gateway address, WiFi status

**Agri Data sidebar — all phases:**
- Phase 1: weather (temp, precipitation, wind, cloud cover, humidity)
- Phase 2: solar (UV, shortwave, sunrise/sunset/day length, first/last light, solar noon)
- Phase 3: soil (temperature 0–7cm / 7–28cm, moisture, ET₀, VPD)
- Phase 4: moon phase, moonrise/moonset (USNO + Sunrise-Sunset.org); NWS frost/freeze alerts
- Phase 5a: Frost Risk (derived — radiative cooling model from temp + cloud cover + wind); Photoperiod (derived — day length classification + seasonal trend)
- Phase 5b: Growing Degree Days (season-to-date since Jan 1, base 10°C / 50°F, ERA5 archive + recent forecast bridge); Chill Hours (season-to-date since Nov 1 / May 1, asymmetric linear temperature model ±5%)
- DLI Accumulator — accumulated daily light integral from TSL2591 lux sensor; 30+ crop presets with DLI target ranges; user-defined custom presets; progress shown as percentage of daily target

**Preferences persistence (`/prefs`)**
- Board is source of truth; localStorage is write-through cache and offline fallback
- Persisted: location, active agri params, units, refresh rate, auto-sync, font scales, tooltip toggle, per-source enabled state, card visibility per board, DLI crop preset/target/user presets, GDD base temperature, season start date
- `proxyFetch()` — tries `/proxy` first; falls back to direct browser fetch if board has no internet (desktop use case)
- `externalFetchDone` flag — chips show `—` instead of `Loading…` when all fetches complete with no data

---

## Host Firmware

### v0.8.1 — Hardware
- Added KiCad 9.0 design files for Brain Board V2.0 (schematic, PCB layout, project file)
- Added `hardware/README.md` with key specs, I2C address table, and architecture note
- Fixed TCA9534 I2C address in `Brain_Board_Reference.md` — corrected from 0x20 to 0x27

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
