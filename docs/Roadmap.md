# Automato Brain Board — Product Roadmap

Last updated: 2026-03-21

This document captures the planned development path for the Automato Brain Board
firmware, dashboard, and automato.ag platform integration. It is a living document
— updated as decisions are made and architecture evolves.

---

## Guiding Principles

- **Offline first** — the board must function without internet or even a browser open
- **Progressive enhancement** — each tier adds capability without breaking the tier below
- **No lock-in** — users have full access to their sketch code at all times
- **Safe by default** — outputs default to their hardware-defined failsafe state under any failure
- **Useful out of the box** — a new Brain Board should be immediately functional
  with no configuration, no internet, and no external tools required
- **Non-proprietary** — third-party I2C devices and hardware are first-class citizens;
  Automato hardware integrates into other ecosystems and vice versa
- **Novice-accessible** — most users are not programmers; every feature must be
  reachable without writing code; intermediate users should be able to extend
  without forking

---

## The Stem Cell Concept

The Brain Board ships with Base Firmware — a stable, minimal foundation
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

---

## UX Architecture

Three additive layers — each lower layer always works regardless of upper layer availability:

| Layer | Access | Features |
|-------|--------|---------|
| `index.html` (offline) | Local network only | All core features — sensor readings, relay control, rules, settings |
| `index.html` (online) | Local network + internet | Core features + internet-dependent features (Agri Data, weather) |
| automato.ag | Any network | Extended UX too large for the board + remote viewing |

**All user data lives on the board.** automato.ag holds only an ephemeral relay cache —
no user accounts, no stored preferences, no email, no registration required.

---

## Cloud Identity

- Automato Network Name and password are set during `/setup`
- Cloud token = `hash(Automato Network Name + password)` — derived locally on the board
- Network Name alone → mesh membership (boards find each other)
- Network Name + Password → cloud identity (board authenticates to automato.ag relay)
- automato.ag never sees credentials — only the derived token
- Auto-generated default name (e.g. `automato-garden-7e20`) makes collision negligible without hardware salt

---

## Base Firmware

### What Ships on Every Brain Board

```
Base Firmware
├── WiFi provisioning (AP mode on first boot, STA mode after setup)
├── OTA update receiver (/update endpoint)
├── Basic sensor reading (SHTC3, TSL2591)
├── I2C scan on boot — detects connected devices
├── Offline webapp (served from LittleFS)
└── NVS storage for WiFi credentials and device settings
```

### First Boot Experience (no internet required)

1. User powers Brain Board for the first time
2. Board creates its own WiFi access point: Automato-XXXX
3. User connects phone or laptop to Automato-XXXX
4. Opens http://192.168.4.1 in browser
5. Full Automato webapp loads directly from the board
6. User sees live sensor readings immediately
7. User enters their WiFi credentials and optionally sets Automato Network Name + password
8. Board joins their network, accessible at http://brainboard.local
9. If internet available — board registers with automato.ag via derived cloud token

### After First Boot

- Board always accessible at http://brainboard.local on local network
- Offline webapp always available regardless of internet connectivity
- automato.ag enhances the experience when internet is available
- OTA updates can be pushed from automato.ag or uploaded manually

---

## Flash Memory Layout (4MB)

```
4MB Flash
├── Base firmware code         ~500KB
├── OTA partition (copy)       ~500KB   (required for safe OTA)
├── LittleFS (webapp + files)  ~2MB     (webapp HTML/CSS/JS lives here)
└── NVS (settings, rules)      ~256KB
```

---

## Firmware Version Roadmap

| Version | Scope | Status |
|---------|-------|--------|
| v0.8.1 | Tab nav shell, I2C Scanner tab, `/i2c-scan` endpoint | ✅ complete |
| v0.9 | Devices tab + Automato Network Name in `/setup` | next |
| v1.0 | Rules tab + Settings tab + plugin hooks + recipe database | target |
| v1.1 | ESP-Mesh-Lite multi-board mesh | post-v1.0 |

### v0.9 Scope

**Devices tab:**
- Reuses `/i2c-scan` + `/data` — no new firmware endpoints needed
- Unknown device badge on any device not in the built-in or user database
- User definition form: name, description, category, datasheet URL, hub flag
- `/user_devices.json` stored in LittleFS — portable, designed for future mesh/cloud sync

**`/setup` additions:**
- Automato Network Name field (auto-generated default, user-editable)
- Password field (strongly suggested, not required)
- Individual board name field remains unchanged (already in NVS)

### v1.0 Scope

- All five tabs functional (Dashboard, Devices, Rules, Settings)
- Devices tab: remote board I2C topology via ESP-NOW scan request/response
- Plugin hook architecture (see below)
- Curated recipe database: 20–30 most common DIY sensors pre-defined in webapp
- Stable REST API — no breaking changes after v1.0

### v1.0 Shippability Checklist

- [ ] All five tabs functional
- [ ] Devices tab: remote board I2C scan (ESP-NOW request → response → displayed in webapp)
- [ ] Plugin hook architecture implemented (`customSetup`, `customLoop`, `customDataJSON`)
- [ ] Curated device recipe database in webapp (novice one-click apply)
- [ ] `/setup` includes Automato Network Name + password fields
- [ ] Hardware auto-detection on boot, results in `/data` JSON
- [ ] Stable documented API — no breaking changes after v1.0
- [ ] Pre-built `.bin` on GitHub — flash without Arduino IDE
- [ ] OTA update flow verified end-to-end
- [ ] OTA instructions in QuickStart.md

---

## Plugin Hook Architecture (v1.0)

Intermediate users can add any Arduino library and custom sensor support without
touching the base firmware. Arduino concatenates all `.ino` files in a sketch folder
at compile time.

Base firmware defines weak hook functions — user implements them in `custom.ino`:

```cpp
// In BrainBoard_Host.ino — user never modifies this file
void __attribute__((weak)) customSetup() {}
void __attribute__((weak)) customLoop() {}
String __attribute__((weak)) customDataJSON() { return "{}"; }
```

`customDataJSON()` return value is merged into the `/data` response — custom sensor
values appear in the dashboard and are available to the rule engine automatically.

When base firmware updates: user replaces `BrainBoard_Host.ino` only. `custom.ino` is untouched.

Phase 2 cloud compile generates `custom.ino` from library templates — same architecture,
no friction for novice users.

---

## Unrecognized I2C Device Usability

| Level | User | Mechanism | Available |
|-------|------|-----------|-----------|
| 0 | Novice | Built-in recipe database — one-click apply | v1.0 |
| 1 | Novice | Community recipe database — search and apply | Phase 3 |
| 2 | Intermediate | `custom.ino` plugin hook + Arduino library | v1.0 |
| 3 | Advanced | Full firmware fork | Always |

Novices with devices not in any database cannot use them until Level 0 or 1 coverage
exists. Document this clearly in user-facing materials.

---

## Platform Roadmap (automato.ag)

### Phase 0 — automato.ag/agridata Public Page

Goal: Standalone public agricultural data service — no Brain Board required, no accounts.

Features:
- All Agri Data sidebar datasets (weather, solar, moon, soil, AQI, alerts) + additional sources
- Interactive data graphs and charts
- Visual map viewer: zoom/pan, project API data as overlays
- Map layers: topographic, watershed, soil classification, and other relevant agricultural layers
- Available to anyone as a free public service

Why Phase 0: Builds brand awareness and demonstrates Automato's agricultural focus before a Brain Board is in hand. Entirely independent of Brain Board development — can be built in parallel.

### Phase 1 — Cloud Relay (Remote Viewing)

Goal: User can view their board's sensor data from any network, without user accounts.

Components:
- Board pushes sensor readings to automato.ag on interval (HTTP POST)
- Server stores latest readings in ephemeral relay cache keyed by cloud token
- automato.ag dashboard fetches and displays readings for the matching token
- NAT traversal: board maintains persistent outbound WebSocket — enables
  automato.ag to send commands (relay control, settings) back to the board
- No user accounts, no email, no registration

User flow:
1. User sets Automato Network Name + password in `/setup`
2. Board derives cloud token locally and registers with automato.ag
3. User visits automato.ag, enters Network Name + password
4. automato.ag derives same token, fetches their board's data
5. Dashboard shows live readings from any network, any device

### Phase 2 — Cloud Compile

Goal: User never needs Arduino IDE after first flash.

Components:
- Arduino CLI hosted on automato.ag server
- Automato board package installed on server
- Monaco Editor (VS Code engine) embedded in automato.ag
- Pre-made `.ino` template library (relay control, soil monitoring, etc.)
- Compile API: accepts `.ino` + `custom.ino`, returns compiled `.bin`
- Browser acts as relay: downloads `.bin` from automato.ag, pushes to board
  on local network via `/update` endpoint

Limitation: Browser must be open on the same local network as the board for OTA.

### Phase 3 — Community Device Recipe Database

Goal: Novice users can use any device another community member has already defined.

Components:
- Public device recipe search on automato.ag
- Community submission flow — no account required (GitHub PR model or anonymous submit)
- Recipes are public domain, attributed but not locked to a user
- Automato curates and validates submissions

---

## Three-Tier Automation Architecture

```
Tier 1 — Cloud (automato.ag)
  Full rule evaluation on server
  All data sources available (sensor data + Agri Data + weather)
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
  Relay defaults to failsafe state if no rules match
```

Failover: Tiers activate downward automatically on loss of higher tier.
Recovery: Board resumes highest available tier when connectivity restored.
Heartbeat: Configurable timeout — if no signal from higher tier within X seconds, Tier 3 takes over.

---

## Rule Engine Design (all tiers)

- Each rule has: name, priority (1=highest), logic operator, target relay(s), conditions
- Conditions compare sensor values: above / below / equals threshold
- Logic operators between conditions: AND, OR, NOT, XOR
- Conflict resolution: highest priority rule wins
- If no rules fire: relay defaults to hardware-defined failsafe state
- Manual dashboard toggle always overrides all rules regardless of priority
- Multiple rules can target the same relay — priority resolves conflicts

---

## Relay Safety Contract

Outputs default to the **hardware-defined failsafe state** under all failure conditions.
This includes: boot, sensor failure, WiFi loss, cloud loss, browser closed,
no rules configured, conflicting rules, and hardware expander not found.

**Failsafe state is hardware-selectable per output board** (next PCB revision and all
future output boards): a physical jumper selects Normally ON or Normally OFF.
A GPIO sense pin reads the jumper position at boot. The dashboard displays the
active failsafe mode prominently — amber warning if Normally ON.

Current Brain Board V2.0: failsafe = Normally OFF (hardcoded, no jumper on current hardware).

Rationale: Agricultural failure modes are asymmetric. A heater should default ON
(frost protection). A flood pump should default OFF. The installer, not the firmware,
makes this decision.

---

## Multi-Board Networking

### Current Architecture (ESP-NOW Star Topology)

```
Router <-> Host Board <-> Remote 1
                     <-> Remote 2
```

All remote boards must be within ESP-NOW LR range of the host.
Fixed roles: user designates board closest to router as Host.

### Planned Architecture (ESP-Mesh-Lite) — target v1.1

```
Router <-> Board 1 <-> Board 2 <-> Board 3
                              <-> Board 4
```

Any board only needs to reach its nearest neighbour. Self-forming and self-healing.
Dynamic root election — board closest to router becomes root automatically.
Boards identified by Automato Network Name — same name = same mesh.

**Explicitly listed by Espressif as a smart agriculture target use case.**

| Parameter | Value |
|---|---|
| Maximum layers | 15 (5-6 recommended) |
| Max connections per node | 10 hardware, 6 recommended |
| Practical network size | 100–500 nodes |
| Node-to-node distance | <100m stable, ~170m low throughput |
| Self-forming / self-healing | Yes |
| Arduino compatibility | To be verified before implementation |

**Action required before v1.1:** Verify ESP-Mesh-Lite Arduino framework compatibility.

---

## Known Decisions and Rationale

| Decision | Rationale |
|---|---|
| TCA9534 address is 0x27 (not 0x20) | SparkFun Qwiic GPIO has all address jumpers bridged by default |
| ESP-NOW LR on AP interface only | Setting LR on STA before WiFi connects breaks connection |
| Highest priority rule wins conflicts | Industry standard. Enables safety override rules. |
| Failsafe state is jumper-selectable (next revision) | Agricultural failure modes are asymmetric — installer decides, not firmware |
| Browser-side external API calls | Keeps firmware lean; no API keys stored on device |
| Phase 1 platform = cloud relay, not cloud compile | Remote viewing benefits every user; cloud compile is a developer feature |
| No user accounts on automato.ag | Privacy-first; cloud identity derived from Network Name + password locally |
| Monaco Editor for cloud IDE | Same engine as VS Code — familiar, powerful, well maintained |
| Base firmware pre-installed at shipping | Users functional out of box, no tools required |
| Offline webapp on LittleFS | Updatable independently of firmware; scales beyond PROGMEM limits |
| AP mode on first boot | True zero-infrastructure setup — works anywhere, no router required |
| Stem cell architecture | Stable base + specialised OTA overlays = safe, flexible, maintainable |
| Plugin hook via weak functions + custom.ino | Intermediate users extend without forking; base firmware updates are non-breaking |
| MicroSD slot on next Brain Board revision | IO18–IO21 are unconnected on V2.0; ~$0.40–$0.60 BOM; enables always-on local logging |
| EX-01 is optional, not required | Only needed for >8 board types or >1 of same type per bus |
| Third-party I2C devices are first-class | Automato is explicitly non-proprietary |

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
| v0.6.1 | TCA9534 address fix (0x20 → 0x27) |
| v0.7 | OTA firmware update + LittleFS migration |
| v0.8 | WiFi provisioning, captive portal, mDNS, channel scan |
| v0.8.1 | Tab nav shell, I2C Scanner tab, `/i2c-scan` endpoint ✅ |
| v0.9 | Devices tab + Automato Network Name in `/setup` |
| v1.0 | All five tabs, plugin hooks, recipe database — stable, shippable |
| v1.1 | ESP-Mesh-Lite multi-board mesh |

---

## ESP32-C6 Unexplored Capabilities

The Brain Board's ESP32-C6 contains hardware features not yet used in current firmware.
Full details in [`docs/ESP32C6_Capabilities.md`](ESP32C6_Capabilities.md).

| Capability | Agricultural Relevance | Roadmap Status |
|---|---|---|
| LP (Low-Power) Co-Processor | Run Tier 3 rules while HP core sleeps — enables battery deployment | Future |
| Wi-Fi 6 TWT | Scheduled radio wake windows — extends battery life on remote nodes | Future |
| Bluetooth 5 LE | Phone-based provisioning, BLE sensor beacon, WiFi-down fallback | Planned |
| Zigbee 3.0 / Thread 1.3 | Alternative mesh transport for large deployments | Under consideration |
| Die Temperature Sensor | MCU health diagnostic, zero additional hardware | Done (v0.8.1) |
| Hardware Crypto Accelerators | Practical HTTPS to automato.ag, secure boot for production | Phase 1 |
| Hardware PWM | Richer LED status, buzzer, motor/dimmer control | Future |
| Hardware Pulse Counter (PCNT) | Flow meters, anemometers, rain gauges | Future |

Note: LP core, TWT, and 802.15.4 require ESP-IDF, not the Arduino framework.
Arduino is used now for development speed. ESP-IDF migration for specific
features is a future consideration.
