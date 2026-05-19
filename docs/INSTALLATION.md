# Installation Guide

Complete step-by-step guide to flash this firmware on your ESP8266 weather clock.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Arduino IDE Setup](#arduino-ide-setup)
3. [Hardware Connection](#hardware-connection)
4. [First Flash (via FTDI)](#first-flash-via-ftdi)
5. [Initial Configuration](#initial-configuration)
6. [OTA Updates](#ota-updates)
7. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Hardware Required

- **ESP8266 Weather Clock** (TJ-56-654 or compatible)
- **FTDI USB-to-Serial adapter** (3.3V!)
  - Recommended: FT232RL, CP2102, CH340
  - ⚠️ Must support 3.3V - 5V adapters will damage ESP8266
- **Jumper wires** (male-to-female, 5 pieces)
- **USB cable** (for FTDI adapter)

### Software Required

- **Arduino IDE** (1.8.19+ or 2.x)
  - Download: https://www.arduino.cc/en/software
- **USB drivers** for your FTDI chip:
  - FT232: https://ftdichip.com/drivers/vcp-drivers/
  - CP2102: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
  - CH340: Usually auto-installed on macOS/Linux

---

## Arduino IDE Setup

### 1. Install ESP8266 Board Support

**Method 1: Via Board Manager (recommended)**

1. Open Arduino IDE
2. Go to: **File → Preferences**
3. In "Additional Board Manager URLs", add:
   ```
   http://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
4. Click **OK**
5. Go to: **Tools → Board → Boards Manager**
6. Search: "ESP8266"
7. Install: **esp8266 by ESP8266 Community** (version 3.0.0 or newer)
8. Wait for installation to complete

**Method 2: Manual Installation**

See: https://arduino-esp8266.readthedocs.io/en/latest/installing.html

### 2. Install Required Libraries

Go to: **Sketch → Include Library → Manage Libraries**

Install the following libraries (search by name):

| Library                      | Author           | Min Version | Purpose                       |
| ---------------------------- | ---------------- | ----------- | ----------------------------- |
| **Adafruit GFX Library**     | Adafruit         | 1.11.0      | Graphics primitives           |
| **Adafruit SSD1306**         | Adafruit         | 2.5.0       | OLED display driver           |
| **NTPClient**                | Fabrice Weinberg | 3.2.0       | NTP time sync (base)          |
| **WiFiManager**              | tzapu            | 2.0.0       | Captive portal setup          |
| **AsyncHTTPRequest_Generic** | Khoi Hoang       | 1.13.0      | Async weather fetch           |
| **ESPAsyncTCP**              | me-no-dev        | 1.2.2       | Async TCP (required by above) |

**Installation steps for each library:**

1. Search library name in Library Manager
2. Click **Install**
3. Wait for "INSTALLED" badge
4. Repeat for all libraries

### 3. Board Configuration

**Important**: Configure these settings **before** compiling:

1. Go to: **Tools → Board → ESP8266 Boards**
2. Select: **Generic ESP8266 Module**
3. Configure settings:

| Setting           | Value                      | Why                                      |
| ----------------- | -------------------------- | ---------------------------------------- |
| Flash Size        | `1MB (FS:64KB OTA:~470KB)` | Enables OTA with 470KB max firmware      |
| Flash Mode        | `DIO`                      | Compatible with most ESP-01S modules     |
| Flash Frequency   | `40MHz`                    | Safe default for all ESP8266             |
| CPU Frequency     | `80MHz`                    | Standard (can use 160MHz for more speed) |
| Crystal Frequency | `26MHz`                    | Default for ESP-01S                      |
| Upload Speed      | `115200`                   | Balance between speed and reliability    |
| Debug Level       | `None`                     | Reduces firmware size                    |
| IwIP Variant      | `v2 Lower Memory`          | Better for 1MB flash devices             |
| Erase Flash       | `Only Sketch`              | Preserves config on re-flash             |

---

## Hardware Connection

### Step 1: Identify Pins

ESP-01S pinout (looking at module from top, antenna up):

```
       ┌─────────────┐
       │             │
       │  [antenna]  │
       │             │
3V3 ━━━━━━━━━━━━━━━━━ GND
 TX ━━━━━━━━━━━━━━━━━ GPIO0
 RX ━━━━━━━━━━━━━━━━━ GPIO2
 EN ━━━━━━━━━━━━━━━━━ GND
       │             │
       └─────────────┘
```

### Step 2: Wire FTDI to ESP-01S

**Connections:**

| FTDI Pin | ESP-01S Pin | Wire Color | Notes                             |
| -------- | ----------- | ---------- | --------------------------------- |
| 3.3V     | 3V3         | Red        | Power (NOT 5V!)                   |
| GND      | GND         | Black      | Ground                            |
| TX       | RX          | Yellow     | Data: FTDI transmit → ESP receive |
| RX       | TX          | Green      | Data: FTDI receive → ESP transmit |
| GND      | GPIO0       | Blue       | **Programming mode** (temporary)  |

**⚠️ CRITICAL**:

- **Never connect 5V to ESP-01S** - it's not 5V tolerant!
- Double-check polarity before powering on
- GPIO0-to-GND connection is **temporary** (only for programming mode)

### Step 3: Enter Programming Mode

1. **Connect all wires** as shown above (including GPIO0 to GND)
2. **Plug FTDI into USB** (ESP-01S powers on in programming mode)
3. **Verify**: Some FTDI adapters have a power LED that should light up
4. **Remove GPIO0-to-GND jumper** (keep other connections)

ESP-01S is now in programming mode, ready to receive firmware.

---

## First Flash (via FTDI)

### Step 1: Open Project

1. Download or clone this repository
2. Navigate to: `esp8266-weather-clock-opensource/firmware/weather_clock/`
3. Open: `weather_clock.ino` in Arduino IDE

### Step 2: Verify Board Settings

1. Go to: **Tools → Board → Generic ESP8266 Module**
2. Confirm settings match those in [Board Configuration](#3-board-configuration)
3. Go to: **Tools → Port**
4. Select your FTDI adapter:
   - macOS: `/dev/cu.usbserial-*` or `/dev/cu.wchusbserial*`
   - Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
   - Windows: `COM3`, `COM4`, etc.

If port doesn't appear:

- Check USB cable is data-capable (not charge-only)
- Install FTDI drivers
- Try different USB port
- Check wire connections

### Step 3: Compile Firmware

1. Click: **Sketch → Verify/Compile** (or press Ctrl+R / Cmd+R)
2. Wait for compilation (1-2 minutes)
3. Check output for:
   ```
   Sketch uses X bytes (X%) of program storage space.
   Global variables use Y bytes (Y%) of dynamic memory.
   ```
4. Verify:
   - Program storage < 470KB (for OTA to work)
   - IRAM usage < 95% (shown in verbose output)

### Step 4: Upload Firmware

1. **Ensure GPIO0 was grounded during power-on** (then removed)
2. Click: **Sketch → Upload** (or press Ctrl+U / Cmd+U)
3. Watch serial monitor for:
   ```
   Connecting........
   Chip is ESP8266EX
   Uploading stub...
   Running stub...
   Writing at 0x00000000... (X %)
   ```
4. Wait for: **Hard resetting via RTS pin...**
5. Success message: **Done uploading**

**If upload fails**, see [Troubleshooting](#upload-fails).

### Step 5: Power Cycle

1. **Disconnect FTDI from USB**
2. **Remove GPIO0-to-GND wire** (very important!)
3. **Reconnect FTDI to USB** (ESP-01S boots into normal mode)
4. Firmware should now be running!

---

## Initial Configuration

### Step 1: Connect to Device AP

1. On your phone/laptop, scan for WiFi networks
2. Look for: **TJ56654-Setup** (or similar)
3. Password: `12345678`
4. Connect to this network

### Step 2: Captive Portal

**Automatic (iOS/Android):**

- Captive portal should pop up automatically
- If not, manually browse to: http://192.168.4.1

**Manual (laptop):**

- Browse to: http://192.168.4.1

### Step 3: Configure WiFi

1. Click: **Configure WiFi**
2. Select your home network from the list
3. Enter WiFi password
4. (Optional) Set custom hostname
5. Click: **Save**
6. Device reboots and connects to your WiFi

### Step 4: Find Device IP

**Method 1: Router Admin Panel**

- Log into your router
- Look for device: "tj56654-clock"
- Note its IP address (e.g., 192.168.1.47)

**Method 2: mDNS (if your OS supports it)**

- Browse to: http://tj56654-clock.local/
- Works on macOS, Linux, iOS out-of-box
- Windows: Install [Bonjour Print Services](https://support.apple.com/kb/DL999)

**Method 3: Serial Monitor**

1. Keep FTDI connected (no GPIO0 to GND!)
2. Open: **Tools → Serial Monitor**
3. Set baud rate: **115200**
4. Press reset button (if available) or power cycle
5. Watch for: `WiFi connected! IP: 192.168.x.x`

### Step 5: Access Web Interface

Browse to: `http://<device-ip>/` or `http://tj56654-clock.local/`

You should see:

- Current time display
- Navigation links (Config, Debug, Update)

### Step 6: Configure Settings

1. Go to: `http://<device-ip>/config`
2. Configure:
   - **Timezone offset** (in seconds from UTC)
   - **DST enabled** (for European DST rules)
   - **Weather location** (latitude, longitude, city name)
   - **Display settings** (brightness, rotation interval)
3. Click: **Save Configuration**
4. Device reboots with new settings

**Timezone examples:**

- UTC+0 (London winter): `0`
- UTC+1 (Paris winter): `3600`
- UTC-5 (New York winter): `-18000`

---

## OTA Updates

After initial FTDI flash, all future updates can be done **over WiFi** (no wires!).

### Method 1: Web Interface (Easiest)

1. Download latest `.bin` file from releases
2. Browse to: `http://<device-ip>/update`
3. Login:
   - Username: `admin`
   - Password: `admin` (change in source code!)
4. Click: **Choose File**
5. Select `.bin` file
6. Click: **Update**
7. Wait for upload (~1 minute)
8. Device reboots automatically
9. Check version at: `http://<device-ip>/debug`

### Method 2: Arduino IDE

1. Open `.ino` file in Arduino IDE
2. Go to: **Tools → Port**
3. Select: **tj56654-clock at 192.168.x.x** (network port!)
4. Click: **Sketch → Upload**
5. Wait for upload
6. Device reboots automatically

**Note**: Network port only appears if device is online and mDNS is working.

### Method 3: curl (Command Line)

```bash
# Build firmware first, then:
curl -u admin:admin -F "file=@/path/to/firmware.bin" http://192.168.x.x/update
```

Replace:

- `192.168.x.x` with your device IP
- `/path/to/firmware.bin` with actual path to .bin file

---

## Troubleshooting

### Upload Fails

**Error: "espcomm_open failed"**

- Check: GPIO0 was grounded during power-on
- Check: FTDI driver installed
- Try: Different USB port
- Try: Lower upload speed (57600 instead of 115200)

**Error: "espcomm_upload_mem failed"**

- Check: Wire connections (especially RX↔TX swap)
- Check: FTDI is 3.3V (not 5V)
- Try: Power ESP-01S from external 3.3V supply (FTDI may not provide enough current)

**Error: "Chip sync error"**

- GPIO0 must be LOW during boot
- Try: Hold GPIO0 to GND, reset ESP, then release GPIO0

### Compilation Fails

**Error: "library not found"**

- Install missing library via Library Manager
- Restart Arduino IDE after installing

**Error: "Sketch too big"**

- Flash size must be set to 1MB
- Reduce features if necessary (disable weather, etc.)

**IRAM overflow error**

- Some functions missing `ICACHE_FLASH_ATTR`
- Use version from this repo (already optimized)

### WiFi Connection Fails

**Device creates AP but won't connect to home WiFi**

- ESP8266 only supports 2.4GHz (not 5GHz)
- Try: Different WiFi channel (1, 6, or 11)
- Check: WiFi password is correct
- Check: Router supports 802.11n

**Device reboots in a loop**

- Likely: Power supply too weak (brownout)
- Solution: Use powered USB hub or different power adapter
- Minimum: 500mA @ 5V

### Display Issues

**Display is blank**

- Check: I2C wiring (SDA=GPIO0, SCL=GPIO2)
- Check: Display I2C address (try 0x3C and 0x3D in code)
- Test: Use `/api/i2c-scan` endpoint to detect display

**Display shows garbage**

- Wrong display library or initialization
- This firmware is for SSD1306-compatible OLED
- Verify display model is GM009605v4.3 or similar

**Display is upside down**

- Change `display_orientation` in `/config`
- Values: 0 (normal), 1 (90°), 2 (180°), 3 (270°)

### Time Not Syncing

**Time shows 00:00:00**

- Check: WiFi is connected (`/api/status`)
- Check: NTP server is reachable (default: pool.ntp.org)
- Check: Router firewall allows UDP port 123
- Try: Different NTP server (e.g., time.google.com)

**Time is wrong by hours**

- Check: Timezone offset in `/config`
- Remember: Offset is in **seconds**, not hours
  - Example: UTC+1 = 3600 seconds

### Weather Not Updating

**Temperature shows 0.0°C**

- Check: Internet connectivity (`/api/debug`)
- Check: Latitude/longitude are correct
- Check: Open-Meteo API is accessible (visit https://open-meteo.com/ in browser)
- Try: Manual weather fetch at `/test-weather` (if implemented)

### OTA Update Fails

**Web upload hangs at 0%**

- Check: Device is online and responsive
- Try: Smaller firmware (disable features)
- Try: Upload via Arduino IDE instead

**Upload completes but device doesn't reboot**

- Wait 30 seconds (sometimes slow)
- Manually power cycle device
- Check serial output for errors

### Serial Monitor Shows Errors

**"DNS resolution failed"**

- In v1.9.0 (fixed in v1.9.1)
- Upgrade to v1.9.1 or later

**Watchdog reset / exception**

- Likely: Code bug or memory corruption
- Check: IRAM usage < 95%
- Report: Open issue with serial log

---

## Advanced: Custom Configuration

### Change OTA Password

Edit in source code (line ~60):

```cpp
ArduinoOTA.setPassword("your-secret-password");
```

### Change Web Admin Password

Edit in source code (line ~430):

```cpp
if (!server.authenticate("admin", "your-secret-password")) {
```

### Disable Features

To save memory, disable unused features:

**Disable weather:**

- Set `weather_enabled = false` in `/config`
- Or remove weather code from source

**Disable sunrise/sunset:**

- Set `show_sunrise_sunset = false` in `/config`

**Disable display rotation:**

- Set `display_rotation_sec = 0` (manual switch only)

---

## Factory Reset (no tools needed)

Since v1.9.6, three quick power cycles within 10 seconds trigger a factory reset:

1. Power off the device
2. Power on (briefly, < 10 sec)
3. Power off, power on
4. Power off, power on

On the third boot the OLED shows `FACTORY RESET! WiFi: TJ56654-Setup Pass: 12345678`.
Connect your phone to that AP and reconfigure WiFi at `http://192.168.4.1/config`.

Use this if:

- Forgot configured WiFi credentials
- Device stuck in "No WiFi" loop after router change
- Tests/fuzzing corrupted config

## Verifying Installation

Run the test suite against your device:

```bash
python3 tests/test_device.py 192.168.x.x
```

A healthy device passes all 73 tests, with heap drift under 1 KB across the run.

## Getting Help

If you're still stuck:

1. **Check existing issues**: https://github.com/your-repo/issues
2. **Open new issue** with:
   - Arduino IDE version
   - ESP8266 board package version
   - Library versions
   - Full serial monitor output
   - Steps to reproduce
3. **Join discussion** for general questions

---

**Happy flashing!** 🚀
