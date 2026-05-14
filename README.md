# Reverse Engineering a €5 AliExpress Weather Clock: A Security Story

<p align="center">
  <a href="https://github.com/petrochen/esp8266-weather-clock-opensource/releases">
    <img src="https://img.shields.io/github/v/release/petrochen/esp8266-weather-clock-opensource?style=flat-square&label=Release&color=green" alt="Release">
  </a>
  <a href="https://github.com/petrochen/esp8266-weather-clock-opensource/blob/main/LICENSE">
    <img src="https://img.shields.io/github/license/petrochen/esp8266-weather-clock-opensource?style=flat-square&label=License&color=blue" alt="License">
  </a>
  <a href="https://github.com/petrochen/esp8266-weather-clock-opensource/issues">
    <img src="https://img.shields.io/github/issues/petrochen/esp8266-weather-clock-opensource?style=flat-square&label=Issues&color=orange" alt="Issues">
  </a>
  <img src="https://img.shields.io/badge/ESP8266-ESP--01S-blue?style=flat-square" alt="Hardware">
  <img src="https://img.shields.io/badge/Status-Production%20Ready-brightgreen?style=flat-square" alt="Status">
</p>

## TL;DR

I bought a cute weather clock kit from AliExpress ([TJ-56-654](https://pt.aliexpress.com/item/1005008333782531.html)) and discovered it was **leaking my WiFi password in plaintext** to anyone within radio range. So I ripped out the firmware, wrote my own, and ended up with a fully async, OTA-updatable, Home Assistant-ready smart clock that's actually secure.

---

## Table of Contents

- [The Discovery: When "Smart" Means "Insecure"](#the-discovery-when-smart-means-insecure)
- [The Device](#the-device)
- [The Investigation](#the-investigation)
- [The Solution: Custom Firmware](#the-solution-custom-firmware)
- [Technical Deep Dive](#technical-deep-dive)
- [The Journey: v1.7 → v1.9.2](#the-journey-v17--v192)
- [What's Next: Home Assistant Integration](#whats-next-home-assistant-integration)
- [How to Flash This Firmware](#how-to-flash-this-firmware)
- [Web Interface](#web-interface)
- [API Documentation](#api-documentation)
- [Security Improvements](#security-improvements)
- [Lessons Learned](#lessons-learned)
- [Credits](#credits)

---

## The Discovery: When "Smart" Means "Insecure"

It started innocently enough. I ordered what looked like a fun DIY electronics project: an ESP8266-based weather clock with a transparent acrylic case and an OLED display. The listing promised:

- ✅ WiFi weather updates
- ✅ 3-day forecast
- ✅ Temperature, humidity, date/time
- ✅ "Intelligent connected to WIFI"

What they didn't mention:

**🚨 CRITICAL SECURITY FLAW 🚨**

When you first set up the device, it creates an access point with a default password. Fair enough - that's how WiFiManager works. But here's where it gets bad:

1. You connect to the AP (192.168.4.1)
2. You configure your home WiFi credentials
3. Device connects to your network
4. **The open AP stays active in parallel**
5. **Your WiFi password is displayed in plaintext on the config page**

Anyone within WiFi range could:

- Connect to the device's AP (weak default password)
- Browse to 192.168.4.1
- Read your WiFi password in plaintext
- Access your network

This is a textbook example of poor IoT security design. No thanks.

---

## The Device

**Product**: ESP8266 Mini Weather Clock Kit
**Model**: TJ-56-654
**Price**: ~€5 EUR
**Source**: [AliExpress Link](https://pt.aliexpress.com/item/1005008333782531.html)

### Original Hardware Specifications

| Component   | Details                                  |
| ----------- | ---------------------------------------- |
| **MCU**     | ESP-01S (ESP8266EX, 1MB flash, 80KB RAM) |
| **Display** | GM009605v4.3 OLED (128x64, I2C)          |
| **Power**   | 5V USB (Micro-USB)                       |
| **Case**    | Transparent acrylic (40x40x43mm)         |
| **PCB**     | TJ-56-654 main board                     |

### What It Came With

- Acrylic case parts (6 pieces)
- ESP-01S WiFi module
- OLED display module
- Main PCB with headers
- USB power cable
- Brass standoffs and screws
- Pin headers (soldering required)

### Original Firmware Issues

Beyond the password leak:

- **Dependency on QWeather API**: Requires account registration, project setup, API key management
- **Chinese cloud service**: All weather data routes through proprietary servers
- **No OTA updates**: Firmware updates require disassembly and FTDI connection
- **Limited features**: Fixed display modes, no customization
- **Unknown code**: Closed-source firmware, no way to audit what it's doing

---

## The Investigation

### Opening It Up

The transparent case made inspection easy - just unscrew the brass standoffs. Inside:

- **ESP-01S module** clearly labeled with pinout
- **I2C OLED display** connected via 4 pins (VCC, GND, SDA, SCL)
- **No additional sensors** (temperature/humidity were from weather API, not local)

The ESP-01S pinout is printed right on the PCB:

```
3V3  |  GND
 TX  |  GPIO0  (I2C SDA)
 RX  |  GPIO2  (I2C SCL)
EN   |  GND
```

### Connecting FTDI

To flash custom firmware, you need:

1. **FTDI USB-to-Serial adapter** (3.3V! Not 5V - you'll fry the ESP8266)
2. **Jumper wires**
3. **Steady hands**

**Wiring:**

```
FTDI        ESP-01S
────────────────────
 3V3    →   3V3
 GND    →   GND
 TX     →   RX
 RX     →   TX
 GND    →   GPIO0  (for programming mode)
```

**Boot into flash mode:**

1. Connect GPIO0 to GND
2. Power on the device
3. Remove GPIO0 to GND connection after boot
4. Device is now in programming mode

**Programming:**

- Use Arduino IDE with ESP8266 board support
- Select board: "Generic ESP8266 Module"
- Flash size: 1MB (FS:64KB OTA:~470KB)
- Upload speed: 115200 baud

After the first flash with OTA support, you never need wires again - all updates happen over WiFi.

---

## The Solution: Custom Firmware

I decided to write a complete replacement firmware with:

### Core Principles

1. **Security First**: No hardcoded credentials, no open networks, WiFiManager with proper AP timeout
2. **Privacy**: Use free, open APIs (Open-Meteo instead of QWeather)
3. **Maintainability**: OTA updates for painless improvements
4. **Performance**: Fully async architecture, no blocking operations
5. **Reliability**: Proper error handling, exponential backoff, memory safety

### Features Implemented

#### 🌐 Network & Time

- **WiFiManager** captive portal for secure first-time setup
- **Hybrid WiFi**: Synchronous on boot (ensures proper init), async reconnect during operation
- **NTP time sync** with configurable server and interval
- **Timezone support** with automatic European DST calculation
- **mDNS**: Access via `http://tj56654-clock.local/`

#### 🌦️ Weather Data

- **Open-Meteo API**: Free, no registration, no API key
- **Configurable location**: Latitude/longitude + city name
- **Data**: Temperature, sunrise, sunset, daylight duration
- **Smart updates**: Async fetch every 30 minutes (configurable)

#### 🔄 OTA Updates

- **Web-based OTA**: Upload .bin files via browser at `/update`
- **ArduinoOTA**: Update directly from Arduino IDE
- **Non-blocking**: System stays responsive during updates
- **Secure**: Password-protected upload (admin/admin - change it!)

#### 📺 Display Modes

Three rotating display screens (configurable interval):

1. **Time Mode**
   - Large HH:MM display
   - Blinking colon animation
   - Day of week and date
   - 12/24 hour format support

2. **Weather Mode**
   - Temperature with superscript °c
   - City name
   - Clean, minimalist layout

3. **Sunrise/Sunset Mode**
   - Sunrise time with ↑ arrow
   - Sunset time with ↓ arrow
   - **Daylight duration** (e.g., "Day 9h 41m")

All modes are center-aligned, rotation-aware, and gracefully handle missing data.

#### 🌐 Web Interface

- `/` - Home page with live time
- `/config` - Full configuration form
- `/debug` - System diagnostics
- `/update` - OTA firmware upload

#### 🔌 REST API

All endpoints return JSON:

- `GET /api/time` - Current time
- `GET /api/status` - System status (WiFi, uptime, heap)
- `GET /api/debug` - Detailed diagnostics
- `GET /api/weather` - Weather + sunrise/sunset
- `GET /api/config` - Export configuration
- `POST /api/config` - Import configuration
- `POST /api/eeprom-clear` - Factory reset
- `POST /api/reboot` - Remote reboot

---

## Technical Deep Dive

### Architecture: Fully Async State Machines

The firmware uses **zero blocking operations** in the main loop. Everything is state-machine-based:

#### Weather State Machine

```cpp
enum WeatherState { IDLE, REQUESTING, SUCCESS, FAILED };
```

Uses `AsyncHTTPRequest` library:

- Non-blocking HTTP requests
- Callback-based response handling
- Exponential backoff on failures (1s → 2s → 4s)
- Maximum 3 retries before giving up

#### NTP State Machine

```cpp
enum NTPState { IDLE, REQUEST_SENT, WAITING, SUCCESS, FAILED };
```

Custom manual NTP implementation:

- Builds raw UDP packets (48 bytes)
- Non-blocking `parsePacket()` checks
- 5-second timeout
- Independent epoch tracking for accuracy between syncs

#### WiFi State Machine

```cpp
enum WiFiConnectionState { IDLE, CONNECTING, CONNECTED, FAILED };
```

**Hybrid model** (this was critical!):

- **Setup phase**: Synchronous connection (waits up to 10 seconds)
  - Why? OTA, web server, NTP all need WiFi ready
  - Without this, device shows blank display for 10+ seconds
- **Loop phase**: Async reconnection (checks every 5 seconds)
  - Why? Don't freeze the entire system if WiFi drops

### Memory Optimization

ESP8266 has strict memory limits:

| Memory Type | Total     | Used    | Usage   | Status      |
| ----------- | --------- | ------- | ------- | ----------- |
| **Flash**   | 1,048,576 | 408,844 | 38%     | ✅ Plenty   |
| **RAM**     | 80,192    | 37,644  | 46%     | ✅ Safe     |
| **IRAM**    | 65,536    | 61,987  | **94%** | ⚠️ Critical |

**IRAM Crisis Solution:**

IRAM (Instruction RAM) is limited and fills fast. The solution: `ICACHE_FLASH_ATTR` macro.

```cpp
void ICACHE_FLASH_ATTR handleConfig() {
  // This function's code lives in Flash, not IRAM
  // Saves precious IRAM at cost of slightly slower execution
}
```

Applied to 26 functions (web handlers, display, config utilities), reducing IRAM pressure from **overflow risk** to **sustainable 94%**.

**String Safety:**

Avoid String concatenation in loops (causes heap fragmentation):

```cpp
// ❌ BAD - 140+ concatenations
String html = "";
html += F("<!DOCTYPE html>");
html += F("<head>...");  // x138 more times

// ✅ GOOD - Chunked responses
server.setContentLength(CONTENT_LENGTH_UNKNOWN);
server.send(200, "text/html", "");
server.sendContent_P(HTML_HEADER);
server.sendContent_P(HTML_FOOTER);
server.sendContent("");  // End
```

### Configuration Storage

26-field struct stored in EEPROM (512 bytes):

```cpp
struct Config {
  char ssid[32];
  char password[64];
  int timezone_offset;
  bool dst_enabled;
  uint8_t brightness;
  char ntp_server[64];
  unsigned long ntp_interval;
  bool hour_format_24;
  char hostname[32];
  float latitude;
  float longitude;
  char city_name[32];
  bool weather_enabled;
  unsigned long weather_interval;
  unsigned long display_rotation_sec;
  bool show_weather;
  bool show_sunrise_sunset;
  uint8_t display_orientation;
  uint32_t magic;  // 0xC10CC10C - validation
};
```

**EEPROM validation**: Magic number check prevents loading corrupted data. On validation failure, gracefully defaults to safe config.

### Display Hardware Discovery

This took **3 firmware iterations** to get right:

**v1.5**: Assumed TM1637 (7-segment LED driver)

- ❌ Wrong - device has OLED, not 7-segment LEDs

**v1.6**: Tried TM1650 (another LED driver)

- ❌ Wrong - I2C addresses didn't match

**v1.7**: Identified GM009605v4.3 (SSD1306-compatible OLED)

- ✅ Correct! Used Adafruit_SSD1306 library
- ✅ Discovered swapped pins: SDA on GPIO0, SCL on GPIO2

**Pin mapping quirk:**

Standard ESP8266 I2C uses GPIO4 (SDA) and GPIO5 (SCL), but ESP-01S only exposes GPIO0 and GPIO2. The board designer mapped:

- GPIO0 → SDA (unusual)
- GPIO2 → SCL (unusual)

This is **backwards** from typical breakout boards, but works perfectly once configured:

```cpp
Wire.begin(0, 2);  // SDA=GPIO0, SCL=GPIO2
```

---

## The Journey: v1.7 → v1.9.2

### v1.7: Display Discovery ✅

- Identified correct display hardware
- Basic time display working
- WiFiManager integration
- First OTA deployment

### v1.8: Stability & Security 🔒

**Goals**: Fix memory issues, eliminate security holes

**Changes:**

- IRAM optimization (added `ICACHE_FLASH_ATTR` to 26 functions)
- Removed hardcoded WiFi credentials
- Fixed NTP interval bug (config value was ignored)
- Fixed boolean parsing in JSON import
- Added input validation (buffer overflow protection)
- Chunked HTTP responses (eliminated 140+ String concatenations)

**Result**: IRAM usage 94% → 70%, no memory leaks, secure config storage

### v1.9.0: Full Async Refactoring ⚡

**Goals**: Eliminate all blocking operations

**Changes:**

- Async HTTP weather fetch (AsyncHTTPRequest library)
- Custom async NTP implementation (manual UDP packets)
- Async WiFi connection (state machine)
- Removed all `delay()` calls from `loop()`
- Exponential backoff retry logic

**Performance:**

| Operation      | Before (v1.8)  | After (v1.9.0) | Improvement         |
| -------------- | -------------- | -------------- | ------------------- |
| Weather fetch  | 1-10s blocking | 0ms            | ✅ Async callback   |
| NTP sync       | 5-20s blocking | 0ms            | ✅ Non-blocking UDP |
| WiFi reconnect | 15s blocking   | 0ms            | ✅ State machine    |
| Loop time      | 10ms minimum   | <1ms           | ✅ 10x faster       |

**Result**: Device stays responsive during OTA updates while weather is fetching!

### v1.9.1: Hybrid Fix 🎯

**Problem Discovered:**

After deploying v1.9.0, the display showed **blank screen for 10 seconds** after boot, with `DNS resolution failed` errors in logs.

**Root Cause:**

Making WiFi fully async broke the **initialization order**:

```cpp
void setup() {
  setupWiFi();                   // Returns immediately (async)
  setupOTA();                    // WiFi NOT ready! ❌
  setupWebServer();              // WiFi NOT ready! ❌
  testInternetConnectivity();    // WiFi NOT ready! → DNS error
}
```

**Solution: Hybrid Model**

| Phase     | WiFi Mode    | Blocking? | Why?                        |
| --------- | ------------ | --------- | --------------------------- |
| `setup()` | Synchronous  | 10s max   | OTA/web/NTP need WiFi ready |
| `loop()`  | Asynchronous | 0s        | Don't freeze on reconnect   |

**Results:**

- ✅ Display shows time immediately after WiFi connects (~15 sec boot)
- ✅ No "DNS resolution failed" errors
- ✅ Proper initialization order guaranteed
- ✅ Device never freezes on WiFi loss during operation

### v1.9.2: WiFi Resilience (Current) 🛡️

**Problem Discovered:**

After WiFi outages, the device would **clear stored credentials** and enter AP mode, requiring manual reconfiguration every time the router restarted.

**Root Cause:**

Aggressive credential clearing on connection failure:

```cpp
if (wifiRetry.currentRetry >= wifiRetry.maxRetries) {
  memset(config.ssid, 0, sizeof(config.ssid));     // ❌ Clears credentials!
  memset(config.password, 0, sizeof(config.password));
  saveConfig();
  // Enter AP mode...
}
```

**Solution: Resilient WiFi**

| Feature             | Before (v1.9.1)            | After (v1.9.2)                  |
| ------------------- | -------------------------- | ------------------------------- |
| Credential clearing | After 5 failed attempts    | Never                           |
| Retry strategy      | Give up after 5 tries      | Infinite with backoff           |
| Max retry interval  | N/A                        | 5 minutes                       |
| Fallback AP         | After clearing credentials | After ~5 min (dual STA+AP mode) |
| Clock during outage | Blank display              | Shows last synced time          |

**Key Changes:**

- **Never clear credentials** on connection failure
- **Exponential backoff**: 5s → 10s → 20s → ... → 5min max
- **Fallback AP** ("TJ56654-Setup") enabled after ~5 min, while still retrying
- **Dual STA+AP mode**: Device continues reconnect attempts while AP is active
- **SDK credentials support**: Tries WiFiManager-stored credentials first, then EEPROM
- **"No WiFi" display**: Shows retry countdown instead of cryptic numbers
- **"!" indicator**: Shown in date line when WiFi disconnected

**Network Activity Summary:**

| Service | Interval   | Endpoint           | Protocol      |
| ------- | ---------- | ------------------ | ------------- |
| NTP     | 1 hour     | pool.ntp.org:123   | UDP           |
| Weather | 30 min     | api.open-meteo.com | HTTP          |
| mDNS    | continuous | 224.0.0.251        | UDP multicast |

~50 requests/day total.

**Results:**

- ✅ Credentials persist through WiFi outages
- ✅ Device automatically reconnects when WiFi returns
- ✅ Clock continues running with last synced time
- ✅ User can reconfigure via fallback AP if needed

**Startup Timeline:**

```
[0-5s]   Display init, startup animation
[5-15s]  WiFi connection (SYNCHRONOUS in setup())
         ✅ WiFi connected! IP assigned
[15-20s] OTA init, web server start, NTP client ready
         ✅ Internet test: PASSED
[20-30s] First async NTP sync
         ✅ Time synced and displayed
```

### Memory Evolution

| Version | RAM Usage    | IRAM Usage       | Flash Usage   | Notes                 |
| ------- | ------------ | ---------------- | ------------- | --------------------- |
| v1.7    | 34,980 (43%) | **61,987 (94%)** | 407,500 (38%) | IRAM crisis           |
| v1.8    | 36,980 (46%) | **45,120 (68%)** | 407,800 (38%) | ICACHE_FLASH_ATTR fix |
| v1.9.0  | 37,516 (46%) | **61,987 (94%)** | 408,540 (38%) | Async libs added      |
| v1.9.1  | 37,644 (46%) | **61,987 (94%)** | 408,844 (38%) | Hybrid WiFi fix       |
| v1.9.2  | 37,800 (47%) | **61,987 (94%)** | 409,100 (39%) | WiFi resilience       |

**Verdict**: Stable memory usage, no leaks detected after 24h+ uptime tests.

---

## What's Next: Home Assistant Integration

The firmware is designed to be extensible. Next planned features:

### Custom Display Screens

Pull data from Home Assistant via REST API:

- **Smart home stats**: Energy usage, room temperatures
- **Sensor data**: Air quality, CO2 levels
- **Automation states**: Alarm status, door locks

### MQTT Integration

- Publish time/weather data to MQTT broker
- Subscribe to topics for display content
- Enable automation triggers (e.g., display alert when door opens)

### WebSocket Live Updates

Replace polling with WebSocket for:

- Real-time config changes without page refresh
- Live display preview in web UI
- Push notifications for firmware updates

---

## How to Flash This Firmware

### Requirements

- **Hardware**: TJ-56-654 weather clock or compatible ESP-01S + OLED setup
- **FTDI Adapter**: 3.3V USB-to-Serial (CP2102, FT232RL, CH340)
- **Software**: Arduino IDE 1.8.x or 2.x

### Arduino IDE Setup

1. **Install ESP8266 Board Support**
   - File → Preferences
   - Additional Board Manager URLs: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Tools → Board → Boards Manager → Search "ESP8266" → Install

2. **Install Required Libraries**
   - Sketch → Include Library → Manage Libraries
   - Install:
     - `Adafruit GFX Library`
     - `Adafruit SSD1306`
     - `NTPClient`
     - `WiFiManager` (by tzapu)
     - `AsyncHTTPRequest_Generic`
     - `ESPAsyncTCP`
     - `ArduinoJson` (by Benoit Blanchon)

3. **Board Configuration**
   - Board: "Generic ESP8266 Module"
   - Flash Size: "1MB (FS:64KB OTA:~470KB)"
   - Flash Mode: "DIO"
   - Flash Frequency: "40MHz"
   - CPU Frequency: "80MHz"
   - Upload Speed: "115200"

### First Flash (via FTDI)

1. **Wire the ESP-01S**:

   ```
   FTDI 3.3V  →  ESP-01S 3V3
   FTDI GND   →  ESP-01S GND
   FTDI TX    →  ESP-01S RX
   FTDI RX    →  ESP-01S TX
   FTDI GND   →  ESP-01S GPIO0 (boot mode)
   ```

2. **Compile and Upload**:
   - Open `weather_clock.ino`
   - Sketch → Upload
   - Wait for "Done uploading"
   - Remove GPIO0-to-GND jumper
   - Press reset or power cycle

3. **Initial Setup**:
   - Device creates AP: "TJ56654-Setup"
   - Connect to it (password: `12345678`)
   - Captive portal opens automatically
   - Select your WiFi network and enter password
   - Device reboots and connects

### Subsequent Updates (OTA)

1. **Via Web Interface** (easiest):
   - Browse to `http://192.168.x.x/update` (find IP from router)
   - Or use mDNS: `http://tj56654-clock.local/update`
   - Login: `admin` / `admin`
   - Choose .bin file from `build/` folder
   - Click "Update"
   - Device reboots automatically (~15 seconds)

2. **Via Arduino IDE**:
   - Tools → Port → Select "tj56654-clock at 192.168.x.x"
   - Sketch → Upload
   - No wires needed!

---

## Web Interface

### Home Page (`/`)

Current time display with live updates via JavaScript (fetches `/api/time` every second).

### Configuration Page (`/config`)

Comprehensive settings form:

**WiFi Settings**

- SSID
- Password
- Hostname (for mDNS)

**Time Settings**

- Timezone offset (seconds from UTC)
- DST enabled (European rules)
- NTP server address
- NTP sync interval (seconds)
- Hour format (12h/24h)

**Weather Settings**

- Enabled/disabled toggle
- Latitude
- Longitude
- City name (for display)
- Update interval (seconds)

**Display Settings**

- Brightness (0-7)
- Rotation (0°, 90°, 180°, 270°)
- Display rotation interval (seconds)
- Show weather screen (toggle)
- Show sunrise/sunset screen (toggle)

All settings persist to EEPROM and survive reboots.

### Debug Page (`/debug`)

Real-time diagnostics:

- **System**: Uptime, free heap, chip ID, flash size
- **WiFi**: SSID, IP, signal strength, MAC address, gateway, DNS
- **Time**: Current time, timezone, DST status, NTP sync status
- **NTP Stats**: Last sync, attempts, successes, failures
- **Weather**: Temperature, sunrise/sunset, last update, API status
- **Network Tests**: Internet connectivity, DNS resolution
- **Display**: Current mode, rotation, brightness

Perfect for troubleshooting connectivity or API issues.

---

## API Documentation

All endpoints return JSON (except `/update` which is for file upload).

### `GET /api/time`

Current time information.

**Response:**

```json
{
  "current": "14:23:45",
  "date": "2026-01-03",
  "day": "Friday",
  "timezone_offset": 0,
  "dst_active": false
}
```

### `GET /api/status`

System status overview.

**Response:**

```json
{
  "wifi": {
    "ssid": "MyNetwork",
    "ip": "192.168.1.47",
    "rssi": -38,
    "hostname": "tj56654-clock"
  },
  "time": {
    "current": "14:23:45",
    "timezone_offset": 0,
    "ntp_synced": true
  },
  "system": {
    "uptime": 3627,
    "free_heap": 35104,
    "chip_id": "f77134"
  }
}
```

### `GET /api/weather`

Current weather data.

**Response:**

```json
{
  "temperature": 15.4,
  "city": "Portimao",
  "sunrise": "07:48",
  "sunset": "17:29",
  "daylight_hours": 9,
  "daylight_minutes": 41,
  "last_update": "14:20:00",
  "valid": true
}
```

### `GET /api/config`

Export full configuration as JSON.

**Response:**

```json
{
  "ssid": "MyNetwork",
  "timezone_offset": 0,
  "dst_enabled": true,
  "brightness": 5,
  "ntp_server": "pool.ntp.org",
  "ntp_interval": 3600,
  "hour_format_24": true,
  "hostname": "tj56654-clock",
  "latitude": 37.19,
  "longitude": -8.54,
  "city_name": "Portimao",
  "weather_enabled": true,
  "weather_interval": 1800,
  "display_rotation_sec": 5,
  "show_weather": true,
  "show_sunrise_sunset": true,
  "display_orientation": 0
}
```

### `POST /api/config`

Import configuration from JSON.

**Request Body**: Same structure as export response (password field optional for security).

**Response:**

```json
{
  "status": "ok"
}
```

Device automatically reboots after import.

### `POST /api/eeprom-clear`

Factory reset (clears EEPROM).

**Response:**

```json
{
  "status": "cleared"
}
```

Device reboots to WiFiManager captive portal.

### `POST /api/reboot`

Remote reboot.

**Response:**

```json
{
  "status": "rebooting"
}
```

Device reboots immediately.

---

## Security Improvements

### What Changed from Original Firmware

| Issue                  | Original                       | Custom Firmware                   |
| ---------------------- | ------------------------------ | --------------------------------- |
| **WiFi Password Leak** | Plaintext in open AP           | No open AP after setup            |
| **Persistent AP**      | Always active                  | Only on first boot or failure     |
| **API Keys**           | QWeather requires registration | Open-Meteo (no key needed)        |
| **Cloud Dependency**   | Chinese servers                | Direct API calls, no intermediary |
| **Firmware Updates**   | Manual FTDI only               | OTA via WiFi (password-protected) |
| **Config Access**      | No authentication              | Admin password required           |
| **Code Transparency**  | Closed source                  | Open source (you're reading it!)  |

### Best Practices Implemented

1. **WiFiManager Timeout**: AP automatically closes after 180 seconds if no configuration
2. **Fallback AP Mode**: If credentials fail, device creates secure AP ("TJ56654-Clock" with password)
3. **EEPROM Validation**: Magic number check prevents loading corrupted data
4. **Input Sanitization**: Buffer overflow protection on all user inputs
5. **Memory Safety**: No dynamic String allocations in loops, fixed-size buffers
6. **Error Handling**: Graceful degradation (e.g., display shows time even if weather fails)

### Recommended Post-Flash Steps

1. **Change OTA password**: Edit line ~60 in `.ino` file:

   ```cpp
   ArduinoOTA.setPassword("admin");  // Change this!
   ```

2. **Change web admin password**: Edit line ~430:

   ```cpp
   if (!server.authenticate("admin", "admin")) {  // Change this!
   ```

3. **Set strong WiFi AP fallback password**: Edit line ~780:

   ```cpp
   WiFi.softAP("TJ56654-Clock", "12345678");  // Change this!
   ```

4. **Disable unnecessary features**: If you don't need weather, disable it in `/config` to save bandwidth

---

## Lessons Learned

### Hardware

1. **Always check pinouts**: Don't assume standard pin mappings - this device swaps SDA/SCL
2. **FTDI is your friend**: A $2 adapter unlocks any ESP8266 device
3. **Transparent cases are great**: Made debugging and identification trivial
4. **Read the PCB silk screen**: Model numbers and pin labels save hours of guessing

### Software

1. **Async is hard but worth it**: Fully non-blocking architecture eliminates user-facing freezes
2. **IRAM is precious**: On ESP8266, use `ICACHE_FLASH_ATTR` liberally
3. **Hybrid approaches work**: Don't be dogmatic - synchronous WiFi in setup() solved a critical UX issue
4. **State machines scale**: Better than callback hell for complex async operations
5. **Test on real hardware**: Emulators can't catch pin mapping errors or memory constraints

### Security

1. **IoT security is often terrible**: Always audit devices before trusting them on your network
2. **Open source is safer**: Closed firmware is a black box - you have no idea what it's doing
3. **Defaults matter**: Insecure defaults (open AP, plaintext passwords) lead to real vulnerabilities
4. **Defense in depth**: Multiple layers (WiFiManager timeout, password protection, validation) catch mistakes

### Development

1. **OTA from day 1**: Flashing via FTDI gets old fast - build OTA support early
2. **Version your work**: Backup files (.bak, .bak2) saved me multiple times
3. **Document as you go**: Release notes and architecture docs prevent "what was I thinking?" moments
4. **Incremental improvements**: v1.7 → v1.8 → v1.9.x made debugging manageable

---

## Credits

**Hardware**: TJ-56-654 Weather Clock Kit ([AliExpress](https://pt.aliexpress.com/item/1005008333782531.html))

**Firmware**: Written from scratch with love and frustration

**Libraries Used**:

- [ESP8266 Arduino Core](https://github.com/esp8266/Arduino)
- [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306)
- [WiFiManager](https://github.com/tzapu/WiFiManager)
- [AsyncHTTPRequest_Generic](https://github.com/khoih-prog/AsyncHTTPRequest_Generic)
- [NTPClient](https://github.com/arduino-libraries/NTPClient)

**APIs**:

- [Open-Meteo](https://open-meteo.com/) - Free weather API, no registration required

**Tools**:

- Arduino IDE 2.x
- FTDI FT232RL USB-to-Serial adapter
- Lots of coffee ☕

---

## Project Structure

```
esp8266-weather-clock/
├── firmware/
│   └── weather_clock/
│       └── weather_clock.ino       # Main firmware (~2,100 lines)
├── docs/
│   ├── HARDWARE.md                 # Hardware specifications
│   ├── INSTALLATION.md             # Flashing guide
│   ├── v1.9_RELEASE_NOTES.md       # v1.9.0 async refactoring
│   ├── v1.9.1_HYBRID_FIX.md        # WiFi startup fix
│   └── v1.9.2_WIFI_RESILIENCE.md   # WiFi resilience documentation
├── CHANGELOG.md                    # Version history
└── README.md                       # This file
```

---

## License

This project is released into the public domain. Do whatever you want with it. If you improve it, consider sharing your changes - that's how we make IoT better.

---

## Final Thoughts

This project started as "I don't trust this device" and ended as "I built something better."

The original firmware had security holes you could drive a truck through. The custom replacement:

- ✅ Doesn't leak WiFi passwords
- ✅ Uses free, open APIs
- ✅ Updates over WiFi
- ✅ Runs fully async (no freezing)
- ✅ Integrates with Home Assistant (coming soon)
- ✅ Is completely auditable (you're reading the source)

Total cost: €5 hardware + a weekend of tinkering.

If you have one of these devices, **flash this firmware**. If you're buying IoT gadgets, **always audit them first**. And if something seems insecure, **fix it yourself** - that's the hacker spirit.

Now go make something cool. 🚀

---

**P.S.**: If you found this useful, consider starring the repo. If you found a bug, open an issue. If you want to add Home Assistant screens, let's collaborate - I'm planning that next!

**Author**: apetrochenko
**Date**: 2026-01-06
**Firmware Version**: v1.9.2 (Production Ready)
