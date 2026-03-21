# Automato Brain Board V2.0 — Hardware Reference

## MCU

| Property | Value |
|---|---|
| Chip | ESP32-C6-MINI-1-N4 |
| Architecture | RISC-V dual core (HP core 160 MHz + LP core 20 MHz) |
| Clock | 160 MHz |
| Flash | 4 MB |
| RAM | 512 KB |
| WiFi | 802.11 b/g/n/ax (2.4 GHz) + ESP-NOW LR |
| Bluetooth | BLE 5.0 |
| Logic Level | 3.3V — GPIO pins NOT 5V tolerant |

> The HP core runs all Arduino firmware. The LP core is a secondary low-power processor that can operate independently while the HP core sleeps — see [ESP32-C6 Unexplored Capabilities](../docs/ESP32C6_Capabilities.md).

---

## I2C Bus

| Pin | GPIO |
|---|---|
| SDA | IO6 |
| SCL | IO7 |

### Onboard I2C Devices

| Device | Address | Function |
|---|---|---|
| SHTC3 | 0x70 | Temperature + Relative Humidity |
| TSL2591 | 0x29 | Ambient Light (lux, visible, infrared) |

### Qwiic / STEMMA QT Connectors

- **J2** and **J3** — both connected to the same I2C bus
- Pinout: GND · 3V3 · SDA · SCL
- Compatible with all SparkFun Qwiic and Adafruit STEMMA QT devices
- **Note:** Power only (3.3V) — no 5V available on Qwiic connectors

---

## GPIO

| Pin | Function | Notes |
|---|---|---|
| IO6 | SDA | I2C data |
| IO7 | SCL | I2C clock |
| IO9 | Boot button | Active LOW, pulled up |
| IO22 | Red LED | Active HIGH |
| IO23 | Blue LED (LED_BUILTIN) | Active HIGH |
| EN | Reset button (SW1) | Hardware reset |

---

## Power

| Rail | Details |
|---|---|
| Input | USB-C |
| Operating voltage | 3.3V |
| GPIO output | 3.3V max |

---

## Relay Expansion (Qwiic GPIO)

For relay control, connect a **SparkFun Qwiic GPIO (TCA9534)** to J2 or J3.

| Property | Value |
|---|---|
| I2C address | 0x27 (all address jumpers bridged — SparkFun Qwiic GPIO default) |
| Library | SparkFun TCA9534 (Arduino Library Manager) |
| Relay board | Amazon B0CHFJSNP6 (5V coil, optoisolated, active HIGH) |
| Signal level | 3.3V from TCA9534 is sufficient to trigger optocoupler |
| Relay power | 5V from USB hub (independent of Brain Board) |

### Wiring

```
Brain Board Qwiic (J2 or J3)
  └── SparkFun Qwiic GPIO (TCA9534, 0x27)
        └── Pin 0 → Relay Board IN
```

### Power Setup

```
USB-C Hub
  ├── Port A → Brain Board USB-C (3.3V logic power)
  └── Port B → Relay Board USB-C (5V coil power)
               Shared ground via hub — no ground loop
```

---

## Notes

- No LoRa radio on Brain Board V2.0
- No LCD or display
- No SD card
- ESP-NOW Long Range (LR) mode requires enabling on AP interface **after** WiFi connects — not before
