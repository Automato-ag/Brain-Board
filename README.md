# Brain Board — Automato Ag

Firmware and documentation for the **Automato Brain Board V2.0**, an ESP32-C6 based agricultural IoT node with onboard temperature, humidity, and light sensing, long-range ESP-NOW wireless, relay control, and a self-hosted web dashboard served directly from the board.

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
| Boot Button | IO9 — hold 5s at startup to clear WiFi credentials |
| Qwiic/STEMMA QT | J2, J3 — GND / 3V3 / SDA / SCL |
| Logic Level | 3.3V only — GPIO pins not 5V tolerant |

See [`hardware/Brain_Board_Reference.md`](hardware/Brain_Board_Reference.md) for full pinout and hardware notes.

---

## Firmware

### Current Versions

| Sketch | Version | Role |
|---|---|---|
| [`BrainBoard_Host_v081`](firmware/BrainBoard_Host/BrainBoard_Host_v081/) | **v0.8.1** | WiFi + WebServer + ESP-NOW receiver + Relay + I2C Scanner |
| [`BrainBoard_Remote_v05`](firmware/BrainBoard_Remote/BrainBoard_Remote_v05/) | **v0.5** | ESP-NOW transmitter — sensor data + auto channel scan |

### Required Libraries

Install all of the following via **Arduino Library Manager**:

- `Adafruit SHTC3 Library`
- `Adafruit TSL2591`
- `Adafruit BusIO` *(installed automatically as dependency)*
- `Adafruit Unified Sensor` *(installed automatically as dependency)*
- `SparkFun TCA9534` *(Host only — for Qwiic GPIO relay control)*

### Arduino IDE Settings

| Setting | Value |
|---|---|
| Board | ESP32C6 Dev Module |
| USB CDC On Boot | **Enabled** — required, board crashes on Serial without this |
| Partition Scheme | **Custom** — required, uses `partitions.csv` in sketch folder |
| Upload Speed | 921600 |

> **Note:** USB CDC On Boot must be re-enabled each Arduino IDE session — it does not always persist between sessions.

### First Flash — Host

1. Open `BrainBoard_Host_v081/BrainBoard_Host_v081.ino` in Arduino IDE
2. Set board settings as above
3. **Sketch → Upload** — flash firmware
4. **Tools → ESP32 Sketch Data Upload** — upload `data/` folder to LittleFS
5. Open Serial Monitor at 115200 baud, press RESET
6. Connect phone or laptop to the `Automato-XXXX` WiFi network
7. Open `http://192.168.4.1/setup` — enter your WiFi credentials
8. Board joins your network and is accessible at `http://automato-XXXX.local`

### First Flash — Remote

1. Open `BrainBoard_Remote_v05/BrainBoard_Remote_v05.ino`
2. Set board settings as above (no LittleFS upload needed)
3. Flash to Board 2 — it will auto-scan channels and find the Host automatically

### Reset WiFi Credentials

- Hold **Boot button (IO9)** for 5 seconds at startup, **or**
- Use the **Reset WiFi** button in the dashboard

### OTA Updates (after first flash)

- Navigate to `http://boardname.local/update` or `http://192.168.4.1/update`
- Upload new firmware `.bin` or new LittleFS filesystem image
- Firmware and webapp update independently

---

## Dashboard

The Brain Board serves a complete webapp directly from its own flash memory — no internet or cloud required. Works in a field with no cell service.

### Tabs

| Tab | Description | Status |
|---|---|---|
| Dashboard | Live sensor readings, relay control, Agri Data sidebar | Live |
| I2C Scanner | Scan I2C bus, identify connected devices from 125+ device database | Live |
| Devices | Board topology and attached hardware — all connected Automato boards | Planned v0.9+ |
| Rules | Tier 3 offline rule engine — NVS-stored automation rules | Planned v0.9 |
| Settings | Network, board name, and system settings | Planned |

### HTTP Endpoints

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | Dashboard webapp (served from LittleFS) |
| `/data` | GET | Sensor JSON + relay state |
| `/version` | GET | Firmware + webapp version |
| `/relay` | GET | Relay control (`?state=0\|1`, `?override=auto`) |
| `/relay/status` | GET | Relay state JSON |
| `/i2c-scan` | GET | I2C bus scan — returns `{"devices":[...]}` |
| `/setup` | GET/POST | WiFi provisioning form |
| `/wifi/reset` | POST | Clear credentials + reboot |
| `/update` | GET | OTA update UI |
| `/update/firmware` | POST | Flash new firmware `.bin` |
| `/update/filesystem` | POST | Flash new LittleFS image |

### Agri Data Sidebar

All external API calls are browser-side — the board serves only the dashboard HTML and `/data` endpoint.

| Parameter Group | Source |
|---|---|
| Weather, Wind, Cloud Cover | Open-Meteo |
| Solar, UV Index, Shortwave Radiation | Open-Meteo |
| First Light, Sunrise, Solar Noon, Sunset, Day Length | Sunrise-Sunset.org |
| Soil Temperature and Moisture, ET0, VPD | Open-Meteo |
| Moon Phase, Moonrise, Moonset | Sunrise-Sunset.org |
| PM2.5, Pollen | Open-Meteo Air Quality |

### Relay Control

- GPIO expander: SparkFun Qwiic GPIO (TCA9534) at I2C address 0x27, connected via Qwiic cable
- Relay defaults **OFF** at all times — boot, sensor failure, I2C error, connectivity loss
- Manual ON/OFF toggle in dashboard always overrides automation
- Rule engine data structures stubbed and ready for v0.9

### ESP-NOW Long Range

- LR protocol enabled on STA interface after WiFi connects
- Remote board auto-scans channels 1–13 at startup — no hardcoded channel required
- Board 2 transmits sensor payload every 3 seconds
- Stale detection at 15 seconds

---

## Relay Safety Contract

Relays default **OFF** under all failure conditions: boot, sensor failure, WiFi loss, cloud loss, browser closed, no rules configured, conflicting rules, hardware expander not found.

**There is no condition under which a relay defaults ON.**

---

## Roadmap

| Version | Feature | Status |
|---|---|---|
| v0.8.1 | I2C Scanner tab + tab navigation shell | Complete |
| v0.9 | Tier 3 offline rule engine — NVS rules, rule builder UI | Next |
| v1.0 | Base firmware — stable, shippable, pre-built `.bin` | Target |

See [`docs/Roadmap.md`](docs/Roadmap.md) for full architecture, three-tier automation design, platform roadmap, and multi-board networking plans.

---

## Repository Structure

```
Brain-Board/
├── README.md
├── CHANGELOG.md
├── LICENSE
├── .gitignore
├── firmware/
│   ├── BrainBoard_Host/
│   │   ├── BrainBoard_Host_v081/        <- current
│   │   │   ├── BrainBoard_Host_v081.ino
│   │   │   ├── partitions.csv
│   │   │   └── data/
│   │   │       ├── index.html
│   │   │       └── version.txt
│   │   ├── BrainBoard_Host_v08/
│   │   │   ├── BrainBoard_Host_v08.ino
│   │   │   ├── partitions.csv
│   │   │   └── data/
│   │   │       ├── index.html
│   │   │       └── version.txt
│   │   ├── BrainBoard_Host_v0.6.1.ino
│   │   └── BrainBoard_Host_v0.6.ino
│   └── BrainBoard_Remote/
│       ├── BrainBoard_Remote_v05/       <- current
│       │   └── BrainBoard_Remote_v05.ino
│       └── BrainBoard_Remote_v0.4.ino
├── docs/
│   ├── Roadmap.md
│   ├── QuickStart.md
│   ├── RelayControl.md
│   ├── AgriDataSidebar.md
│   ├── ESP32C6_Capabilities.md
│   ├── I2C_Address_Map.md
│   └── ProductFamily.md
├── hardware/
│   └── Brain_Board_Reference.md
└── tools/
    └── I2C_Scanner/
        └── I2C_Scanner.ino
```

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

*Automato Ag — [automato.ag](https://automato.ag)*
