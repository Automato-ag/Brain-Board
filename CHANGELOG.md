# Changelog

All notable changes to Brain Board firmware are documented here.

---

## Host Firmware

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

### v0.4 — WiFi Fix (matches Host v0.4)
- Aligned with Host ESP-NOW LR setup order fix

### v0.3 — Callback Fix (matches Host v0.3)
- Fixed send callback signature for ESP-IDF v5.5+

### v0.2 — Initial Remote Sketch
- Reads SHTC3 + TSL2591
- Sends `SensorPayload` to Host MAC every 3 seconds via ESP-NOW LR
