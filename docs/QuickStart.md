# Quick Start Guide

## What You Need

- 2× Automato Brain Board V2.0
- USB-C cables for both boards
- A 2.4 GHz WiFi network (WPA2)
- Arduino IDE 2.x with the Automato board package installed

---

## Step 1 — Install Libraries

Open Arduino IDE → **Tools → Manage Libraries** and install:

- `Adafruit SHTC3 Library`
- `Adafruit TSL2591`
- `SparkFun TCA9534` *(for relay control — install even if you don't have the hardware yet)*

Accept all dependency installs (Adafruit BusIO, Adafruit Unified Sensor).

---

## Step 2 — Flash Board 1 (Host)

1. Connect Board 1 via USB-C
2. Select **Automato Brain Board V2.0** in Arduino IDE
3. Open `firmware/BrainBoard_Host/BrainBoard_Host_v08/BrainBoard_Host_v08.ino`
4. Click **Upload**
5. Open **Serial Monitor** at **115200 baud** and press **RESET**

---

## Step 3 — Provision WiFi

On first boot (or after a credential reset), Board 1 enters provisioning mode.

1. On your phone or laptop, connect to the WiFi network **`Automato-XXXX`** (where XXXX is the last 4 characters of the board's MAC address)
2. A setup page should open automatically. If not, navigate to **`http://192.168.4.1`**
3. Enter your home WiFi **SSID** and **password**
4. Optionally set a **board name** (used as the mDNS hostname — letters, numbers, spaces, hyphens only)
5. Click **Save & Connect**
6. The board will reboot and join your WiFi network

> 💡 Credentials are stored in NVS and survive reboots and OTA updates. You only need to do this once.

---

## Step 4 — Find Board 1 on Your Network

After provisioning, Board 1 is reachable at:

- **`http://boardname.local`** — if you set a board name (e.g. `http://greenhouse.local`)
- **`http://automato-XXXX.local`** — if no board name was set
- **`http://<IP address>`** — IP is printed in Serial Monitor at startup

---

## Step 5 — Flash Board 2 (Remote)

1. Open `firmware/BrainBoard_Remote/BrainBoard_Remote_v05/BrainBoard_Remote_v05.ino`
2. Find Board 1's MAC address — it's printed in Serial Monitor at startup
3. Paste it into the sketch:

```cpp
uint8_t HOST_MAC_ADDRESS[] = {0xE4, 0xB3, 0x23, 0x89, 0x7E, 0x20};
```

4. Connect Board 2 via USB-C and upload the sketch
5. Open Serial Monitor — you should see Board 2 scanning channels and locking on:

```
Scanning channels 1–13 for Board 1...
  Channel 1... no response.
  Channel 6... ACK! Locking to channel 6.
Ready. Sending every 3000 ms on channel 6.
```

> Board 2 automatically finds the correct WiFi channel. If the channel ever changes, it re-scans after 30 seconds of silence.

---

## Step 6 — Open the Dashboard

Navigate to Board 1's address in any browser on the same WiFi network.

You should see:
- **Board 1** sensor readings (live)
- **Board 2** sensor readings with link status badge
- **Relay Control** panel in the sidebar (if Qwiic GPIO is connected)
- **Agri Data** sidebar (enter a location to load external data)

---

## Relay Control (Optional)

Requires a **SparkFun Qwiic GPIO (TCA9534)** connected to Board 1's Qwiic port.

1. Connect Qwiic GPIO to either Qwiic port on Board 1
2. Wire relay board IN to Qwiic GPIO pin 0
3. Power relay board from USB hub (5V)
4. Reset Board 1 — TCA9534 will be detected automatically
5. Use ON/OFF buttons in the dashboard Relay Control panel

If the Qwiic GPIO is not connected, a warning banner appears but everything else works normally.

---

## Resetting WiFi Credentials

To clear stored credentials and re-enter provisioning mode:

1. With Board 1 connected to power, press and **hold the BOOT button (IO9)**
2. While holding BOOT, press and release **RESET**
3. Keep holding BOOT for **5–7 seconds** after the reset
4. Release — Board 1 will clear its credentials and restart in provisioning mode

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `Automato-XXXX` WiFi not visible | Board may be connected to a saved network. Do a credential reset (see above). |
| Setup page doesn't open automatically | Navigate manually to `http://192.168.4.1` while connected to `Automato-XXXX`. |
| `boardname.local` not resolving | mDNS can be unreliable on some Windows networks. Use the IP address instead (printed in Serial Monitor). |
| Board 2 stuck scanning channels | Confirm Board 1 is powered and running v0.8. Confirm the MAC address in the Remote sketch is correct. |
| Board 2 shows "Signal lost" | Board 2 may be powered off or out of range. Stale timeout is 15 seconds. Board 2 will re-scan channels automatically. |
| Relay GPIO warning showing | TCA9534 not detected on I2C. Check Qwiic cable seating. |
| Dashboard won't load | Confirm you're on the same WiFi network as Board 1. |
