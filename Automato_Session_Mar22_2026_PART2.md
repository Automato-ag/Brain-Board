# Automato Session — March 22, 2026 (Part 2)

## Session Overview

This session began by committing the remaining v0.9 changes from Part 1, then pivoted into a deep architectural research and planning session covering mesh networking options, board mobility, multi-hop relay design, and a full roadmap revision. No new firmware or webapp code was written — this was a strategy and architecture session.

---

## Commits Made

**Commit:** `v0.9: Automato Network Name, WiFi reset UX, Network Name in Dashboard`
- `/setup` form: Network Name + Network Password fields, NVS storage (netname, netpass)
- Auto-generated default name (`automato-XXXX`) from MAC suffix if NVS empty
- `/version` JSON: added `networkName` field
- WiFi reset flow: shows step-by-step reconnection instructions with `http://192.168.4.1/setup`
- Setup banner href changed from `/setup` to `http://192.168.4.1/setup`
- Dashboard WiFi status: displays Network Name below mDNS line

**User preference established:** Never include `Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>` in commit messages. Saved to feedback memory.

---

## Research: Mesh Networking Options for ESP32-C6

A full comparative analysis was done before deciding on the v1.1 mesh approach. Results:

| Option | Verdict | Reason |
|---|---|---|
| ESP-Mesh-Lite | ❌ | ESP-IDF 5.x only — no Arduino. Patches ESP-IDF at build time. Can't link from Arduino sketch without patched board package. |
| painlessMesh | ❌ | Requires arduino-esp32 v2.0.x; ESP32-C6 requires v3.x. Mutually exclusive. |
| zh_network | ❌ | ESP32-C6 unverified; tested on PlatformIO only, not Arduino IDE. 218-byte max payload. |
| Thread / OpenThread | ⏳ | ESP32-C6 hardware supports 802.15.4 natively. But requires ESP-IDF, needs Border Router. Deferred to v2.0+. |
| Custom ESP-NOW | ✅ | Arduino-native, verified on ESP32-C6, no external dependencies, stable indefinitely. |

### Could ESP-Mesh-Lite live in a separate layer?

The architectural concept is sound — pre-compile ESP-Mesh-Lite as a library layer, expose a clean API to Arduino sketches. Theoretically achievable via Path A: apply ESP-Mesh-Lite patches to the ESP-IDF build inside the `automato-arduino` custom board package.

**Why not now:** Each upstream release of arduino-esp32 or ESP-Mesh-Lite requires a full ESP-IDF rebuild (~2–4 hours CI), patch conflict resolution, and hardware smoke test. For a small team this is unsustainable. Revisit when bandwidth exists.

### The "never needs an update" principle

External mesh libraries introduce permanent update dependencies. The most robust firmware owns everything it depends on. Custom ESP-NOW code, once written, is stable indefinitely regardless of what Espressif ships.

---

## Key Architectural Decisions

### Board Mobility Is the Core Use Case

Boards are NOT fixed-installation devices. They move with:
- Plants transplanted from germination to garden bed
- Pots relocated indoors/outdoors seasonally
- Garden beds rearranged between harvests

**Fixed Host/Remote roles are the wrong model.** They require manual reconfiguration every time a board moves. This is unacceptable.

### Every Board Is a Full Peer

All Brain Boards are identical hardware running identical firmware. Every board already has: web server, LittleFS webapp, REST API, NVS rules, sensors, relay control. The "gateway" role is a runtime state, not a hardware assignment.

### ESP-NOW Peer Limits

- Hard limit: **20 peers per board** (`ESP_NOW_MAX_TOTAL_PEER_NUM`) — not configurable
- Star topology max: 1 gateway + 20 direct peers = 21 boards
- With 1 relay tier: ~50–80 boards
- With 2 relay tiers: ~150–200 boards
- Covers all realistic Automato installations

### Multi-Hop Relay — Verified Viable

Scenario confirmed: Board4 moves from Board2's range to Board3's range. Sequence:
1. Board4's MSG_HELLO beacon received by Board3 (new neighbor)
2. Board3 registers Board4 as ESP-NOW peer
3. Board3 propagates routing update to Board1 (gateway)
4. Board1 updates routing table: Board4 reachable via Board3
5. Transition time: 1–3 beacon cycles

**Duplicate-drop mechanism:** Each packet carries `originMac + seq`. Any board that has already forwarded a given combination drops it — prevents routing loops.

**TTL guard:** Packets carry a TTL (default 5). Decrements each hop. Dropped at 0 regardless. Second line of defense against loops.

**Bidirectional:** Sensor data flows toward gateway. Commands (relay toggle, config changes) flow back via the same path in reverse.

---

## Revised Roadmap

| Version | Scope |
|---|---|
| v0.9 | ✅ Complete |
| v1.0 | Rules tab + Settings tab + Network tab (stub) + plugin hooks + recipe database |
| v1.1 | Auto-discovery (MSG_HELLO) + dynamic gateway + `networkname.local` + 1-hop relay + unified firmware |
| v1.2 | Multi-hop relay + routing tables + full peer equality |

### v1.1 Architecture Detail

**MSG_HELLO beacon:**
```cpp
typedef struct {
  uint8_t  type;            // MSG_HELLO (0x03)
  char     networkName[33]; // must match to join
  char     boardName[33];   // individual board identity
  uint8_t  mac[6];          // sender MAC
  bool     hasWiFi;         // does this board have router connectivity?
} HelloPayload;
```
Any board receiving a MSG_HELLO with matching Network Name registers the sender as an ESP-NOW peer automatically. No manual configuration.

**Dynamic gateway election:** Board with active WiFi router connectivity = gateway. If multiple boards have WiFi, highest RSSI wins. Changes automatically. No user action.

**`networkname.local` mDNS:** Gateway advertises Network Name as mDNS hostname. User's bookmark (e.g., `southknox.local`) works regardless of which physical board is currently the gateway. Re-resolves within ~60 seconds of a gateway change.

**Graceful degradation:**
| State | Behavior |
|---|---|
| On WiFi | Reachable at `boardname.local`, visible in network dashboard |
| ESP-NOW only (1 hop) | Data relayed through nearest peer, visible in dashboard |
| Completely isolated | Tier 3 rules run; rejoins automatically when back in range |

**Unified firmware:** `BrainBoard_Host` and `BrainBoard_Remote` merge into a single `BrainBoard` firmware. Do not invest in extending `BrainBoard_Remote` — it will be absorbed in v1.1.

### v1.2 Architecture Detail

**Packet structure:**
```cpp
typedef struct {
  uint8_t  type;
  uint8_t  originMac[6];  // for duplicate-drop
  uint16_t seq;           // per-board sequence number
  uint8_t  ttl;           // decrements per hop; drop at 0
  uint8_t  payload[];
} MeshPacket;
```

Routing tables per board, updated within 1–3 beacon cycles when topology changes.

---

## v1.0 Decisions Confirmed

**Rules tab:**
- Local board sensors only in v1.0
- Cross-board sensor references require mesh peer identity (v1.2)
- Rule engine structs already written in firmware — `evaluateRules()` commented out of `loop()`, ready to activate

**Settings tab contents (v1.0):**
- Board name (individual identity)
- Network Name + password (mirrors `/setup` — editable without knowing the `/setup` URL)
- WiFi credentials reset
- Start with these; expand as capabilities grow

**Network tab:**
- Stub in v1.0 — shows single-board connectivity status and current role
- Visual peer map (all boards, roles, relay paths) in v1.1

**Role indicator:**
- Visible on every board's dashboard
- v1.0: Host / Remote (fixed roles still in use)
- v1.1+: Gateway / Relay / Isolated (dynamic)

---

## Files Updated This Session

- `docs/Roadmap.md` — full rewrite of Multi-Board Networking section; version table updated; Known Decisions expanded with 7 new entries; ESP-Mesh-Lite references removed
- `CLAUDE.md` — v0.9 marked complete; 5-tab structure documented; v1.0/v1.1 scope updated; firmware naming warning added; two new guiding principles added
- `memory/project_architecture.md` — roadmap updated, ESP-NOW/Networking section rewritten with new mesh architecture
- `memory/feedback_working_style.md` — no Co-Authored-By in commits

---

## Open Questions Carried Forward

1. Connector decision — two 6-pin Automato ports, or one 6-pin + one Qwiic?
2. Thread/OpenThread — revisit at v2.0 when ESP-IDF path is clearer
3. Multi-board webapp design — discuss before v1.1 (gateway aggregates all peer dashboards)
4. Cross-board rules UI — how does user specify "Board2 soil moisture" vs "Board1 soil moisture"? Deferred to v1.2 design phase.
5. Historical data logging — SD card firmware + browser File System API — which version?

---

## Next Steps

Build v1.0. Starting options:
- **Rules tab** — firmware: `/rules` GET/POST endpoints, NVS persistence, activate `evaluateRules()` in loop; webapp: rule builder UI
- **Settings tab** — webapp only (LittleFS deploy, no firmware reflash): board name, Network Name, WiFi reset form
- **Network tab stub** — webapp only: connectivity status, role display

Session ended before v1.0 work began.
