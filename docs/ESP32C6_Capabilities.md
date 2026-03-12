# ESP32-C6 — Unexplored Capabilities

The Brain Board's ESP32-C6-MINI-1-N4 contains several hardware features not yet used in current firmware. These are documented here as potential avenues for exploration in future versions.

---

## 1. LP (Low-Power) Co-Processor

The ESP32-C6 contains two completely independent CPUs:

- **HP core** — 160 MHz RISC-V, runs all current Arduino firmware
- **LP core** — 20 MHz RISC-V, can run independently while HP core sleeps

The LP core can read I2C sensors (including the onboard SHTC3 and TSL2591), monitor GPIO, evaluate threshold conditions, and wake the HP core only when action is required. Measured power consumption drops from ~22 mA (HP active) to ~3 mA (LP only) — roughly an order of magnitude.

**Relevance to Brain Board:** The Tier 3 offline rule engine (currently planned for `loop()` on the HP core) could run entirely on the LP core. The HP core would only wake for WiFi transmission or relay actuation. This is the architecture that enables serious unattended battery-powered deployment.

**Caveat:** LP core programming requires ESP-IDF, not the Arduino framework. Official Espressif examples include reading an I2C light sensor while HP is in deep sleep and waking HP when a threshold is exceeded — essentially the Brain Board's exact hardware setup.

- ESP-IDF docs: [ULP LP-Core Coprocessor Programming](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/system/ulp-lp-core.html)

---

## 2. Wi-Fi 6 Target Wake Time (TWT)

The Brain Board uses Wi-Fi 6 (802.11ax). TWT is a Wi-Fi 6 feature that lets a device negotiate a fixed schedule with the router: "I will transmit every 30 seconds at these exact intervals — keep the radio off otherwise." Between scheduled windows the radio is truly powered down.

**Relevance to Brain Board:** The Remote board (Board 2) currently transmits via ESP-NOW every 3 seconds regardless. A future battery-powered remote node using TWT over WiFi instead could negotiate a much longer sleep interval while remaining connected, potentially extending battery life by up to 67%.

**Caveat:** Both the board *and* the router must support Wi-Fi 6 TWT. Requires ESP-IDF Wi-Fi driver configuration, not the standard Arduino WiFi library.

---

## 3. Bluetooth 5 LE

BLE is active silicon on the C6 but unused in current firmware. Three practical use cases for the Brain Board:

**Configuration interface:** A BLE GATT server would let you configure WiFi credentials, offline rules, and thresholds from a phone app before the board is connected to WiFi — the same pattern used by most commercial IoT products.

**Passive sensor beacon:** The board can broadcast sensor readings via BLE advertisement packets simultaneously with serving the WiFi dashboard. Readings become visible to any phone within Bluetooth range without opening a browser.

**WiFi-down fallback:** If the router is unavailable, a phone app could poll sensors directly over BLE even when the dashboard is unreachable.

Bluetooth 5 LE long-range coded PHY mode also extends BLE range significantly — useful for sensor nodes at the edge of WiFi coverage.

---

## 4. Zigbee 3.0 / Thread 1.3 (IEEE 802.15.4)

The same radio that handles Wi-Fi and BLE can also run Zigbee 3.0 or Thread 1.3. The Arduino board package already includes Zigbee mode options in the IDE menu.

**Relevance to Brain Board:** Multiple remote sensor nodes could form a Thread or Zigbee mesh and route data back to a Host board acting as a border router, which then bridges to WiFi. This scales better than ESP-NOW for deployments with many boards spread across large areas.

**Caveat:** Wi-Fi, BLE, and 802.15.4 share the same antenna and cannot all run simultaneously at full capacity. Espressif provides coexistence firmware, but configuration and throughput tradeoffs apply.

---

## 5. Onboard Die Temperature Sensor

The ESP32-C6 has a hardware temperature sensor built into the chip silicon, separate from the SHTC3. It measures the die temperature of the MCU itself.

**Relevance to Brain Board:** Useful as a diagnostic — elevated die temperature relative to ambient indicates heavy CPU or radio load. Accessible in Arduino with a single call: `temperatureRead()`. Zero additional hardware required.

---

## 6. Hardware Crypto Accelerators

The C6 includes hardware acceleration for AES, SHA, RSA, and ECC — the operations that make TLS/HTTPS expensive on software-only microcontrollers.

**Relevance to Brain Board:** When the board communicates with the automato.ag cloud relay, HTTPS is practical without significant performance impact. Also enables Flash Encryption and Secure Boot for production deployments — preventing firmware and credential extraction from a board that is physically obtained. These are one-time production-hardening steps (irreversible once enabled) to be considered before shipping boards to customers.

---

## 7. Hardware PWM (LED Controller)

Six independent hardware PWM channels at 14-bit resolution, accessible via `analogWrite()` or the Arduino LEDC library.

**Relevance to Brain Board:** The three onboard LEDs (red IO22, blue IO23, green) could use breathing/pulsing animations as richer status indicators rather than on/off. Also applicable to buzzer tones, motor speed control, or dimming any load on a relay.

---

## 8. Hardware Pulse Counter (PCNT)

A hardware peripheral that counts digital input transitions independently of the CPU — no polling, no missed counts, works while the CPU is sleeping.

**Relevance to Brain Board:** Directly applicable to common agricultural sensors:

| Sensor Type | What It Counts |
|---|---|
| Flow meter | Pulses → water volume delivered to irrigation |
| Anemometer | Pulses → wind speed |
| Rain gauge (tipping bucket) | Pulses → rainfall (typically 0.2 mm per tip) |

These sensors are standard in precision agriculture. Currently, integrating them would require polling a GPIO (burning CPU time) or risking missed pulses during sleep. The hardware counter handles all of this for free.

---

*For current firmware features and roadmap, see the [main README](../README.md).*
