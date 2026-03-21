# Automato Brain Board V2.0 — Hardware Files

This folder contains hardware design files for the Automato Brain Board V2.0.

---

## Files

### Brain_Board_Reference.md
Complete hardware reference — MCU specs, pin definitions, I2C addresses, power rails, and peripheral wiring.

### kicad/Brain_Board_V2.0/
KiCad 9.0 design files for the Brain Board V2.0 PCB.

| File | Description |
|---|---|
| `Automato_V2.0.kicad_sch` | Schematic |
| `Automato_V2.0.kicad_pcb` | PCB layout |
| `Automato_V2.0.kicad_pro` | KiCad project file |

**To open:** Install [KiCad 9.0](https://www.kicad.org/download/) and open `Automato_V2.0.kicad_pro`.

---

## Key Specs

| Property | Value |
|---|---|
| MCU | ESP32-C6-MINI-1-N4 |
| Flash | 4 MB |
| Onboard sensors | SHTC3 (temp/humidity, 0x70), TSL2591 (light, 0x29) |
| I2C | SDA = IO6, SCL = IO7 |
| Connector | 2× Qwiic (JST-PH 4-pin, 3.3V) |
| Power | USB-C, 3.3V logic |
| Logic level | 3.3V — GPIO pins not 5V tolerant |

---

## Onboard I2C Addresses (fixed — cannot be changed)

| Address | Device | Function |
|---|---|---|
| 0x29 | TSL2591 | Ambient light |
| 0x70 | SHTC3 | Temperature + humidity |

### Reserved addresses (next revision)
| Address | Device | Notes |
|---|---|---|
| 0x68 | DS3231 RTC | Onboard RTC planned for next Brain Board revision |

---

## Architecture Note

The Brain Board is always a discrete, standalone PCB. It is never integrated into a peripheral board. All output capability (relay, MOSFET, GPIO expansion) lives on separate peripheral boards connected via cable.

See [`docs/Roadmap.md`](../docs/Roadmap.md) for full hardware roadmap including the 6-pin Automato connector specification and planned peripheral board lineup.
