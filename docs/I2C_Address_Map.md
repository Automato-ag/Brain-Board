# Automato I2C Address Map

Last updated: 2026-03-17

This is a living document. Every I2C device across the entire Automato product
family is tracked here. **This document must be consulted before finalising any
PCB design.** Changing an I2C address after a PCB is manufactured requires
cutting traces or adding wire jumpers.

---

## How to Use This Document

Before adding any I2C chip to a new PCB design:
1. Check the Reserved Addresses table — confirm the address is not already taken
2. Check the Known Conflicts table — confirm no conflict with Brain Board onboard devices
3. Add the new device to the Full Address Allocation table
4. Note whether the address is fixed or configurable

---

## Brain Board Onboard Devices (permanent constraints)

These addresses are fixed or reserved on the Brain Board PCB. Every product in the
Automato ecosystem must work around them.

| Device | Address | Fixed? | Notes |
|---|---|---|---|
| SHTC3 (temp/humidity) | 0x70 | YES — hardcoded | Cannot be changed. High conflict risk. |
| TSL2591 (light) | 0x29 | YES — hardcoded | Cannot be changed. |
| DS3231 (RTC) | 0x68 | YES — hardcoded | Added to Brain Board in next revision. Do not assign other devices to 0x68. |

> ⚠️ 0x70 is a high-risk address. The TCA9548A I2C multiplexer defaults to 0x70.
> Any Automato product using a TCA9548A must configure it to 0x71 or higher.
> Any peripheral board using a chip that can be at 0x70 must ship with that chip
> at a different address.

---

## Reserved Address Blocks

These ranges are reserved for specific Automato purposes.
Do not assign other devices to these addresses.

| Address Range | Reserved For | Notes |
|---|---|---|
| 0x29 | TSL2591 (Brain Board onboard) | Fixed — do not use |
| 0x68 | DS3231 RTC (Brain Board onboard) | Fixed — do not use |
| 0x50 | Self-ID EEPROM — Brain Board Solo (BB-01) | AT24C02 |
| 0x51 | Self-ID EEPROM — AC Relay Board (AC-01) | AT24C02 |
| 0x52 | Self-ID EEPROM — AC Wall Plug (AC-02) | AT24C02 |
| 0x53 | Self-ID EEPROM — DC MOSFET Board (DC-01) | AT24C02 |
| 0x54 | Self-ID EEPROM — DC Relay Board (DC-02) | AT24C02 |
| 0x55 | Self-ID EEPROM — Motion Control Board (DC-03) | AT24C02 |
| 0x56 | Self-ID EEPROM — Battery Management (DC-04) | AT24C02 |
| 0x57 | Self-ID EEPROM — RS-485 Board (CN-01) | AT24C02 |
| 0x70 | SHTC3 (Brain Board onboard) | Fixed — do not use |

> Note: AT24C02 supports addresses 0x50–0x57 (8 addresses total, one per board type).
> If more than 8 board types need self-identification EEPROMs, use a different EEPROM
> chip with a different base address, or use the I2C Hub to isolate EEPROM buses.

---

## Full Address Allocation Table

### 0x00 – 0x0F (Reserved by I2C specification)
Do not use.

### 0x10 – 0x1F

| Address | Device | Product | Configurable? | Notes |
|---|---|---|---|---|
| 0x10 | — | — | — | Available |
| 0x11 | — | — | — | Available |
| 0x12–0x1F | — | — | — | Available |

### 0x20 – 0x2F

| Address | Device | Product | Configurable? | Notes |
|---|---|---|---|---|
| 0x20 | TCA9534 GPIO expander | BB-02 / AC-01 / DC-01 | YES (A0-A2) | Default if all jumpers open. Our boards ship at 0x27 (all bridged) |
| 0x21 | TCA9534 variant | Expansion | YES | Available if needed |
| 0x22 | TCA9534 variant | Expansion | YES | Available if needed |
| 0x23 | TCA9534 variant | Expansion | YES | Available if needed |
| 0x24 | TCA9534 variant | Expansion | YES | Available if needed |
| 0x25 | TCA9534 variant | Expansion | YES | Available if needed |
| 0x26 | TCA9534 variant | Expansion | YES | Available if needed |
| 0x27 | TCA9534 GPIO expander | Current (SparkFun Qwiic GPIO) | YES | All address jumpers bridged — current hardware default |
| 0x28–0x2F | — | — | — | Available |

### 0x30 – 0x3F

| Address | Device | Product | Configurable? | Notes |
|---|---|---|---|---|
| 0x30–0x3B | — | — | — | Available |
| 0x3C | SSD1306 OLED display | BB-03 | Limited (0x3C or 0x3D) | Default OLED address |
| 0x3D | SSD1306 OLED display alt | BB-03 alt | Limited | Alternate OLED address |
| 0x3E–0x3F | — | — | — | Available |

### 0x40 – 0x4F

| Address | Device | Product | Configurable? | Notes |
|---|---|---|---|---|
| 0x40 | INA219 current monitor | AC-01/DC-01/DC-02/EX-02 ch1 | YES (A0-A1) | 4 possible addresses 0x40–0x43. Default 0x40. |
| 0x41 | INA219 current monitor | ch2 | YES | |
| 0x42 | INA219 current monitor | ch3 | YES | |
| 0x43 | INA219 current monitor | ch4 | YES | |
| 0x44–0x47 | INA219 variants | Expansion | YES | Soft reserve for additional INA219 channels |
| 0x40 | HDC1080 humidity sensor | EX-03 | Fixed at 0x40 | CONFLICT RISK with INA219 — use I2C Hub if both needed |
| 0x48 | ADS1115 ADC | EX-03 (soil moisture, EC) | YES (A0 pin) | 4 addresses: 0x48–0x4B |
| 0x49 | ADS1115 ADC alt | EX-03 alt channel | YES | |
| 0x4A–0x4B | ADS1115 variants | Expansion | YES | |

> **INA219 vs INA260:** INA219 (~$1.50 each) selected over INA260 (~$5.58 each).
> On a 4-channel board INA219 adds ~$6 to BOM vs ~$22 for INA260.
> INA219 accuracy is sufficient for agricultural load fault detection.
> INA219 uses addresses 0x40–0x43 (4 addresses via A0/A1 pins).
> INA260 uses 0x40–0x4F (16 addresses) — not needed for current lineup.

> ⚠️ CONFLICT: HDC1080 (0x40 fixed) conflicts with INA219 at default address 0x40.
> If both EX-03 and a power board with INA219 are on the same bus, configure
> INA219 to 0x41+ OR use I2C Hub to isolate them on separate channels.

### 0x50 – 0x5F

| Address | Device | Product | Configurable? | Notes |
|---|---|---|---|---|
| 0x50 | AT24C02 Self-ID EEPROM | BB-01 Brain Board Solo | Fixed per board type | RESERVED |
| 0x51 | AT24C02 Self-ID EEPROM | AC-01 AC Relay Board | Fixed per board type | RESERVED |
| 0x52 | AT24C02 Self-ID EEPROM | AC-02 AC Wall Plug | Fixed per board type | RESERVED |
| 0x53 | AT24C02 Self-ID EEPROM | DC-01 DC MOSFET Board | Fixed per board type | RESERVED |
| 0x54 | AT24C02 Self-ID EEPROM | DC-02 DC Relay Board | Fixed per board type | RESERVED |
| 0x55 | AT24C02 Self-ID EEPROM | DC-03 Motion Control Board | Fixed per board type | RESERVED |
| 0x56 | AT24C02 Self-ID EEPROM | DC-04 Battery Management | Fixed per board type | RESERVED |
| 0x57 | AT24C02 Self-ID EEPROM | CN-01 RS-485 Board | Fixed per board type | RESERVED |
| 0x58–0x5F | — | — | — | Available — reserve for future Self-ID EEPROMs |

### 0x60 – 0x6F

| Address | Device | Product | Configurable? | Notes |
|---|---|---|---|---|
| 0x60 | PCA9685 PWM driver | DC-04 Servo Driver | YES (A0-A5) | 64 possible addresses 0x40–0x7F — using 0x60 as default |
| 0x61–0x6F | PCA9685 variants | Expansion | YES | Reserve for additional servo/PWM boards |
| 0x62 | SCD40 CO2 sensor | EX-03 | Fixed at 0x62 | CONFLICT with PCA9685 if PCA9685 configured to 0x62 — avoid |

> Note: PCA9685 has 6 address pins and supports 64 addresses from 0x40–0x7F.
> Configure PCA9685 to avoid 0x40–0x4F (INA260/ADS1115 range) and 0x62 (SCD40).
> 0x60 is a clean default.

### 0x70 – 0x7F

| Address | Device | Product | Configurable? | Notes |
|---|---|---|---|---|
| 0x70 | SHTC3 temp/humidity | Brain Board (onboard) | NO — FIXED | PERMANENT CONSTRAINT |
| 0x71 | TCA9548A I2C Multiplexer | EX-01 I2C Hub | YES (A0-A2) | Configured to 0x71 on PCB — avoids SHTC3 at 0x70 |
| 0x72–0x77 | TCA9548A variants | Expansion | YES | Available for additional Hub boards |
| 0x29 | TSL2591 light sensor | Brain Board (onboard) | NO — FIXED | Listed here for completeness — see 0x20-0x2F range |

---

## Known Conflict Summary

| Conflict | Devices | Resolution |
|---|---|---|
| HIGH RISK | SHTC3 (0x70, fixed) vs TCA9548A default (0x70) | TCA9548A must be configured to 0x71+ on all Automato PCBs |
| HIGH RISK | SHTC3 (0x70, fixed) vs HT16K33 LED driver default (0x70) | Do not use HT16K33 on same bus as Brain Board, or configure to 0x71+ |
| HIGH RISK | DS3231 (0x68, Brain Board onboard) vs MPU6050/ICM-20948 (0x68) | Use I2C Hub to isolate if IMU needed alongside Brain Board |
| MEDIUM RISK | INA219 default (0x40) vs HDC1080 (0x40, fixed) | Use I2C Hub to isolate, or configure INA219 to 0x41+ |
| MEDIUM RISK | SCD40 (0x62, fixed) vs PCA9685 if configured to 0x62 | Configure PCA9685 away from 0x62 |
| LOW RISK | Multiple TCA9534 boards at same address | Set different A0-A2 jumpers per board — up to 8 boards supported |
| LOW RISK | Multiple AT24C02 EEPROMs | Each board type assigned a unique address 0x50–0x57 |

---

## Addresses Available for Future Assignment

The following ranges are currently unallocated and available for new products:

- 0x10–0x1F (16 addresses)
- 0x28–0x2F (8 addresses, excluding TCA9534 variants)
- 0x30–0x3B (12 addresses)
- 0x44–0x47 (4 addresses — soft reserve for INA260 expansion)
- 0x58–0x5F (8 addresses — soft reserve for future Self-ID EEPROMs)
- 0x63–0x6F (13 addresses, excluding SCD40 at 0x62)
- 0x72–0x77 (6 addresses — TCA9548A variants)

---

## DS3231 RTC Note

The DS3231 RTC uses address 0x68 and is now integrated directly onto the Brain
Board PCB (next revision). This means 0x68 is a permanent Brain Board constraint
alongside 0x70 (SHTC3) and 0x29 (TSL2591).

0x68 is also used by several IMUs (MPU6050, ICM-20948) and other RTCs (DS1307,
PCF8523). If any future Automato product uses an IMU, a conflict will occur.
Use the I2C Hub (EX-01) to isolate these devices on separate channels if both
are needed on the same Brain Board.

There is no longer a standalone RTC expansion board (EX-04 removed from lineup).
All Brain Boards include RTC from the next hardware revision.

---

## I2C Hub Usage Guide

When two or more Automato peripheral boards have conflicting I2C addresses,
use the EX-01 I2C Hub board (TCA9548A at 0x71):

1. Connect I2C Hub to Brain Board via 6-pin Automato cable
2. Connect conflicting boards to separate Hub ports
3. Brain Board firmware detects Hub via self-ID EEPROM on boot
4. Firmware automatically manages channel switching transparently
5. User never needs to think about address conflicts

The I2C Hub is not needed for most simple setups. It is an accessory for
complex multi-board installations.

---

## Updating This Document

When adding a new IC to any Automato PCB design:
1. Look up its I2C address(es) in the datasheet
2. Check this document for conflicts
3. Add an entry to the Full Address Allocation Table
4. Update the Known Conflict Summary if applicable
5. Commit the updated document to the Brain-Board repository

Filename: `docs/I2C_Address_Map.md`
Repository: `https://github.com/Automato-ag/Brain-Board`
