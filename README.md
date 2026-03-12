# Brain Board — Automato Ag

Firmware and documentation for the **Automato Brain Board V2.0**, an ESP32-C6 based agricultural IoT node with onboard temperature, humidity, and light sensing, long-range ESP-NOW wireless, and relay control.

---

## Hardware

| Component | Details |
|---|---|
| MCU | ESP32-C6-MINI-1-N4, 160 MHz, 4 MB Flash |
| Temp/Humidity | SHTC3 — I2C 0x70 |
| Light | TSL2591 — I2C 0x29 |
| I2C Bus | SDA = IO6, SCL = IO7 |
| Blue LED | IO23 (LED_BUILTIN) |
| Red LED | IO22 |
| Boot Button | IO9 |
| Qwiic/STEMMA QT | J2, J3 — GND / 3V3 / SDA / SCL |
| Logic Level | 3.3V only — GPIO pins not 5V tolerant |

See [`hardware/Brain_Board_Reference.md`](hardware/Brain_Board_Reference.md) for full pinout and hardware notes.

---

## Firmware

### Current Versions

| Sketch | Version | Role |
|---|---|---|
| [`BrainBoard_Host`](firmware/BrainBoard_Host/) | v0.6 | WiFi + WebServer + ESP-NOW receiver + Relay control |
| [`BrainBoard_Remote`](firmware/BrainBoard_Remote/) | v0.4 | ESP-NOW transmitter — sensor data only |

### Required Libraries

Install all of the following via **Arduino Library Manager**:

- `Adafruit SHTC3 Library`
- `Adafruit TSL2591`
- `Adafruit BusIO` *(installed automatically as dependency)*
- `Adafruit Unified Sensor` *(installed automatically as dependency)*
- `SparkFun TCA9534` *(Host only — for Qwiic GPIO relay control)*

### Board Package

Install the **Automato ESP32 board package** in Arduino IDE:

> Boards Manager URL: *(add your Automato board package URL here)*

Target board: **Automato Brain Board V2.0**

### Quick Setup

1. Flash `BrainBoard_Host_v0.6.ino` to **Board 1**
2. Open Serial Monitor at 115200 baud and press RESET
3. Note the MAC address printed at startup
4. Paste that MAC into `BrainBoard_Remote_v0.4.ino` as `HOST_MAC_ADDRESS`
5. Flash `BrainBoard_Remote_v0.4.ino` to **Board 2**
6. Open `http://<Board1_IP>` in your browser

---

## Features

### Dashboard
- Live sensor readings — temperature, humidity, ambient light
- Two-board display with ESP-NOW link status
- Metric / Imperial toggle
- Dark theme, mobile-friendly

### Agri Data Sidebar
All external API calls are browser-side — the board serves only the dashboard HTML and `/data` endpoint.

| Parameter Group | Source |
|---|---|
| Weather & Forecast | Open-Meteo |
| Solar, UV, Radiation | Open-Meteo |
| First Light / Civil Dawn, Sunset, Day Length | Sunrise-Sunset.org |
| Soil Temperature & Moisture, ET₀, VPD | Open-Meteo |
| Moon Phase, Moonrise/Moonset | Sunrise-Sunset.org |
| PM2.5, Pollen | Open-Meteo Air Quality |

### Relay Control
- SparkFun Qwiic GPIO (TCA9534) at I2C address 0x20
- Relay defaults **OFF** at all times — boot, sensor failure, connectivity loss
- Manual ON/OFF toggle in dashboard always overrides automation
- Rule engine data structures stubbed and ready for Stage 2

### ESP-NOW Long Range
- LR protocol enabled on AP interface after WiFi connects
- Board 2 transmits sensor payload every 3 seconds
- Stale detection at 15 seconds

---

## Roadmap

| Stage | Feature | Status |
|---|---|---|
| 1 | Manual relay control via Qwiic GPIO | ✅ Complete |
| 2 | Tier 3 offline rule engine (NVS storage) | 🔜 Next |
| 3 | Tier 2 browser-based rule builder UI | Planned |
| 4 | Tier 1 cloud relay via automato.ag | Planned |
| — | mDNS (`brainboard.local`) | Planned |
| — | automato.ag data relay + remote access | Planned |
| — | ESP32-C6 unexplored capabilities (LP core, BLE, TWT, Zigbee, pulse counter) | [See exploration notes](docs/ESP32C6_Capabilities.md) |

---

## Repository Structure

```
Brain-Board/
├── README.md
├── CHANGELOG.md
├── .gitignore
├── firmware/
│   ├── BrainBoard_Host/
│   │   └── BrainBoard_Host_v0.6.ino
│   └── BrainBoard_Remote/
│       └── BrainBoard_Remote_v0.4.ino
├── hardware/
│   └── Brain_Board_Reference.md
└── docs/
    ├── QuickStart.md
    ├── RelayControl.md
    └── AgriDataSidebar.md
```

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

*Automato Ag — [automato.ag](https://automato.ag)*
