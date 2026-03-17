# Automato Brain Board — Product Roadmap

Last updated: 2026-03-17

This document captures the planned development path for the Automato Brain Board
firmware, dashboard, and automato.ag platform integration. It is a living document
— updated as decisions are made and architecture evolves.

---

## Guiding Principles

- **Offline first** — the board must function without internet or even a browser open
- **Progressive enhancement** — each tier adds capability without breaking the tier below
- **No lock-in** — users have full access to their sketch code at all times
- **Safe by default** — relays and outputs always default OFF under any failure condition
- **Useful out of the box** — a new Brain Board should be immediately functional
  with no configuration, no internet, and no external tools required

---

## The Stem Cell Concept

The Brain Board ships with a Base Firmware — a stable, minimal foundation
analogous to stem cells in biology. Like stem cells, it is undifferentiated: it
contains the basic machinery of life and the ability to become anything, but has
not yet specialised into a specific function.

From this base, the user (or automato.ag) delivers specialised firmware via OTA —
irrigation control, environmental monitoring, relay automation, etc. — without
ever needing a USB cable or the Arduino IDE again.

The base firmware is:
- Stable — versioned separately, rarely changes
- Trusted — the known-good fallback if a feature firmware update fails
- Self-sufficient — hosts a complete webapp locally, works with no internet
- Aware — scans I2C on boot, knows what hardware is connected
- Connected — registers with automato.ag when internet is available

---

## Base Firmware

### What Ships on Every Brain Board

```
Base Firmware
├── WiFi provisioning (AP mode on first boot, STA mode after setup)
├── OTA update receiver (/update endpoint)
├── Basic sensor reading (SHTC3, TSL2591)
├── I2C scan on boot — detects connected Qwiic devices
├── Offline webapp (served from LittleFS)
├── NVS storage for WiFi credentials and device settings
└── automato.ag registration (when internet available)
```

### First Boot Experience (no internet required)

1. User powers Brain Board for the first time
2. Board creates its own WiFi access point: Automato-XXXX
3. User connects phone or laptop to Automato-XXXX
4. Opens http://192.168.4.1 in browser
5. Full Automato webapp loads directly from the board
6. User sees live sensor readings immediately
7. User enters their WiFi credentials — board joins their network
8. Board now accessible at http://brainboard.local
9. If internet available — board registers with automato.ag

### After First Boot

- Board always accessible at http://brainboard.local on local network
- Offline webapp always available regardless of internet connectivity
- automato.ag enhances the experience when internet is available
- OTA updates can be pushed from automato.ag or uploaded manually

---

## Offline Webapp

### Concept

The Brain Board hosts a complete, self-contained Automato webapp served directly
from its own flash memory. No internet required. No automato.ag required.
Works in a field with no cell service.

### Three Hosting Contexts

The Automato webapp operates in three contexts, with graceful degradation:

```
Context 1 — Served from Brain Board (always available)
  Loaded from LittleFS on ESP32 flash
  Works completely offline
  Works without automato.ag
  Limited by available flash (~1-2MB for webapp)
  Updated via OTA filesystem update

Context 2 — Served from automato.ag (full featured)
  Unlimited size and capability
  Cloud compile, template library, multi-device management
  Communicates with board via local network or cloud relay
  Requires internet access

Context 3 — Both simultaneously (optimal)
  Board serves base UI
  automato.ag enhances with cloud features when internet available
  Graceful degradation to Context 1 when offline
  User experience is continuous across connectivity states
```

### Offline Webapp Feature Set

| Feature | Offline | With Internet |
|---|---|---|
| Live sensor dashboard | Yes | Yes |
| I2C device scanner | Yes | Yes |
| Relay manual control | Yes | Yes |
| WiFi configuration | Yes | Yes |
| OTA firmware update (manual .bin upload) | Yes | Yes |
| Agri Data sidebar (Open-Meteo etc.) | No | Yes |
| Cloud compile and flash | No | Yes |
| Template library | No | Yes |
| Multi-device management | No | Yes |
| Historical data charts | No | Yes |
| Remote access | No | Yes |

### Flash Memory Layout (4MB)

```
4MB Flash
├── Base firmware code         ~500KB
├── OTA partition (copy)       ~500KB   (required for safe OTA)
├── LittleFS (webapp + files)  ~2MB     (webapp HTML/CSS/JS lives here)
└── NVS (settings, rules)      ~256KB
```

---

## LittleFS Migration

### Why This Matters

Currently the dashboard HTML is embedded in the sketch as a PROGMEM string.
This approach works but does not scale — every UI change requires a full firmware
reflash, and the dashboard size is limited by available RAM.

### The LittleFS Approach

Migrating the webapp to LittleFS means:
- HTML, CSS, and JS stored as actual files on the ESP32 filesystem
- Webapp can be updated independently of firmware (separate OTA partition)
- Much larger webapp possible (~2MB vs current ~73KB)
- Firmware updates and webapp updates are decoupled

### Migration Plan

Planned alongside OTA work in v0.7 / v0.8. Existing PROGMEM dashboard
continues to work until migration is complete.

---

## Firmware Roadmap

### Stage 2 — Tier 3 Offline Rule Engine
Next milestone

- Rule builder UI in dashboard sidebar
- Rules stored in NVS via Arduino Preferences library
- WiFi failover detection with configurable heartbeat timeout
- evaluateRules() activated in loop() when offline or no higher tier active
- /config endpoint for saving/loading rules from dashboard
- Offline mode indicator in dashboard

### Stage 3 — Tier 2 Browser Rule Engine

- Full rule builder UI with condition editor
- Rules evaluated in browser JavaScript when dashboard tab is open
- Sends relay commands to board via existing /relay endpoint
- Operates on local network without internet
- Falls back to Tier 3 when browser closed

### Stage 4 — Tier 1 Cloud Rule Engine (automato.ag)

- Rules stored and evaluated on automato.ag server
- 24/7 operation without browser open
- Historical data logging
- Multi-device management
- Alert/notification system
- Requires automato.ag backend (see Platform Roadmap)

---

## Dashboard Roadmap

### Near Term

- I2C Bus Scanner panel — live scan from dashboard, device identification,
  browser-side device database (125+ devices from i2cdevices.org)
  Planned for v0.6.2

- OTA Firmware Update panel — upload compiled .bin from browser,
  progress indicator, version display, password protected
  Planned for v0.7

- LittleFS migration — move webapp from PROGMEM to filesystem,
  enable independent webapp + firmware updates
  Planned for v0.7 / v0.8

- mDNS support — board reachable at http://brainboard.local
  Planned, no version assigned yet

- AP mode first-boot UI — WiFi provisioning on http://192.168.4.1
  Planned for base firmware

### Medium Term

- Rule builder UI (Stage 2 + Stage 3 above)
- Multi-board management view
- Historical sensor data charts (requires data storage)
- Alert configuration UI

---

## Platform Roadmap (automato.ag)

### Phase 1 — Cloud Compile + Local OTA
Minimal infrastructure, maximum impact

Goal: User never needs Arduino IDE after first flash.

Components:
- Arduino CLI hosted on automato.ag server
- Automato board package installed on server
- Monaco Editor (VS Code editor) embedded in automato.ag webpage
- Pre-made .ino template library (relay control, soil monitoring, etc.)
- Compile API endpoint: accepts .ino, returns compiled .bin
- /update OTA endpoint on Brain Board (receives .bin, flashes, reboots)
- Browser acts as relay: downloads .bin from automato.ag, pushes to board
  on local network via http://brainboard.local/update

User flow:
1. Brain Board ships with base firmware pre-installed
2. User powers board, connects to Automato-XXXX AP
3. Configures WiFi via offline webapp at http://192.168.4.1
4. Board joins network, registers with automato.ag
5. User opens automato.ag, board appears in their account
6. I2C scan shows connected hardware — automato.ag suggests templates
7. User selects or edits template in Monaco Editor
8. Clicks Compile and Flash
9. automato.ag compiles, browser downloads .bin, browser pushes to board
10. Board flashes and reboots — no USB cable, no Arduino IDE ever again

Limitation: Browser must be open on the same local network as the board.

### Phase 2 — Full Cloud Relay + Remote OTA
Board is remotely accessible from anywhere

Components (builds on Phase 1):
- Device registration system (device token per board)
- Data relay backend: board POSTs sensor data to automato.ag on interval
- Hosted dashboard on automato.ag showing live + historical data
- Firmware update queue: automato.ag stores pending .bin, board polls and
  self-flashes without browser open
- User accounts with multi-device support
- Alert/notification system

User flow (OTA):
1. User edits/selects template on automato.ag
2. Clicks Compile and Flash
3. automato.ag compiles and queues firmware for device
4. Board polls, downloads .bin directly from automato.ag, flashes, reboots
5. Works from anywhere — board does not need to be on same network as user

---

## Three-Tier Automation Architecture

```
Tier 1 — Cloud (automato.ag)
  Full rule evaluation on server
  All data sources available
  24/7, no browser required
  Requires internet + automato.ag uptime
    |
    v failover if cloud unreachable
Tier 2 — Browser
  Rule engine runs in dashboard tab
  Local network only
  No internet required
  Only active while browser is open
    |
    v failover if browser closed
Tier 3 — On-board (offline)
  Simplified rules in NVS flash
  Local sensor data only
  No network required
  Always running as safety net
  Relay defaults OFF if no rules match
```

Failover: Tiers activate downward automatically on loss of higher tier.
Recovery: Board resumes highest available tier when connectivity restored.
Heartbeat: Configurable timeout — if no signal from higher tier within X
seconds, Tier 3 takes over.

---

## Rule Engine Design (all tiers)

- Each rule has: name, priority (1=highest), logic operator, target relay(s), conditions
- Conditions compare sensor values: above / below / equals threshold
- Logic operators between conditions: AND, OR, NOT, XOR
- Conflict resolution: highest priority rule wins
- If no rules fire: relay defaults OFF
- Manual dashboard toggle always overrides all rules regardless of priority
- Multiple rules can target the same relay — priority resolves conflicts

---

## Relay Safety Contract

This is non-negotiable at every tier and every version:

Relays default OFF under all failure conditions.
This includes: boot, sensor failure, WiFi loss, cloud loss, browser closed,
no rules configured, conflicting rules, and hardware expander not found.
There is no condition under which a relay defaults ON.

---

## Known Decisions and Rationale

| Decision | Rationale |
|---|---|
| TCA9534 address is 0x27 (not 0x20) | SparkFun Qwiic GPIO has all address jumpers bridged by default |
| ESP-NOW LR on AP interface only | Setting LR on STA before WiFi connects breaks connection |
| Highest priority rule wins conflicts | Industry standard. Enables safety override rules. |
| Relay defaults OFF always | Agricultural safety — unexpected ON state can damage crops, equipment, livestock |
| Browser-side external API calls | Keeps firmware lean; no API keys stored on device |
| Phase 1 OTA uses browser as relay | Avoids need for cloud infrastructure in early phase |
| Monaco Editor for cloud IDE | Same engine as VS Code — familiar, powerful, well maintained |
| Base firmware pre-installed at shipping | Users functional out of box, no tools required |
| Offline webapp on LittleFS | Updatable independently of firmware; scales beyond PROGMEM limits |
| AP mode on first boot | True zero-infrastructure setup — works anywhere, no router required |
| Stem cell architecture | Stable base + specialised OTA overlays = safe, flexible, maintainable |

---

## Version History Summary

| Version | Key Feature |
|---|---|
| v0.1 | Single-board sensor dashboard |
| v0.2 | Two-board ESP-NOW LR |
| v0.3 | ESP-IDF v5.5+ callback fix |
| v0.4 | WiFi + LR coexistence fix |
| v0.5 | Agri Data sidebar |
| v0.6 | Manual relay control (Qwiic GPIO) |
| v0.6.1 | TCA9534 address fix (0x20 to 0x27) |
| v0.6.2 | I2C scanner panel in dashboard (planned) |
| v0.7 | OTA firmware update panel + LittleFS migration (planned) |
| v0.8 | AP mode first-boot provisioning + base firmware (planned) |
| v1.0 | Base firmware — stable, shippable, pre-installed (target) |

---

## Multi-Board Networking

### Current Architecture (ESP-NOW Star Topology)

```
Router <-> Host Board <-> Remote 1
                     <-> Remote 2
```

All remote boards must be within ESP-NOW LR range of the host.
Maximum 20 peers per device (theoretical), 4-8 practical.

### Planned Architecture (ESP-Mesh-Lite)

```
Router <-> Board 1 <-> Board 2 <-> Board 3
                              <-> Board 4
```

Any board only needs to reach its nearest neighbour.
Range extends across a property by chaining boards together.
Self-forming and self-healing — boards find their own best path.

**Explicitly listed by Espressif as a smart agriculture target use case.**

### ESP-Mesh-Lite Specifications

| Parameter | Value |
|---|---|
| Maximum layers | 15 (5-6 recommended) |
| Max connections per node | 10 hardware limit, 6 recommended |
| Practical comfortable network size | 100-500 nodes |
| Node-to-node distance (stable) | Less than 100m |
| Node-to-node distance (low throughput) | ~170m |
| Self-forming | Yes — automatic parent node selection |
| Self-healing | Yes — automatic reconnection on node failure |
| OTA support | Yes — built in |
| Arduino compatibility | To be verified before implementation |

### Node Role Design

Fixed roles recommended for v1.0:
- User designates the board physically closest to their router as the root node
- All other boards automatically join the mesh and find their own path
- Role assignment is a one-time physical decision, not a software configuration
- Dynamic host election deferred to future roadmap item

### Migration Path from ESP-NOW

Espressif describes ESP-Mesh-Lite as a "quick migration" from existing
WiFi applications. The current ESP-NOW architecture does not block this
migration — ESP-Mesh-Lite is a transport layer change only. The three-tier
automation architecture, rule engine, relay control, and dashboard are
all unaffected.

**Action required before implementation:** Verify ESP-Mesh-Lite Arduino
framework compatibility. If not available, assess ESP-IDF integration
path or Arduino wrapper options.

---

## ESP32-C6 Unexplored Capabilities

The Brain Board's ESP32-C6 contains several hardware features not yet used
in current firmware. Each has direct relevance to agricultural IoT use cases.
Full details in [`docs/ESP32C6_Capabilities.md`](ESP32C6_Capabilities.md).

| Capability | Agricultural Relevance | Roadmap Status |
|---|---|---|
| LP (Low-Power) Co-Processor | Run Tier 3 rules while HP core sleeps — enables battery deployment | Future |
| Wi-Fi 6 TWT | Scheduled radio wake windows — extends battery life on remote nodes | Future |
| Bluetooth 5 LE | Phone-based provisioning, BLE sensor beacon, WiFi-down fallback | Planned (base firmware) |
| Zigbee 3.0 / Thread 1.3 | Alternative mesh transport for large multi-board deployments | Under consideration |
| Die Temperature Sensor | MCU health diagnostic, zero additional hardware | Easy win — any version |
| Hardware Crypto Accelerators | Practical HTTPS to automato.ag, secure boot for production boards | Phase 2 |
| Hardware PWM | Richer LED status indicators, buzzer, motor/dimmer control | Future |
| Hardware Pulse Counter (PCNT) | Flow meters, anemometers, rain gauges — standard precision ag sensors | Future |

Note: LP core, TWT, and 802.15.4 require ESP-IDF, not the Arduino framework.
This is a known tradeoff — Arduino is used now for development speed.
Migration to ESP-IDF for specific features is a future consideration.
