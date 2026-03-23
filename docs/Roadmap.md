# Automato Brain Board — Product Roadmap

Last updated: 2026-03-23

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
| v0.9 | Devices tab + remote I2C scan via ESP-NOW + Automato Network Name | ✅ complete |
| v1.0 | Settings tab (functional) + Network tab (stub) + `POST /settings` endpoint | ✅ complete |
| v1.1 | ESP-NOW mesh: MSG_HELLO beacon, unified firmware, dynamic gateway, `networkname.local`, 1-hop relay, live Network tab peer map + plugin hooks | next |
| v1.2 | Rules tab: firmware rule engine, LittleFS rule storage, local + peer sensor conditions, relay actions + recipe database | post-v1.1 |
| v1.3 | Multi-hop relay + routing tables + full peer equality + cross-board rule conditions | post-v1.2 |

### v0.9 Scope — Complete ✅

**Devices tab (replaces I2C Scanner tab):**
- Host I2C scan via `/i2c-scan` endpoint
- Remote board I2C scan via ESP-NOW (`MSG_SCAN_REQUEST` / `MSG_SCAN_RESPONSE`)
- User device definition form — name and description stored in localStorage by address
- Collapsible I2C address map (full 0x08–0x77 grid)
- 5 tabs present: Dashboard · Devices · Rules · Settings · Network

**`/setup` additions:**
- Automato Network Name field — auto-generated default (`automato-XXXX`), user-editable
- Password field — stored in NVS
- Network Name exposed in `/version` JSON as `networkName`
- Network Name displayed in Dashboard WiFi status area

**WiFi reset UX:**
- Reset flow shows step-by-step reconnection instructions with AP SSID and `http://192.168.4.1/setup` link

### v1.0 Scope — Complete ✅

- Settings tab: board name, Network Name + password, WiFi reset — pre-fills from `/version` on open
- Network tab (stub): HOST/REMOTE role badge, board name, mDNS address, Network Name, WiFi status; "Coming in v1.1" section explains auto-discovery
- `POST /settings` endpoint: saves board name, Network Name, and password to NVS without requiring WiFi credentials (unlike `POST /setup` which requires `ssid`)
- Stable 5-tab layout established: Dashboard · Devices · Rules · Settings · Network

### v1.1 Scope — Next

- **Unified firmware:** `BrainBoard_Host` and `BrainBoard_Remote` merge into single `BrainBoard` firmware — all boards are full peers
- **MSG_HELLO beacon:** boards broadcast Network Name + board name + MAC + hasWiFi on startup and periodically; matching boards auto-register as ESP-NOW peers
- **Dynamic gateway election:** board with active WiFi router connection = gateway; changes automatically when boards move; strongest RSSI wins if multiple candidates
- **`networkname.local` mDNS:** gateway advertises Network Name as mDNS hostname (e.g., `southknox.local`); user bookmark works regardless of which board is gateway
- **1-hop relay:** boards out of WiFi range relay sensor data through nearest ESP-NOW peer; commands relay back the same path
- **Network tab (live):** peer map showing all discovered boards, their roles, relay paths, and WiFi status
- **Plugin hook architecture:** `customSetup()`, `customLoop()`, `customDataJSON()` weak functions in base firmware; user adds `custom.ino` to sketch folder

### v1.1 Shippability Checklist

- [ ] Unified firmware — single `BrainBoard.ino` replaces both Host and Remote
- [ ] MSG_HELLO beacon implemented and tested (2+ boards auto-discover)
- [ ] Dynamic gateway election tested (gateway transfers when board moves)
- [ ] `networkname.local` mDNS verified from gateway board
- [ ] 1-hop relay verified (board out of WiFi range, data reaches dashboard)
- [ ] Network tab: live peer map populated from beacon data
- [ ] Plugin hook architecture implemented and documented

### v1.2 Scope

- Rules tab: full firmware rule engine, LittleFS rule storage, `/rules` GET/POST endpoints
- Rule conditions: local board sensors (temp, humidity, light); peer board sensors (using peer MAC identity established in v1.1)
- Rule actions: local relay (TCA9534); future output boards (AC-01, DC-01–04) as they ship
- Rule schema designed for forward compatibility — `"source": "local"` in v1.2 becomes `"source": "peer:<mac>"` without migration
- Curated recipe database: 20–30 most common DIY sensors pre-defined in webapp (used for device naming in Devices tab and referenced in Rules conditions)
- Stable documented REST API — no breaking changes after v1.2

### v1.3 Scope

- Multi-hop relay + routing tables (see Multi-Board Networking section below)
- Cross-board rule conditions referencing boards 2+ hops away
- Full peer equality — no architectural distinction between any boards

---

## Plugin Hook Architecture (v1.1)

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
| 0 | Novice | Built-in recipe database — one-click apply | v1.2 |
| 1 | Novice | Community recipe database — search and apply | Phase 3 |
| 2 | Intermediate | `custom.ino` plugin hook + Arduino library | v1.1 |
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

### Design Principle: Boards Are Mobile

Brain Boards follow plants, seasons, and growing conditions. A board monitoring
germinating seedlings moves outside when transplanted. A board in a pot moves
with the pot. A board is repositioned between garden beds each season.

The architecture must accommodate this. Fixed Host/Remote roles require manual
reconfiguration every time a board moves. That is unacceptable.

**Every Brain Board is a full peer.** All boards run identical firmware with identical
capabilities: web server, webapp, REST API, NVS rules, sensors, relay control.
The "gateway" role (the board with active router connectivity that serves the
network dashboard) is determined dynamically — not assigned by the user.

### Current State (v0.9) — Fixed Roles, Manual Configuration

```
Router <─WiFi─> Board 1 (Host) <─ESP-NOW─> Board 2 (Remote)
                               <─ESP-NOW─> Board 3 (Remote)
```

Boards within ESP-NOW range of Board 1 only. Fixed roles. Remote MAC addresses
must be known in advance. Board 1 must remain closest to router.

### v1.1 Target — Auto-Discovery + Dynamic Gateway + 1-Hop Relay (next)

```
Router <─WiFi─> Gateway <─ESP-NOW─> Board 2
                        <─ESP-NOW─> Board 3
                        <─ESP-NOW─> Board 4 (out of WiFi range, relayed)
```

**Network Name beacon (MSG_HELLO):**
Every board broadcasts a small beacon packet on startup and periodically:
```cpp
typedef struct {
  uint8_t  type;            // MSG_HELLO
  char     networkName[33]; // must match to join
  char     boardName[33];   // individual board identity
  uint8_t  mac[6];          // sender MAC
  bool     hasWiFi;         // does this board have router connectivity?
} HelloPayload;
```
Any board that receives a `MSG_HELLO` with a matching Network Name registers
the sender as an ESP-NOW peer automatically. No manual MAC entry. No roles to assign.

**Dynamic gateway election:**
Whichever board has active WiFi router connectivity becomes the gateway.
If multiple boards have WiFi, the one with the strongest RSSI wins.
If the current gateway loses WiFi, another connected board takes over within
one beacon cycle. No user action required.

**`networkname.local` mDNS:**
The gateway advertises the Automato Network Name as an mDNS hostname
(e.g., `southknox.local`). The user's browser bookmark works regardless of
which physical board is the current gateway. mDNS TTL is short — re-resolves
automatically within ~60 seconds of a gateway change.

**Individual board access:**
Every board on WiFi remains accessible at `boardname.local` independently.
If a board moves out of mesh range but reconnects to WiFi, it is immediately
reachable at its own address.

**1-hop relay:**
Boards out of WiFi range but within ESP-NOW range of the gateway (or another
WiFi-connected board) relay their sensor data through that board. Commands
from the dashboard relay back the same path.

**Graceful degradation when a board moves:**

| Board state | Behaviour |
|---|---|
| On WiFi | Reachable at `boardname.local`, appears in network dashboard |
| ESP-NOW range only (1 hop) | Data relayed through nearest peer, visible in dashboard |
| Completely isolated | Tier 3 rules run independently; rejoins automatically when back in range |

### v1.3 Target — Multi-Hop Relay + Routing Tables

```
Router <─WiFi─> Board 1 <─ESP-NOW─> Board 2 <─ESP-NOW─> Board 4
                        <─ESP-NOW─> Board 3 <─ESP-NOW─> Board 4 (after move)
```

Board 4 moves from Board 2's range to Board 3's range. The routing table
updates within one beacon cycle. Board 4 remains visible in the dashboard
throughout, briefly showing "reconnecting" during the transition.

**Packet structure:**
```cpp
typedef struct {
  uint8_t  type;
  uint8_t  originMac[6]; // originating board — used for duplicate-drop
  uint16_t seq;          // sequence number per originating board
  uint8_t  ttl;          // decrements each hop; dropped at 0
  uint8_t  payload[];
} MeshPacket;
```

**Duplicate-drop:** A board that has already forwarded a given `originMac + seq`
combination drops it immediately. Prevents routing loops.

**TTL guard:** Packets dropped after N hops regardless. Default TTL = 5
(supports a chain of 5 boards — more than any realistic Automato installation).

**Bidirectional relay:** Sensor data flows toward the gateway. Commands
(relay toggle, settings change) flow from the gateway back to the target board
via the same routing path in reverse.

**Routing table updates:** When a board's beacon is received by a new neighbor,
that neighbor propagates a routing update toward the gateway. All intermediate
boards update their tables. Convergence time: 1–3 beacon cycles.

### Network Scale Limits

| Parameter | Value | Notes |
|---|---|---|
| Max ESP-NOW peers per board | 20 | Hard limit in ESP-NOW stack |
| Max boards in direct star | 21 | 1 gateway + 20 direct peers |
| Max boards with 1 relay tier | ~50–80 | Practical limit with routing overhead |
| Max boards with 2 relay tiers | ~150–200 | Well beyond any realistic installation |
| Max hops | 5 (TTL default) | Configurable |
| Transition time after board moves | 1–3 beacon cycles | Seconds to ~1 minute |

For virtually all Automato installations (2–15 boards), no limit is approached.

### Why Not ESP-Mesh-Lite or Other Mesh Libraries

| Option | Verdict | Reason |
|---|---|---|
| ESP-Mesh-Lite | ❌ | ESP-IDF 5.x only — no Arduino framework support. Would require abandoning Arduino IDE, breaking the plugin hook architecture and webapp-only deploy workflow. |
| painlessMesh | ❌ | Requires arduino-esp32 v2.0.x; ESP32-C6 requires v3.x. Mutually exclusive. |
| zh_network | ❌ | ESP32-C6 unverified; tested on PlatformIO only, not Arduino IDE. |
| Thread/OpenThread | ⏳ | Correct protocol for large IoT mesh; ESP32-C6 hardware supports it. Deferred — requires ESP-IDF, steep ramp-up, needs a Border Router. Revisit at v2.0. |
| Custom ESP-NOW | ✅ | Arduino-native, fully verified on ESP32-C6, no external dependencies, no maintenance treadmill, right-sized for Automato's actual use case. |

**The fundamental constraint:** any external mesh library introduces an update
dependency. When the library or its underlying ESP-IDF changes, firmware must
update or break. The most robust firmware owns everything it depends on.
Custom ESP-NOW code, once written, is stable indefinitely.

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
| No external mesh library | External libraries introduce update dependencies — firmware must update when library or underlying IDF changes. Custom ESP-NOW code is stable indefinitely. |
| Every board is a full peer | Boards move with plants and seasons. Fixed Host/Remote roles require manual reconfiguration on every move. Dynamic gateway election eliminates this friction. |
| Dynamic gateway election | Whichever board has active WiFi router connectivity becomes the gateway. Changes automatically when boards move. User's bookmark (`networkname.local`) never breaks. |
| `networkname.local` mDNS | Gateway advertises Network Name as mDNS hostname. User always reaches the network at the same address regardless of which physical board is currently the gateway. |
| Multi-hop via custom relay (v1.3) | Boards may be out of WiFi range but within ESP-NOW range of another board. TTL + message-ID duplicate-drop prevents routing loops. Routing tables update within 1–3 beacon cycles when boards move. |
| Board mobility is a first-class use case | Boards follow plants, pots, and seasons. Architecture must self-organize around this without requiring user reconfiguration. |

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
| v0.9 | Devices tab, remote I2C scan via ESP-NOW, Automato Network Name | ✅ |
| v1.0 | Settings tab, Network tab stub, `POST /settings` endpoint | ✅ |
| v1.1 | Unified firmware, MSG_HELLO auto-discovery, dynamic gateway, `networkname.local`, 1-hop relay, plugin hooks |
| v1.2 | Rules tab, firmware rule engine, recipe database — stable, shippable |
| v1.3 | Multi-hop relay, routing tables, full peer equality, cross-board rules |
| v1.3 | Multi-hop relay, routing tables, full peer equality, cross-board rules |

---

## ESP32-C6 Unexplored Capabilities

The Brain Board's ESP32-C6 contains hardware features not yet used in current firmware.
Full details in [`docs/ESP32C6_Capabilities.md`](ESP32C6_Capabilities.md).

| Capability | Agricultural Relevance | Roadmap Status |
|---|---|---|
| LP (Low-Power) Co-Processor | Run Tier 3 rules while HP core sleeps — enables battery deployment | Future |
| Wi-Fi 6 TWT | Scheduled radio wake windows — extends battery life on remote nodes | Future |
| Bluetooth 5 LE | Phone-based provisioning, BLE sensor beacon, WiFi-down fallback | Planned |
| Zigbee 3.0 / Thread 1.3 | Alternative mesh transport for large deployments (>50 boards, requires ESP-IDF) | Deferred to v2.0+ |
| Die Temperature Sensor | MCU health diagnostic, zero additional hardware | Done (v0.8.1) |
| Hardware Crypto Accelerators | Practical HTTPS to automato.ag, secure boot for production | Phase 1 |
| Hardware PWM | Richer LED status, buzzer, motor/dimmer control | Future |
| Hardware Pulse Counter (PCNT) | Flow meters, anemometers, rain gauges | Future |

Note: LP core, TWT, and 802.15.4 require ESP-IDF, not the Arduino framework.
Arduino is retained permanently for the plugin hook architecture (`custom.ino`),
webapp-only deploy workflow, and DIY-first accessibility. ESP-IDF features that
require abandoning Arduino are deferred until a migration path exists that
preserves these properties.
