# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.9.8] - 2026-05-19

### Fixed

- **handleConfigSave always rebooted device on save**: only reboots now if WiFi/network
  fields changed (`ssid`, `password`, `hostname`, `ntp_server`). Other settings
  (brightness, timezone, intervals, coordinates, display options) apply live without
  restart. Eliminates reboot cascades during config tweaks and makes the test suite
  safe to run repeatedly.

## [1.9.7] - 2026-05-19

### Fixed

- **Config endpoint accepted garbage values, bricking device** (M1): `/config` form handler
  now validates all inputs. SSID rejected if empty, >31 chars, non-printable, or all-same-char
  (fuzz garbage like "AAAA..."). Numeric fields are `constrain()`-ed to safe ranges
  (`ntp_interval`/`weather_interval`: 60–86400, `brightness`: 0–7, `timezone`: ±12h,
  `latitude`/`longitude`: physical ranges, `display_orientation`: 0–3). Invalid input
  returns HTTP 400 instead of silently saving and rebooting.

## [1.9.6] - 2026-05-18

### Fixed

- **Weather hangs permanently after TCP timeout** (C1): `WEATHER_REQUESTING` state now has a
  15-second watchdog — if the HTTP callback never fires (NAT timeout, server half-close),
  state resets to IDLE and retry logic resumes
- **All retry timers freeze at ~49 days** (C2): Replaced unsafe `millis() >= nextRetryTime`
  with subtraction-safe `(millis() - nextRetryTime) < 0x80000000UL` in RetryConfig and
  WiFiRetryConfig; fixed boot guard with static flag
- **DST switches on wrong day** (H1): Last-Sunday formula was using year-only heuristic;
  now computes weekday of the 31st from current `tm_wday`: verified correct for 2026–2030
- **Heap fragmentation from web API polling** (H2): Replaced String+= concatenation with
  `snprintf`+`sendContent` in `handleAPITime`, `handleAPIStatus`, `handleAPIDebug`,
  `handleAPIWeather` — eliminates permanent heap fragmentation from 1s JS polling
- **Race condition on shared state** (H3): Added `volatile` to `weatherState` and `ntpState`
  — prevents compiler from caching stale values across ESPAsyncTCP callback boundaries
- **Malformed JSON on long error messages**: `lastError` clamped to 79 chars before snprintf

### Changed

- RAM usage: 37,268 bytes (46%) — down from 37,560 due to String elimination in API handlers

## [1.9.5] - 2026-05-18

### Fixed

- **Temperature disappears permanently** (#9): After 3 consecutive API failures,
  the weather state machine was permanently locked — `weatherState` stayed `WEATHER_FAILED`
  and no new requests were ever made even after the API recovered. Fix: reset retry
  counter and state after max retries so periodic refresh resumes after next interval (30 min).

## [1.9.4] - 2026-05-14

### Fixed

- **Date timezone** (#5): Date string now uses local time, so date rolls over at local midnight instead of UTC midnight
- **Weather periodic refresh** (#7): Removed `WEATHER_SUCCESS` state that permanently blocked periodic weather updates after first fetch; also fixed "Last update" counter showing raw timestamp instead of elapsed seconds
- **WiFi connects to wrong network** (#3): `WiFi.begin()` without params is now skipped when a saved SSID exists, preventing connection to SDK-cached open hotspots and config corruption

### Changed

- WiFi startup: SDK-cached credentials only attempted on first boot (no saved SSID); subsequent boots go directly to saved credentials
- ArduinoJson: `StaticJsonDocument` → `JsonDocument` for compatibility with ArduinoJson v7

### Removed

- Unused `weekday` variable in NTP DST calculation (compiler warning)

### Internal

- Split `clock_ntp_ota_v1.9/` directory renamed to `weather_clock/` — version no longer baked into path
- Removed internal development documents from repository

## [1.9.3] - 2026-01-06

### Changed

- Refactored monolithic 2,100-line `.ino` into modular structure:
  `display.cpp`, `ntp_client.cpp`, `weather.cpp`, `web_server.cpp`, `wifi_manager.cpp`

## [1.9.2] - 2026-01-06

### Fixed

- **CRITICAL**: WiFi credentials no longer cleared on connection failure
  - Previous behavior erased SSID/password after failed connection attempts
  - Now credentials persist indefinitely through WiFi outages
- OTA update credential loss issue (SDK credentials now tried first, then EEPROM)

### Added

- **WiFi Resilience**: Infinite retry with exponential backoff (5s → 10s → 20s → ... → 5min max)
- **Fallback AP**: "TJ56654-Setup" enabled after ~5 min of failed attempts
  - Device continues retry attempts while AP is active (dual STA+AP mode)
  - AP automatically disabled when WiFi reconnects
- **"No WiFi" display**: Shows retry countdown instead of numeric counter
- **"!" indicator**: Shown in date line when WiFi is disconnected
- **SDK credentials support**: Tries WiFiManager-stored credentials first

### Changed

- Clock continues running with last synced time during WiFi outages
- Improved user experience during network failures
- Removed aggressive credential clearing behavior

### Network Activity

- NTP sync: 1 hour interval (pool.ntp.org:123 UDP)
- Weather fetch: 30 min interval (api.open-meteo.com HTTP)
- mDNS: continuous (224.0.0.251 UDP multicast)
- ~50 requests/day total

## [1.9.1] - 2026-01-03

### Fixed

- **CRITICAL**: WiFi startup sequence - synchronous connection in setup() to ensure proper initialization order
- Display blank screen for 10+ seconds on boot (now shows time after ~15 seconds)
- "DNS resolution failed" errors during startup
- Sunrise/sunset labels cut off on 128px screen (removed labels, arrows are self-explanatory)

### Changed

- Hybrid WiFi model: synchronous in setup(), async reconnect in loop()
- Display formatting: superscript degree symbol and lowercase 'c' for temperature
- Sunrise/sunset screen now shows daylight duration (e.g., "Day 9h 41m") instead of static "Sun Times" text

### Documentation

- Added detailed v1.9.1_HYBRID_FIX.md explaining startup sequence problem and solution

## [1.9.0] - 2026-01-02

### Added

- Fully async architecture (zero blocking operations in loop)
- Custom async NTP implementation (manual UDP packet handling)
- Async HTTP weather fetch (AsyncHTTPRequest library)
- Exponential backoff retry logic for network failures
- Independent epoch tracking for accurate time between NTP syncs

### Changed

- Replaced blocking NTPClient with custom async UDP implementation
- Replaced blocking HTTP weather with AsyncHTTPRequest
- Removed all delay() calls from loop()
- WiFi connection now async (later fixed in v1.9.1)

### Performance

- Loop time: 10ms → <1ms (10x improvement)
- Weather fetch: 1-10s blocking → 0ms
- NTP sync: 5-20s blocking → 0ms
- WiFi reconnect: 15s blocking → 0ms
- OTA updates now work during active weather fetching

### Technical

- RAM usage: +536 bytes (36,980 → 37,516)
- Flash usage: +1040 bytes (407,500 → 408,540)
- IRAM: 61,987 bytes (94% - stable)

## [1.8.0] - 2026-01-01

### Security

- **CRITICAL**: Removed hardcoded WiFi credentials
- Integrated WiFiManager for secure captive portal setup
- Added config validation (magic number check)
- Input sanitization to prevent buffer overflows

### Fixed

- IRAM overflow crisis (94% → 70% via ICACHE_FLASH_ATTR)
- NTP interval bug (config value was ignored, always used hardcoded 1 hour)
- Boolean parsing errors in JSON config import/export
- Infinite loop protection in display mode rotation
- Memory leaks from String concatenation in web handlers

### Changed

- Web responses now use chunked transfer (eliminated 140+ String concatenations)
- Applied ICACHE_FLASH_ATTR to 26 functions (moved code from IRAM to Flash)
- Improved error handling throughout codebase

### Performance

- Peak heap usage reduced by ~8KB
- EEPROM validation prevents loading corrupted config

## [1.7.0] - 2025-12-31

### Added

- Initial working firmware with correct display support
- NTP time synchronization
- Weather data from Open-Meteo API (free, no API key required)
- OTA update support (web-based and ArduinoOTA)
- Web interface (/, /config, /debug, /update)
- REST API (time, status, weather, config export/import)
- Display rotation (time, weather, sunrise/sunset modes)
- Timezone support with manual DST configuration
- EEPROM configuration persistence

### Hardware Discovery

- Identified display as GM009605v4.3 (not TM1637 or TM1650)
- Discovered swapped I2C pins: SDA=GPIO0, SCL=GPIO2
- Switched to Adafruit_SSD1306 library

### Replaced

- QWeather API → Open-Meteo (no registration required)
- Proprietary firmware → Open source custom firmware
- Insecure WiFi handling → WiFiManager with timeout

## [1.6.0] - 2025-12-30 (unreleased)

### Attempted

- TM1650 LED driver support (incorrect - device has OLED)

## [1.5.0] - 2025-12-29 (unreleased)

### Attempted

- TM1637 7-segment display support (incorrect - device has OLED)

---

## Version Numbering

- **Major version** (X.0.0): Breaking changes, incompatible config format
- **Minor version** (1.X.0): New features, backward-compatible
- **Patch version** (1.9.X): Bug fixes, no new features

---

**Status**: v1.9.3 is production-ready and actively used 24/7.
