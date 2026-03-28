# Brain Board — Automato Ag

Firmware and documentation for the **Automato Brain Board V2.0**, an ESP32-C6 based agricultural IoT node with onboard temperature, humidity, and light sensing, ESP-NOW mesh networking, relay control, and a self-hosted web dashboard served directly from the board.

---

## Hardware

| Component | Details |
|---|---|
| MCU | ESP32-C6-MINI-1-N4, 160 MHz, 4 MB Flash |
| Temp/Humidity | SHTC3 — I2C 0x70 (fixed) |
| Light | TSL2591 — I2C 0x29 (fixed) |
| I2C Bus | SDA = IO6, SCL = IO7 |
| Blue LED | IO23 |
| Red LED | IO22 |
| Boot Button | IO9 — see Boot Button section below |
| Qwiic/STEMMA QT | J2, J3 — GND / 3V3 / SDA / SCL |
| Logic Level | 3.3V only — GPIO pins not 5V tolerant |

See [`hardware/Brain_Board_Reference.md`](hardware/Brain_Board_Reference.md) for full pinout and hardware notes.

---

## Firmware

### Current Version

| Sketch | Version |
|---|---|
| [`BrainBoard_v110`](firmware/BrainBoard/BrainBoard_v110/) | **v1.1.0** |

All boards run the same unified firmware. There is no separate Host or Remote build — role (gateway, relay, isolated) is determined automatically at runtime based on WiFi connectivity.

### Required Libraries

Install all of the following via **Arduino Library Manager**:

- `Adafruit SHTC3 Library`
- `Adafruit TSL2591`
- `Adafruit BusIO` *(installed automatically as dependency)*
- `Adafruit Unified Sensor` *(installed automatically as dependency)*
- `SparkFun TCA9534` *(for Qwiic GPIO relay control)*

### Arduino IDE Settings

| Setting | Value |
|---|---|
| Board | ESP32C6 Dev Module |
| USB CDC On Boot | **Enabled** — required; board crashes on Serial without this |
| Partition Scheme | **Custom** — required; uses `partitions.csv` in sketch folder |
| Upload Speed | 921600 |

> **Note:** USB CDC on Boot must be re-enabled each Arduino IDE session — it does not always persist.

### First Flash

1. Open `BrainBoard_v110/BrainBoard_v110.ino` in Arduino IDE
2. Set board settings as above
3. **Sketch → Upload** — flash firmware
4. **Tools → ESP32 Sketch Data Upload** — upload `data/` folder to LittleFS
5. Open Serial Monitor at 115200 baud, press RESET
6. Connect phone or laptop to the `Automato-XXXX` WiFi network
7. Open `http://192.168.4.1/setup` — enter your Network Name, password, and WiFi credentials
8. Board joins your network and is accessible at `http://networkname.local`

### OTA Updates (after first flash)

- Navigate to `http://boardname.local/update` or `http://192.168.4.1/update`
- Upload new firmware `.bin` or new LittleFS filesystem image
- Firmware and webapp update independently — UI changes do not require reflashing firmware

### Boot Button (IO9)

| Action | Behavior |
|---|---|
| Hold 5s at **startup** | Clears all stored credentials and settings, then reboots |
| Short press during **runtime** | Opens a 10-minute SoftAP access window — board stays accessible at `192.168.4.1` even when on WiFi (useful for field configuration) |

---

## Dashboard

The Brain Board serves a complete webapp directly from its own flash memory — no internet, cloud, or external server required. Works in a field with no cell service.

### Tabs

| Tab | Description | Status |
|---|---|---|
| Dashboard | Live sensor readings, relay control, Agri Data sidebar | Live |
| Devices | Local + peer I2C scan, user-defined device labels, address reference | Live |
| Rules | Automation rule engine — local and cross-board conditions | v1.2 |
| Settings | Display & Behavior (units, refresh rate, auto-sync), Agri Data Sources, Board Settings | Live |
| Network | Mesh topology — board roles, peers, gateway address | Live |

### HTTP Endpoints

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | Dashboard webapp (served from LittleFS) |
| `/data` | GET | Sensor JSON + relay state + network info |
| `/peers` | GET | Peer list JSON |
| `/version` | GET | Firmware + webapp version, mDNS name, network name |
| `/relay` | GET | Relay control (`?state=0\|1`, `?override=auto`) |
| `/relay/status` | GET | Relay state JSON |
| `/i2c-scan` | GET | Local I2C bus scan |
| `/remote-scan` | POST/GET | Peer I2C scan via ESP-NOW |
| `/time` | POST | Set board time from browser |
| `/settings` | POST | Save board name / Network Name / password |
| `/setup` | GET/POST | WiFi provisioning form |
| `/wifi/reset` | POST | Clear WiFi credentials + reboot |
| `/update` | GET | OTA update UI |
| `/update/firmware` | POST | Flash new firmware `.bin` |
| `/update/filesystem` | POST | Flash new LittleFS image |

### Agri Data Sidebar

All external API calls are browser-side — the board serves only the dashboard HTML and `/data` endpoint. No data leaves the board.

Data sources can be enabled or disabled individually from **Settings → Agri Data Sources**.

| Parameter Group | Source |
|---|---|
| Weather, Wind, Cloud Cover | Open-Meteo |
| Solar, UV Index, Shortwave Radiation | Open-Meteo |
| First Light, Sunrise, Solar Noon, Sunset, Day Length | Sunrise-Sunset.org |
| Soil Temperature and Moisture, ET0, VPD | Open-Meteo |
| Moon Phase, Moonrise, Moonset | Sunrise-Sunset.org |
| PM2.5, Pollen | Open-Meteo Air Quality |

---

## ESP-NOW Mesh

All Brain Boards on the same Network Name automatically form a mesh using ESP-NOW. No configuration required beyond giving boards the same Network Name in `/setup`.

| Role | Condition | Behavior |
|---|---|---|
| Gateway | Board has active WiFi | Serves dashboard at `networkname.local`; syncs NTP time; relays peer sensor data |
| Relay | No WiFi, peer discovered | Seeks mesh channel, locks on discovery, forwards sensor data to gateway via ESP-NOW |
| Isolated | No WiFi, no peers found | Seeks continuously; accessible at `192.168.4.1` via its own SoftAP |

**Channel seek:** Non-WiFi boards cycle through channels 1–11 (US), restarting their SoftAP on each channel to keep the radio active for ESP-NOW reception. Lock occurs on first matching MSG_HELLO beacon.

**Plugin hooks:** Add a `custom.ino` file to the sketch folder to extend functionality without modifying core firmware. Available hooks: `customSetup()`, `customLoop()`, `customDataJSON()`.

---

## Relay Control

- GPIO expander: SparkFun Qwiic GPIO (TCA9534) at I2C address 0x27, connected via Qwiic cable
- Relay defaults **OFF** at all times — boot, sensor failure, I2C error, connectivity loss
- Manual ON/OFF toggle in dashboard always overrides automation
- Rule engine arriving in v1.2

## Relay Safety Contract

Relays default **OFF** under all failure conditions: boot, sensor failure, WiFi loss, cloud loss, browser closed, no rules configured, conflicting rules, hardware expander not found.

**There is no condition under which a relay defaults ON.**

---

## Roadmap

| Version | Feature | Status |
|---|---|---|
| v0.8.1 | Tab navigation shell, I2C Scanner tab, `/i2c-scan` endpoint | Complete |
| v0.9 | Devices tab, remote I2C scan via ESP-NOW, Network Name in `/setup` | Complete |
| v1.0 | Settings tab, Network tab stub, `POST /settings` endpoint | Complete |
| v1.1 | ESP-NOW mesh — unified firmware, MSG_HELLO beacon, dynamic gateway, `networkname.local`, 1-hop relay, live Network tab, plugin hooks | Complete |
| v1.2 | Rules tab — firmware rule engine, LittleFS persistence, local + peer conditions, recipe database | Next |
| v1.3 | Multi-hop relay, routing tables, cross-board rules | Planned |

---

## Repository Structure

```
Brain-Board/
├── README.md
├── CHANGELOG.md
├── LICENSE
├── .gitignore
├── firmware/
│   ├── BrainBoard/
│   │   └── BrainBoard_v110/            <- current (v1.1)
│   │       ├── BrainBoard_v110.ino
│   │       ├── partitions.csv
│   │       └── data/
│   │           ├── index.html
│   │           └── version.txt
│   ├── BrainBoard_Host/                <- legacy (pre-v1.1)
│   └── BrainBoard_Remote/              <- legacy (pre-v1.1, retired)
├── docs/
│   ├── Roadmap.md
│   ├── QuickStart.md
│   ├── RelayControl.md
│   ├── AgriDataSidebar.md
│   ├── ESP32C6_Capabilities.md
│   ├── I2C_Address_Map.md
│   └── ProductFamily.md
├── hardware/
│   ├── README.md
│   ├── Brain_Board_Reference.md
│   └── kicad/
│       └── Brain_Board_V2.0/
└── tools/
    └── I2C_Scanner/
        └── I2C_Scanner.ino
```

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

*Automato Ag — [automato.ag](https://automato.ag)*
