# Quick Start Guide

## What You Need

- 2× Automato Brain Board V2.0
- USB-C cables for both boards
- A 2.4 GHz WiFi network (WPA2)
- Arduino IDE 2.x
- The Automato board package installed

---

## Step 1 — Install Libraries

Open Arduino IDE → **Tools → Manage Libraries** and install:

- `Adafruit SHTC3 Library`
- `Adafruit TSL2591`
- `SparkFun TCA9534` *(for relay control — install even if you don't have the hardware yet)*

Accept all dependency installs (Adafruit BusIO, Adafruit Unified Sensor).

---

## Step 2 — Configure WiFi Credentials

Open `BrainBoard_Host_v0.6.ino` and edit lines near the top:

```cpp
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";
```

> ⚠️ Never commit real credentials to GitHub. Use `secrets.h` (already in `.gitignore`) if you want to keep them out of your sketch file.

---

## Step 3 — Flash Board 1 (Host)

1. Connect Board 1 via USB-C
2. Select **Automato Brain Board V2.0** in Arduino IDE
3. Open `firmware/BrainBoard_Host/BrainBoard_Host_v0.6.ino`
4. Click **Upload**
5. Open **Serial Monitor** at **115200 baud**
6. Press the **RESET** button on the board
7. Note the MAC address printed at startup — you'll need it for Board 2

Example output:
```
=== Brain Board Host Node (Board 1) v0.6 ===
Board 1 MAC address: A0:B1:C2:D3:E4:F5
>>> Copy this MAC into BrainBoard_Remote_v0.4.ino <<<
WiFi connected! IP: 192.168.1.42
Open http://192.168.1.42 in your browser
```

---

## Step 4 — Configure and Flash Board 2 (Remote)

1. Open `firmware/BrainBoard_Remote/BrainBoard_Remote_v0.4.ino`
2. Paste Board 1's MAC address into:

```cpp
uint8_t HOST_MAC_ADDRESS[] = {0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF5};
```

3. Connect Board 2 via USB-C
4. Upload the Remote sketch
5. Open Serial Monitor to confirm it's sending data

---

## Step 5 — Open the Dashboard

Navigate to `http://<Board1_IP>` in any browser on the same WiFi network.

You should see:
- **Board 1** sensor readings (live)
- **Board 2** sensor readings with link status badge
- **Relay Control** panel in the sidebar (if Qwiic GPIO is connected)
- **Agri Data** sidebar (enter a location to load external data)

---

## Relay Control (Optional)

Requires a **SparkFun Qwiic GPIO (TCA9534)** connected to J2 or J3.

1. Connect Qwiic GPIO to either Qwiic port
2. Wire relay board IN to Qwiic GPIO pin 0
3. Power relay board from USB hub (5V)
4. Reset Board 1 — TCA9534 will be detected automatically
5. Use ON/OFF buttons in the dashboard Relay Control panel

If the Qwiic GPIO is not connected, a warning banner appears but everything else works normally.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| WiFi won't connect | Check SSID/password. Must be 2.4 GHz. |
| Board 2 not appearing | Confirm MAC address is correct in Remote sketch. Both boards must be powered. |
| Board 2 shows "Signal lost" | Board 2 may be powered off or out of range. Stale timeout is 15 seconds. |
| Relay GPIO warning showing | TCA9534 not detected on I2C. Check Qwiic cable seating. |
| Dashboard won't load | Confirm you're on the same WiFi network as Board 1. |
