# ESP32-C6 — Unexplored Capabilities

The Brain Board's ESP32-C6-MINI-1-N4 contains several hardware features not yet used in current firmware. These are documented here as potential avenues for exploration in future versions.

---

## 1. LP (Low-Power) Co-Processor

The ESP32-C6 contains two completely independent CPUs:

- **HP core** — 160 MHz RISC-V, runs all current Arduino firmware
- **LP core** — 20 MHz RISC-V, can run independently while HP core sleeps

The LP core can read I2C sensors (including the onboard SHTC3 and TSL2591), monitor GPIO, evaluate threshold conditions, and wake the HP core only when action is required. Measured power consumption drops from ~22 mA (HP active) to ~3 mA (LP only) — roughly an order of magnitude.

**Relevance to Brain Board:** The Tier 3 offline rule engine (currently planned for `loop()` on the HP core in Stage 2) could eventually run entirely on the LP core. The HP core would only wake for WiFi transmission or relay actuation. This is the architecture that enables serious unattended battery-powered field deployment — a sensor node running for months on a battery, waking only when a rule condition is met.

**Connection to the roadmap:**
The Stage 2 Tier 3 rule engine will be written for the HP core in Arduino first. This is the right approach for development speed and testability. However, the rule engine should be designed with LP core migration in mind — keeping rule evaluation logic simple, stateless, and data-structure-focused so it can be ported later without a full rewrite.

**The Arduino/ESP-IDF decision point:**
LP core programming requires ESP-IDF, not the Arduino framework. This is a significant architectural decision that will need to be made before v1.0 ships as a production base firmware:

| Approach | Pros | Cons |
|---|---|---|
| Stay on Arduino, HP core only | Fast development, large community, easy OTA | 22mA always-on, no battery viability |
| ESP-IDF for LP core features only | LP core + HP core Arduino hybrid is possible | Increased complexity, mixed toolchain |
| Full ESP-IDF migration | Access to all low-level features | Major rewrite, loses Arduino ecosystem |

The recommended path is a **hybrid approach** — keep the Arduino framework for the main application on the HP core, and add ESP-IDF ULP/LP core code only for the specific sleep-and-sense loop. Espressif supports this pattern and provides examples. This decision does not need to be made now but should be made before designing the production battery-powered remote node.

**Caveat:** LP core programming requires ESP-IDF, not the Arduino framework. Official Espressif examples include reading an I2C light sensor while HP is in deep sleep and waking HP when a threshold is exceeded — essentially the Brain Board's exact hardware setup.

- ESP-IDF docs: [ULP LP-Core Coprocessor Programming](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/system/ulp-lp-core.html)

---

## 2. Wi-Fi 6 Target Wake Time (TWT)

The Brain Board uses Wi-Fi 6 (802.11ax). TWT is a Wi-Fi 6 feature that lets a device negotiate a fixed schedule with the router: "I will transmit every 30 seconds at these exact intervals — keep the radio off otherwise." Between scheduled windows the radio is truly powered down.

**Relevance to Brain Board:** The Remote board (Board 2) currently transmits via ESP-NOW every 3 seconds regardless. A future battery-powered remote node using TWT over WiFi instead could negotiate a much longer sleep interval while remaining connected, potentially extending battery life by up to 67%.

**Caveat:** Both the board *and* the router must support Wi-Fi 6 TWT. Requires ESP-IDF Wi-Fi driver configuration, not the standard Arduino WiFi library.

---

## 3. Bluetooth 5 LE

BLE is active silicon on the C6 but unused in current firmware. Four practical use cases for the Brain Board:

**Configuration interface:** A BLE GATT server would let you configure WiFi credentials, offline rules, and thresholds from a phone app before the board is connected to WiFi — the same pattern used by most commercial IoT products. This is the planned first-boot provisioning path for the base firmware (replacing or supplementing the AP mode WiFi setup page).

**Passive sensor beacon:** The board can broadcast sensor readings via BLE advertisement packets simultaneously with serving the WiFi dashboard. Readings become visible to any phone within Bluetooth range without opening a browser, entering credentials, or being on the same WiFi network. This is directly relevant to the offline webapp concept — a user walking through a field could see live readings from any Brain Board simply by having Bluetooth on their phone enabled. No app required for basic readings; a dedicated app could provide richer interaction.

**WiFi-down fallback:** If the router is unavailable, a phone app could poll sensors directly over BLE even when the dashboard is unreachable. Combined with the Tier 3 offline rule engine, this means a board can continue operating and reporting locally even with no network connectivity of any kind.

**Long-range coded PHY:** Bluetooth 5 LE supports a long-range coded PHY mode that significantly extends BLE range at the cost of throughput — useful for sensor nodes at the edge of WiFi coverage where full WiFi connection is not practical but periodic status updates are needed.

**Caveat:** BLE and WiFi share the antenna on the ESP32-C6. Simultaneous operation is supported via coexistence firmware but involves time-slicing the radio, which reduces effective throughput of both. For most agricultural sensor use cases this tradeoff is acceptable.

---

## 4. Zigbee 3.0 / Thread 1.3 (IEEE 802.15.4)

The same radio that handles Wi-Fi and BLE can also run Zigbee 3.0 or Thread 1.3.

**Relevance to Brain Board:** Multiple remote sensor nodes could form a Thread or Zigbee mesh and route data back to a Host board acting as a border router, which then bridges to WiFi. This scales better than ESP-NOW for deployments with many boards spread across large areas.

**Important clarification on accessibility:** The Automato Arduino board package already includes Zigbee mode options in the Arduino IDE Tools menu. This means basic Zigbee functionality may be accessible within the Arduino framework sooner than previously assumed — it does not necessarily require a full ESP-IDF migration. Thread support is less mature in the Arduino layer and more likely to require ESP-IDF.

**Comparison to ESP-Mesh-Lite (the current planned mesh solution):**

| | ESP-Mesh-Lite | Zigbee / Thread |
|---|---|---|
| Protocol | WiFi | IEEE 802.15.4 |
| Throughput | High | Low |
| Power consumption | Higher | Lower |
| Arduino support | Requires verification | Zigbee in board menu |
| Range per hop | ~100m | ~50–100m |
| Agricultural targeting | Espressif listed use case | Industry standard |

ESP-Mesh-Lite is the current planned path due to higher throughput and explicit smart agriculture targeting. Zigbee/Thread remains a strong alternative for battery-powered deployments where low power outweighs throughput needs.

**Caveat:** Wi-Fi, BLE, and 802.15.4 share the same antenna and cannot all run simultaneously at full capacity. Espressif provides coexistence firmware, but configuration and throughput tradeoffs apply. Running Zigbee/Thread means WiFi and BLE capability is reduced or disabled.

---

## 5. Onboard Die Temperature Sensor

The ESP32-C6 has a hardware temperature sensor built into the chip silicon, separate from the SHTC3. It measures the die temperature of the MCU itself.

**Relevance to Brain Board:** Useful as a diagnostic — elevated die temperature relative to ambient indicates heavy CPU or radio load. Accessible in Arduino with a single call: `temperatureRead()`. Zero additional hardware required.

**This is an immediate easy win.** It should be added to the `/data` JSON endpoint and displayed on the dashboard as a board health indicator in a near-term firmware version. The value is most meaningful when compared against the ambient temperature from the SHTC3 — a die temperature significantly above ambient is a useful early warning of thermal stress, particularly in enclosures in direct sun.

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

**Connection to battery deployment and LP core:** The PCNT and LP core are natural partners. The LP core can monitor the pulse counter and accumulate counts while the HP core sleeps, waking the HP core only to transmit accumulated data or when a threshold is exceeded (e.g. a flow meter detects unexpected irrigation activity at night). This combination — PCNT + LP core + deep sleep — is the architecture for a truly low-power agricultural sensor node that can run for months on battery while counting every raindrop and every litre of water delivered.

This should be considered when designing the Qwiic sensor expansion story for the Brain Board, and when planning which sensors are appropriate for battery-powered remote nodes versus mains-powered host nodes.

---

*For current firmware features and roadmap, see the [main README](../README.md).*
