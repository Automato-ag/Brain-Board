# Relay Control

## Overview

Brain Board relay control uses a **SparkFun Qwiic GPIO (TCA9534)** I2C expander connected via the Qwiic port, driving an optoisolated relay board. The Brain Board's 3.3V I2C signal is sufficient to trigger the optocoupler on the relay board; the relay coil is powered independently at 5V.

---

## Hardware

| Component | Details |
|---|---|
| GPIO Expander | SparkFun Qwiic GPIO — TCA9534 |
| I2C Address | 0x20 (all address pins LOW) |
| Relay Board | Amazon B0CHFJSNP6 (5V coil, optoisolated, active HIGH) |
| Connection | Qwiic port J2 or J3 on Brain Board |

### Wiring Diagram

```
Brain Board
  Qwiic J2/J3 ──► SparkFun Qwiic GPIO (TCA9534)
                        Pin 0 ──────────────────► Relay Board IN
                        GND ────────────────────► Relay Board GND

USB Hub
  Port A ──► Brain Board USB-C      (3.3V logic)
  Port B ──► Relay Board USB-C      (5V coil power)
  (shared ground via hub — safe, no ground loop)
```

---

## Relay Safety Behavior

The relay **always defaults OFF**. This is enforced at every level:

| Condition | Relay State |
|---|---|
| Board powering up (before init) | OFF |
| TCA9534 not found on I2C | OFF (no-op, warning shown) |
| Sensor read failure | OFF |
| WiFi disconnected | OFF |
| No automation rules active | OFF |
| Manual override active | Follows dashboard toggle |

There is no condition under which a relay defaults ON.

---

## HTTP Endpoints

### `GET /relay?state=1`
Turn relay ON (sets manual override).

**Response:**
```json
{ "ok": true, "manualOverride": true, "state": true }
```

### `GET /relay?state=0`
Turn relay OFF (sets manual override).

**Response:**
```json
{ "ok": true, "manualOverride": true, "state": false }
```

### `GET /relay?override=auto`
Clear manual override — hand control to rule engine (Stage 2+).
Relay defaults OFF until first rule evaluation.

**Response:**
```json
{ "ok": true, "manualOverride": false, "state": false }
```

### `GET /relay/status`
Get current relay state.

**Response:**
```json
{ "state": false, "manualOverride": true, "gpioOk": true }
```

The `/data` endpoint also includes a `relay` object on every poll, keeping the dashboard in sync automatically.

---

## Automation Architecture (Roadmap)

Relay control is designed in three tiers:

| Tier | Location | Requires | Status |
|---|---|---|---|
| 1 — Cloud | automato.ag server | Internet | Planned |
| 2 — Browser | Dashboard tab | Local network + open browser | Planned |
| 3 — On-board | Brain Board NVS | Nothing (fully offline) | Next (Stage 2) |

**Failover order:** Cloud → Browser → On-board offline rules

### Rule Engine Design

Already stubbed in firmware (`BrainBoard_Host_v0.6.ino`):

- Each rule has a name, priority, logic operator, target relay(s), and up to 16 conditions
- Conditions compare sensor values (above/below/equals threshold)
- Logic operators: AND, OR, NOT, XOR
- **Conflict resolution: highest priority rule wins** (priority 1 = highest)
- If no rules fire, relay defaults OFF
- Manual dashboard toggle always overrides all rules regardless of priority

The `evaluateRules()` function is present but commented out in `loop()` — it activates in Stage 2.

---

## Dashboard UI

The **Relay Control** panel is a collapsible section at the top of the sidebar.

- **Dot indicator** — amber pulsing = ON, grey = OFF (visible without opening panel)
- **State badge** — ON / OFF with color coding
- **Mode badge** — Manual / Auto
- **ON / OFF buttons** — manual control
- **GPIO warning** — appears if TCA9534 not detected on I2C bus
