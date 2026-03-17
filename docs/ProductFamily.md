# Automato Product Family

Last updated: 2026-03-17

This document defines the hardware architecture, connector standard, product lineup,
and design principles for the Automato product family. It is a living document —
updated as products are defined and decisions are made.

---

## Core Architecture Principle

The Brain Board is the computational organ of every Automato product. It is always
a discrete, separate PCB. It is never integrated into another board. In every
product — bundled or standalone — the Brain Board connects to peripheral boards
via the Automato 6-pin connector.

```
Brain Board (always separate)
    │
    │ 6-pin Automato connector (Qwiic-compatible subset)
    │
Peripheral Board(s) — any combination, daisy-chained or hub-connected
```

Any number of peripheral boards can connect to a single Brain Board over the I2C
bus simultaneously. A single enclosure may contain a Brain Board plus multiple
peripheral boards, all connected by 6-pin Automato cables.

---

## The Two Purchasing Paths

**Separate purchase:** Buy a Brain Board and any peripheral board(s) independently.
Connect with a 6-pin Automato cable. Maximum flexibility, repairability, and
upgradeability. Ideal for custom installations, developers, and advanced users.

**Bundled purchase:** Buy a Brain Board and peripheral board(s) pre-assembled inside
an enclosure. The boards inside a bundle are identical to the separately-sold boards,
connected by the same 6-pin Automato cable. Opening the enclosure and replacing one
board is always possible.

The firmware, the ecosystem, and the automato.ag platform behave identically
regardless of which path the user took.

---

## The 6-Pin Automato Connector

**Physical spec:** JST-PH 2.0mm pitch, 6-pin, keyed and polarized.

**Pinout:**

| Pin | Signal | Notes |
|---|---|---|
| 1 | GND | Common ground |
| 2 | 3.3V | Logic power from Brain Board |
| 3 | SDA | I2C data |
| 4 | SCL | I2C clock |
| 5 | INT | Interrupt — peripheral to Brain Board (active LOW) |
| 6 | 5V or 3.3V | Second power rail — defined per board type |

**Backward compatibility:** A 6-pin to 4-pin Qwiic adapter cable allows standard
Qwiic/STEMMA QT sensors to connect to any Automato 6-pin port. Pins 1–4 are
identical to Qwiic. Pins 5 and 6 are unused in Qwiic-adapter mode.

**Protocol:** I2C is the primary communication protocol. All peripheral boards
communicate with the Brain Board over SDA/SCL. Peripheral boards use onboard
I2C chips (GPIO expanders, ADCs, PWM drivers) to translate I2C commands into
the signals needed for their specific function — analog, PWM, relay switching, etc.

**Why 6-pin over 8-pin:** Most peripheral board signal needs (analog, PWM, GPIO)
are better handled by onboard I2C chips than by routing those signals through
the cable. The 6-pin connector carries everything needed for the vast majority
of use cases in a compact, affordable form factor.

---

## Self-Identification

Every peripheral board carries a small I2C EEPROM that stores its board type,
hardware version, and channel count. The Brain Board reads this on boot and
configures itself automatically — no jumpers, no firmware selection, no user
configuration required.

A user plugs in a peripheral board and the Brain Board knows what it is.

**EEPROM standard:** AT24C02 or equivalent, 256 bytes.
**Address allocation:** See `docs/I2C_Address_Map.md` for reserved EEPROM addresses.

---

## Product Lineup

### Brain Board Products

**BB-01 — Brain Board Solo**
Standalone Brain Board for sensor reading and reporting. Onboard SHTC3
(temperature/humidity), TSL2591 (light), DS3231 RTC, WiFi, ESP-NOW, and
automato.ag integration. No relay, no mains power. The simplest product —
connects to any peripheral board via 6-pin Automato connector.

> **Design decision:** DS3231 RTC is included onboard in next Brain Board revision.
> RTC adds ~$3.10 to BOM (DS3231 ~$2.50 + CR2032 holder ~$0.30 + passives ~$0.30).
> Justified because accurate time enables offline rule scheduling on every product
> in the lineup without requiring a separate board.
> DS3231 uses I2C address 0x68 — reserved in I2C Address Map.
> Estimated Brain Board BOM: ~$11.80 (components only, excl. PCB/assembly).

**BB-02 — Brain Board with GPIO Expander**
Brain Board plus a GPIO expander peripheral board (TCA9534 or similar) in a
compact enclosure. Exposes I/O pins for user-provided sensors, switches, and
devices. For advanced users and custom installations.

**BB-03 — Brain Board with Display (Future)**
Brain Board with an I2C OLED or e-ink display in a purpose-built enclosure.
Local sensor data readout without phone or laptop. Display connects via
6-pin Automato connector.

---

### AC Power Control Products

> ⚠️ All AC products involve mains voltage. See Safety section below.

**AC-01 — Smart Power Strip**
Multi-channel AC relay board in a power strip enclosure with integrated Brain Board.
Evolution of the existing Automato Relay Board. Redesigned to use I2C control
(TCA9534 GPIO expander) rather than 74HC595 shift register. Brain Board connects
via 6-pin Automato cable inside enclosure. Relay board sold separately for users
who supply their own Brain Board.

Upgrade notes from existing Relay Board:
- Control interface: 74HC595 SPI-like → TCA9534 I2C (consistent with ecosystem)
- Connector: USB-C control interface → 6-pin Automato
- Self-identification EEPROM added
- Per-channel current sensing via INA219 (see note below)

> **Current sensing decision:** INA219 (~$1.50 each) used instead of INA260
> (~$5.58 each). For 4-channel board: INA219 adds ~$6 to BOM vs ~$22 for INA260.
> INA219 accuracy is sufficient for agricultural load monitoring (pump fault
> detection, heater verification). INA260's higher accuracy and wider voltage
> range are not needed at 120/240VAC relay loads.
> Estimated AC-01 BOM with INA219 current sensing: ~$23 (components only).

**AC-02 — Smart Wall Plug**
Compact wall plug form factor. Brain Board connects via 6-pin Automato cable to
a relay PCB. Relay PCB plugs directly into wall outlet or power strip and exposes
a controlled NEMA 5-15 or NEMA 5-20 receptacle. Single controlled outlet.
Thermal management in sealed enclosure is a key design constraint.

> Note: UL/ETL certification required for commercial sale of mains-connected
> wall plug products in North America. Plan certification process before
> committing to production volumes.

---

### DC Power Control Products

**DC-01 — DC MOSFET / High-Side Switch Board**
Multi-channel MOSFET/high-side switch board for DC loads — solenoids, pumps,
motors, valves, LED lighting. I2C controlled via onboard TCA9534 GPIO expander.
Connects to Brain Board via 6-pin Automato cable.

Notes:
- MOSFETs preferred over relays for inductive DC loads (no contact arc welding)
- PWM capability on each channel via onboard PCA9685
- Freewheeling diodes required on all inductive load channels
- Rated for typical agricultural DC loads: 12V/24V solenoids, 12V pumps
- Per-channel current sensing via INA219
- Estimated BOM with INA219 current sensing: ~$11.61 (components only)

**DC-02 — DC Relay Board**
Multi-channel relay board for DC loads requiring full electrical isolation.
For applications where MOSFET switching is insufficient — high-voltage DC,
mixed signal/power isolation, or user preference.
- Per-channel current sensing via INA219
- Estimated BOM with INA219: ~$18.11 (components only)

**DC-03 — Motion Control Board (Stepper + Servo)**
Consolidated board combining stepper motor driver and PCA9685 servo/PWM driver.
Covers both actuation types on one board — irrigation valve positioning, automated
vents, feeder mechanisms, damper control, and angular positioning applications.

> **Consolidation decision:** Stepper and servo boards merged into one.
> Adding PCA9685 servo capability to the stepper board costs ~$2.25 in BOM.
> A user doing valve actuation may need either a stepper or servo depending on
> valve type — one board covers both. Estimated BOM: ~$8.36 (components only).

**DC-04 — Battery Management Board**
LiPo/LiFePO4 charging, protection, and state-of-charge reporting over I2C.
For battery-powered remote node deployments. Works with LP core deep sleep
architecture for multi-month battery life on remote sensor nodes.
- Estimated BOM: ~$6.05 (components only)

---

### Connectivity Products

**CN-01 — RS-485 / Modbus Board**
MAX3485 or similar transceiver. Screw terminals for RS-485 field cable (A, B, GND).
Termination resistor jumper. Self-identification EEPROM. Connects to Brain Board
via 6-pin Automato cable.

Enables Brain Board compatibility with the large ecosystem of professional
agricultural sensors using Modbus RTU — soil probes, weather stations,
flow meters, irrigation controllers.

**CN-02 — Cellular Board**
LTE Cat-M1/NB-IoT modem (SIM7600 or similar). SIM card slot. Connects to
Brain Board via 6-pin Automato cable. Provides internet connectivity where
WiFi is unavailable — remote field deployments, no local infrastructure required.

Enables Phase 2 cloud relay (automato.ag data relay + remote OTA) without WiFi.
Particularly relevant for Brain Boards running as autonomous remote nodes.

---

### Expansion and Monitoring Products

**EX-01 — I2C Hub Board**
TCA9548A-based 8-channel I2C multiplexer. Connects to Brain Board via 6-pin
Automato cable. Exposes 4 or 8 downstream 6-pin Automato ports.

Solves I2C address conflicts transparently — user plugs conflicting boards into
different Hub ports, firmware handles channel switching automatically.

Note: TCA9548A configured to 0x71 or higher on PCB (0x70 reserved for SHTC3).
Estimated BOM: ~$3.22 (components only).

**EX-02 — Current and Power Monitoring Board**
INA219 per channel, monitoring load current on relay/MOSFET outputs of third-party
or non-Automato boards. Detects failed pumps, burned-out heaters, open circuit
faults, and overloads. Alerts Brain Board via INT line when thresholds exceeded.

> **INA219 vs INA260:** INA219 (~$1.50) used instead of INA260 (~$5.58).
> INA219 accuracy is sufficient for agricultural load monitoring. Current sensing
> is already built into Automato power boards (AC-01, DC-01, DC-02) — this
> standalone board exists for users monitoring loads on third-party hardware.

**EX-03 — Environmental Sensor Expansion Board**
Additional agricultural sensors beyond Brain Board onboard sensors:
- CO2: SCD40 (0x62)
- Barometric pressure: BMP280 (0x76 or 0x77)
- Soil moisture: capacitive sensor with ADS1115 ADC
- EC (electrical conductivity): via ADS1115 or dedicated IC

> **Note:** RTC functionality (DS3231) has been moved onto the Brain Board PCB
> and is no longer a separate expansion board. All Brain Boards include onboard
> RTC from the next hardware revision.

---

## Safety Contract

This contract applies to every Automato product without exception:

> **All outputs (relay contacts, MOSFET channels, motor drivers) default OFF
> under all failure conditions.**
>
> This includes: board power-up before firmware initializes, firmware crash,
> sensor failure, WiFi loss, cloud loss, I2C bus error, and hardware expander
> not detected.
>
> There is no condition under which any output defaults ON.

**Hardware enforcement:** Relay coils and MOSFET gates must be held in the OFF
state by hardware (pull-down resistors to ground) before firmware initializes.
The safety contract must be a hardware guarantee, not only a software guarantee.

**For AC products specifically:**
- Brain Board and mains voltage must never share a PCB
- AC-DC converter (HiLink HLK-PM03 or equivalent) provides isolated 3.3V
- Relay isolation rated minimum 2500VAC coil-to-contact
- Creepage distance minimum 2.5mm at 250VAC
- Fused AC input on all mains-connected products
- Earth connection required on all mains products

---

## Thermal Management

All products in enclosed enclosures — particularly AC-02 (wall plug) and any
product installed in direct sun — require thermal analysis before production:

- HiLink HLK-PM03 dissipates heat in operation
- Brain Board with WiFi active dissipates heat
- Sealed enclosures in direct agricultural sun can exceed 70°C ambient
- Component derating at elevated temperature must be verified against datasheets
- Ventilation slots or thermal mass calculation required for sealed designs

---

## Regulatory Notes

| Product Type | Regulatory Path |
|---|---|
| DC-only products | Lower burden — no mains voltage certification typically required for component-level boards |
| AC products (mains voltage) | UL/ETL listing expected for commercial sale in North America. CE marking for EU. Significant time and cost — plan early. |
| Cellular products | FCC Part 15 / IC certification for RF. Using a pre-certified cellular module simplifies this considerably. |
| Brain Board | FCC Part 15 Class B for WiFi — ESP32-C6 module may carry existing certification depending on module variant. |

**Recommendation:** Develop and validate DC products before pursuing AC product
certification. DC products have lower regulatory burden and directly address
core agricultural use cases (irrigation, pumps, solenoids).

---

## Roadmap Congruence

This product family is consistent with the Brain Board roadmap in the following ways:

- **Stem cell concept** — Brain Board is the universal computational core
- **Offline first** — every product works without internet
- **Safe by default** — safety contract enforced at hardware level
- **Useful out of the box** — self-identification means no configuration required
- **Progressive enhancement** — Phase 2 cloud relay enabled by cellular board
- **I2C Hub** — transparent conflict resolution, no user expertise required
- **Battery Management Board** — enables LP core + deep sleep battery deployment
- **RTC on Brain Board** — every product gets time-based offline rules automatically

The hardware detection layer (Brain Board reads self-identification EEPROM on boot
and configures itself) should be added to the firmware roadmap before v1.0.

---

## Component Cost Summary

BOM costs only — components at ~100 unit volumes. Does not include PCB
fabrication, assembly, connectors, enclosure, or margin.
PCB + assembly adds approximately $8–15 per board at low volumes.

| Product | Est. BOM Cost | Key cost driver |
|---|---|---|
| BB-01 Brain Board (with RTC) | ~$11.80 | ESP32-C6 module ~$3.50 |
| AC-01 Smart Power Strip (4ch + INA219) | ~$23.00 | Relays + HLK-PM03 |
| AC-02 Smart Wall Plug | ~$18.00 | Relay + HLK-PM03 + compact design |
| DC-01 DC MOSFET Board (4ch + INA219) | ~$11.61 | Simple — no mains components |
| DC-02 DC Relay Board (4ch + INA219) | ~$18.11 | Relays dominate cost |
| DC-03 Motion Control (stepper + servo) | ~$8.36 | Consolidation adds only $2.25 vs stepper-only |
| DC-04 Battery Management | ~$6.05 | Charger IC + fuel gauge |
| CN-01 RS-485 / Modbus | ~$2.15 | Very low cost, high ecosystem value |
| CN-02 Cellular | ~$25–40 | Modem IC dominates |
| EX-01 I2C Hub | ~$3.22 | Very low cost accessory |
| EX-02 Current Monitoring (standalone) | ~$8.00 | INA219 × 4 channels |
| EX-03 Environmental Sensor | ~$12–18 | Depends on sensor selection |
