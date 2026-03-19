# Quick Start Guide

## What You Need

- 1–2× Automato Brain Board V2.0
- USB-C cables
- A 2.4 GHz WiFi network (WPA2)
- Arduino IDE 2.x
- Espressif ESP32 Arduino package (3.x or later)

---

## Step 1 — Install Libraries

Open Arduino IDE → **Tools → Manage Libraries** and install:

- `Adafruit SHTC3 Library`
- `Adafruit TSL2591`
- `SparkFun TCA9534` *(for relay control — install even if you don't have the hardware yet)*

Accept all dependency installs (Adafruit BusIO, Adafruit Unified Sensor).

---

## Step 2 — Arduino IDE Settings

With Board 1 connected, set the following in **Tools**:

| Setting | Value |
|---|---|
| Board | ESP32C6 Dev Module |
| Partition Scheme | Custom |
| USB CDC On Boot | **Enabled** ← critical for serial output |
| All other settings | defaults |

> ⚠️ **USB CDC On Boot must be Enabled.** Without it, the Serial Monitor will show no output and appear as if the board isn't running.

> ⚠️ **Partition Scheme must be Custom.** The sketch folder includes `partitions.csv` which defines the OTA + LittleFS layout. Without this, OTA and LittleFS will not work.

---

## Step 3 — Configure WiFi Credentials

Open `BrainBoard_Host_v07.ino` and edit:

```cpp
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";
```

> ⚠️ Never commit real credentials to GitHub.

---

## Step 4 — Flash Board 1 (Host)

1. Connect Board 1 via USB-C
2. Open `firmware/BrainBoard_Host/BrainBoard_Host_v07.ino` in Arduino IDE
3. Click **Upload**
4. Open **Serial Monitor** at **115200 baud**
5. Press the **RESET** button on the board
6. Note the MAC address printed at startup — you'll need it for Board 2

Example output:
```
=== Automato Brain Board Host v0.7.0 ===
LittleFS... OK  (firmware v0.7.0 / webapp v0.7.0)
TCA9534 GPIO expander... OK — Relay pin 0 LOW (OFF)
SHTC3... OK
TSL2591... OK
Board 1 MAC: E4:B3:23:89:7E:20
WiFi connected! IP: 192.168.1.13
Dashboard: http://192.168.1.13
```

> **First boot:** The firmware automatically formats LittleFS and writes the dashboard files on first boot. No separate LittleFS upload step is needed.

---

## Step 5 — Configure and Flash Board 2 (Remote)

1. Open `firmware/BrainBoard_Remote/BrainBoard_Remote_v04.ino`
2. Paste Board 1's MAC address into:

```cpp
uint8_t HOST_MAC_ADDRESS[] = {0xE4, 0xB3, 0x23, 0x89, 0x7E, 0x20};
```

3. Apply the same Arduino IDE settings as Board 1 (USB CDC On Boot: Enabled)
4. Connect Board 2 via USB-C and click **Upload**
5. Open Serial Monitor to confirm it's sending data

---

## Step 6 — Open the Dashboard

Navigate to `http://<Board1_IP>` in any browser on the same WiFi network.

You should see:
- **Board 1** sensor readings (live)
- **Board 2** sensor readings with link status badge
- **Relay Control** panel in the sidebar (if Qwiic GPIO is connected)
- **Agri Data** sidebar (enter a location to load external data)

---

## OTA Updates

Once Board 1 is running, you can update firmware and the dashboard over WiFi — no USB cable needed.

Navigate to `http://<Board1_IP>/update`.

**Firmware update:**
1. In Arduino IDE: **Sketch → Export Compiled Binary**
2. Select `BrainBoard_Host_v07.ino.bin` (not `.merged.bin`)
3. Upload via the Firmware section

**Webapp update:**
1. Build a LittleFS image from the `data/` folder using the LittleFS uploader plugin
2. Upload the resulting `.bin` via the Webapp section

After a successful update, the page counts down and redirects to the dashboard automatically.

---

## Relay Control (Optional)

Requires a **SparkFun Qwiic GPIO (TCA9534)** connected via Qwiic cable.

1. Connect Qwiic GPIO to either Qwiic port on Brain Board
2. Wire relay board IN to Qwiic GPIO pin 0
3. Set relay board jumper to **H** (high-level trigger)
4. Power relay board from 5V (USB hub or bench supply)
5. Reset Board 1 — TCA9534 will be detected automatically
6. Use ON/OFF buttons in the dashboard Relay Control panel

If the Qwiic GPIO is not connected, a warning banner appears but everything else works normally.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| No serial output | Set USB CDC On Boot → Enabled in Arduino IDE Tools menu |
| LittleFS fails to mount | Set Partition Scheme → Custom. Confirm `partitions.csv` is in the sketch folder. |
| WiFi won't connect | Check SSID/password. Must be 2.4 GHz. |
| Board 2 not appearing | Confirm MAC address is correct in Remote sketch. Both boards must be powered. Reset Board 1 after Board 2 is running. |
| Board 2 shows "Signal lost" | Board 1 may have rebooted onto a different WiFi channel. Reset Board 1. |
| Relay GPIO warning showing | TCA9534 not detected on I2C. Check Qwiic cable seating. |
| Relay not responding to dashboard | Check relay board jumper is in H (high trigger) position. |
| Dashboard won't load | Confirm you're on the same WiFi network as Board 1. |
