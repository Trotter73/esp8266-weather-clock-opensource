# Architecture Notes

Internal notes for contributors modifying the firmware. End users don't need this.

## Async model

The main loop performs zero blocking operations. All network I/O runs through callback-based state machines:

- **Weather**: `AsyncHTTPRequest` callback updates `weatherState` (IDLE → REQUESTING → IDLE on success/failure), with a 15s watchdog in the loop to recover from TCP hangs
- **NTP**: Manual UDP packet build + non-blocking `parsePacket()` with 5s timeout
- **WiFi**: Synchronous in `setup()` (so OTA / web / NTP have a working network at start), async reconnect in `loop()` with exponential backoff

State variables shared between callbacks and the main loop are declared `volatile` (`weatherState`, `ntpState`) since ESPAsyncTCP callbacks fire from within `yield()`/TCP poll and can interrupt the loop at any point.

## Timer correctness

`millis()` is `uint32_t` and rolls over every ~49.7 days. All deadline comparisons use the subtraction-safe pattern:

```cpp
// Unsafe: fails at rollover
if (millis() >= nextRetryTime) { ... }

// Safe: works across rollover
if ((millis() - nextRetryTime) < 0x80000000UL) { ... }
```

See `RetryConfig::isRetryTime()` in `config.h` for the canonical implementation.

## Memory budget (ESP-01S, 1MB flash)

| Pool      | Total     | Used     | %   | Notes                                                    |
| --------- | --------- | -------- | --- | -------------------------------------------------------- |
| **Flash** | 1,048,576 | ~411,000 | 39% | Headroom for features                                    |
| **RAM**   | 80,192    | ~37,700  | 46% | Stable across all 1.9.x releases                         |
| **IRAM**  | 65,536    | 61,987   | 94% | **Critical** — `ICACHE_FLASH_ATTR` required on new funcs |

### IRAM discipline

IRAM is the tightest constraint. Apply `ICACHE_FLASH_ATTR` to any non-critical function so its code lives in flash instead of IRAM:

```cpp
void ICACHE_FLASH_ATTR handleConfig() {
  // code in flash, slightly slower but saves IRAM
}
```

Currently 26 functions use this attribute (web handlers, display routines, config helpers). Functions called from interrupt context or hot loops should stay in IRAM.

### Heap fragmentation

ESP8266 heap is never compacted. Repeated `String` concatenation in long-running handlers (especially `/api/*` endpoints polled by the web UI) permanently fragments the heap until reboot.

Pattern to follow for HTTP responses:

```cpp
char buf[256];
server.setContentLength(CONTENT_LENGTH_UNKNOWN);
server.send(200, "application/json", "");
snprintf_P(buf, sizeof(buf), PSTR("{\"...\":%d}"), value);
server.sendContent(buf);
server.sendContent("");  // close chunked transfer
```

Never:

```cpp
String html = "";
html += "...";  // <- heap fragmentation
```

## EEPROM layout

512 bytes mapped via `EEPROM.begin(512)`:

| Offset | Size   | Content                                           |
| ------ | ------ | ------------------------------------------------- |
| 0      | ~260 B | `Config` struct (validated by magic `0xC10CC10C`) |
| 480    | 2 B    | `ResetCounter` for triple power-cycle reset       |

Writes are flash-sector-erase operations (4KB), so the entire 512-byte image is rewritten on each `EEPROM.commit()`. Expect ~100k cycles before wear matters.

The `Config` struct includes a magic number — on validation failure, the firmware falls back to defaults rather than loading garbage.

## Factory reset

Three quick power cycles within 10 seconds (controlled by `RESET_COUNTER_WINDOW`) triggers a factory reset:

1. Each boot increments the counter at EEPROM offset 480
2. If the device runs for 10s, the counter clears back to 0
3. If count reaches 3 first, WiFi credentials are wiped and the device reboots into WiFiManager AP mode

For atomicity, credentials are committed BEFORE the counter is zeroed. Power loss between the two writes still results in a recoverable state (cleared creds → AP boot).

## Display

128×64 OLED, SSD1306-compatible (GM009605v4.3 panel). The ESP-01S exposes only GPIO0 and GPIO2, and the board designer mapped them as I2C:

```cpp
Wire.begin(0, 2);  // SDA=GPIO0, SCL=GPIO2  (non-standard!)
```

This is the inverse of typical breakout boards — easy to get wrong if you're porting code.

## Web server

`ESP8266WebServer` (not async, but fast enough for low-frequency requests). All API responses use chunked transfer with `snprintf` + `sendContent`. Form save (`/config`) only triggers a reboot when WiFi/network fields actually change; other settings apply live.

Input validation in `handleConfigSave` clamps numeric fields and rejects garbage SSIDs (HTTP 400) — see `isValidSSID()` in `web_server.cpp`.

## Adding new features

1. Check IRAM usage after build — if growing, add `ICACHE_FLASH_ATTR` to your new functions
2. Don't use `String` in handlers or callbacks — use `char buf[]` + `snprintf`
3. New shared state between callbacks and loop → declare `volatile`
4. New timer logic → use the subtraction-safe `millis()` pattern
5. New API endpoints → run `tests/test_device.py` against the device to verify heap stability

For more context on past design decisions, see `CHANGELOG.md`.
