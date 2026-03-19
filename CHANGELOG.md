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

### v0.7 — LittleFS + OTA
- Dashboard HTML moved from PROGMEM to LittleFS — firmware and webapp update independently
- Self-seeding: on first boot, firmware formats LittleFS and writes webapp files from PROGMEM — no external flash tool needed
- Added `/update` OTA browser UI (served from PROGMEM, always available even if LittleFS fails)
- Added `/update/firmware` endpoint — accepts compiled sketch `.bin`
- Added `/update/filesystem` endpoint — accepts LittleFS image `.bin`
- Added `/version` endpoint returning firmware version, webapp version, and LittleFS status
- Added `data/` folder alongside sketch containing `index.html` and `version.txt`
- Added explicit `PIN_SDA=6`, `PIN_SCL=7`, `PIN_LED_B=23`, `PIN_LED_R=22` defines — board-package-agnostic
- OTA success message shows countdown and auto-redirects to dashboard
- Requires `partitions.csv` (included in sketch folder)
- Arduino IDE: Tools → USB CDC On Boot → Enabled (required for serial output)
- Arduino IDE: Tools → Partition Scheme → Custom (required for OTA + LittleFS partition layout)

### v0.6.1 — TCA9534 Address Fix
- Fixed TCA9534 I2C address from 0x20 to 0x27
- SparkFun Qwiic GPIO board has all address jumpers bridged by default (A0=1, A1=1, A2=1)
- Discovered via I2C scanner diagnostic

### v0.6 — Relay Control
- Added relay control via SparkFun Qwiic GPIO (TCA9534) at I2C 0x20
- Relay defaults OFF at boot, sensor failure, and connectivity loss — always
- Added `/relay` and `/relay/status` HTTP endpoints
- Added collapsible Relay Control section to dashboard sidebar
- Manual ON/OFF toggle always overrides automation rules
- Rule engine data structures (`Rule`, `Condition`, `evaluateRules`) stubbed for Stage 2
- Fixed: `setPin()` → `digitalWrite()` (correct SparkFun TCA9534 API)
- Fixed: `OUTPUT` → `GPIO_OUT` (SparkFun TCA9534 constant)
- Fixed: `Condition`/`Rule` structs moved before functions that reference them (Arduino compiler forward declaration)

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

### v0.4 — WiFi Fix + Pin Defines
- Aligned with Host ESP-NOW LR setup order fix
- Added explicit `PIN_SDA=6`, `PIN_SCL=7`, `PIN_LED_B=23`, `PIN_LED_R=22` defines — board-package-agnostic

### v0.3 — Callback Fix (matches Host v0.3)
- Fixed send callback signature for ESP-IDF v5.5+

### v0.2 — Initial Remote Sketch
- Reads SHTC3 + TSL2591
- Sends `SensorPayload` to Host MAC every 3 seconds via ESP-NOW LR
