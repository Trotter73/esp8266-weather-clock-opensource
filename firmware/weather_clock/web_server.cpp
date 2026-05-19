/*
 * web_server.cpp - Web server handlers
 * TJ-56-654 Weather Clock v1.9.3
 */

#include "globals.h"
#include <EEPROM.h>
#include <ESP8266mDNS.h>

// Dummy function for compatibility
void ICACHE_FLASH_ATTR displaySegments(const uint8_t segments[]) {
  // Not used with OLED
}

// PROGMEM templates for handleRoot()
const char ROOT_HTML_HEADER[] PROGMEM =
  "<!DOCTYPE html><html><head>"
  "<meta charset='UTF-8'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<title>TJ-56-654 Clock v" FIRMWARE_VERSION "</title>"
  "<style>"
  "body{font-family:Arial;margin:20px;background:#f0f0f0;}"
  ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
  "h1{color:#333;}.time{font-size:48px;text-align:center;margin:20px 0;font-weight:bold;color:#0066cc;}"
  ".info{margin:10px 0;padding:10px;background:#f9f9f9;border-radius:5px;}"
  ".error{background:#ffebee;color:#c62828;}"
  "a.button{display:inline-block;background:#0066cc;color:white;padding:10px 20px;text-decoration:none;border-radius:5px;margin:10px 5px;}"
  "a.button:hover{background:#0052a3;}"
  ".debug{background:#fff3cd;color:#856404;padding:10px;border-radius:5px;margin:10px 0;}"
  "</style>"
  "<script>"
  "function updateTime(){fetch('/api/time').then(r=>r.json()).then(d=>{document.getElementById('time').innerText=d.time;});}"
  "setInterval(updateTime,1000);updateTime();"
  "</script>"
  "</head><body>"
  "<div class='container'>"
  "<h1>TJ-56-654 NTP Clock v" FIRMWARE_VERSION "</h1>"
  "<div class='time' id='time'>--:--:--</div>";

const char ROOT_HTML_FOOTER[] PROGMEM =
  "<a href='/config' class='button'>Configuration</a>"
  "<a href='/debug' class='button'>Debug Info</a>"
  "<a href='/update' class='button'>Firmware Update</a>"
  "<a href='/api/status' class='button'>Status (JSON)</a>"
  "<button class='button' onclick=\"if(confirm('Reboot device?')) fetch('/api/reboot', {method:'POST'}).then(()=>alert('Rebooting...'))\">Reboot</button>"
  "</div></body></html>";

void ICACHE_FLASH_ATTR handleRoot() {
  char buf[150];

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent_P(ROOT_HTML_HEADER);

  if (lastError != "") {
    snprintf_P(buf, sizeof(buf), PSTR("<div class='info error'><strong>Error:</strong> %s</div>"), lastError.c_str());
    server.sendContent(buf);
  }

  snprintf_P(buf, sizeof(buf), PSTR("<div class='info'><strong>WiFi:</strong> %s</div>"), WiFi.SSID().c_str());
  server.sendContent(buf);

  snprintf_P(buf, sizeof(buf), PSTR("<div class='info'><strong>IP:</strong> %s</div>"), WiFi.localIP().toString().c_str());
  server.sendContent(buf);

  snprintf_P(buf, sizeof(buf), PSTR("<div class='info'><strong>Hostname:</strong> %s.local</div>"), config.hostname);
  server.sendContent(buf);

  snprintf_P(buf, sizeof(buf), PSTR("<div class='info'><strong>Uptime:</strong> %lu seconds</div>"), millis()/1000);
  server.sendContent(buf);

  if (!timeClient.isTimeSet()) {
    snprintf_P(buf, sizeof(buf), PSTR("<div class='debug'><strong>NTP not synced yet</strong><br>Attempts: %d | Success: %d</div>"),
      ntpAttempts, ntpSuccesses);
    server.sendContent(buf);
  }

  server.sendContent_P(ROOT_HTML_FOOTER);
  server.sendContent("");
}

// PROGMEM templates for handleDebug()
const char DEBUG_HTML_HEADER[] PROGMEM =
  "<!DOCTYPE html><html><head>"
  "<meta charset='UTF-8'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<title>Debug Info</title>"
  "<style>"
  "body{font-family:monospace;margin:20px;background:#f0f0f0;}"
  ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:10px;}"
  ".ok{color:green;}.fail{color:red;}"
  "pre{background:#f5f5f5;padding:10px;border-radius:5px;overflow-x:auto;}"
  "button{background:#0066cc;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;margin:5px;}"
  "</style>"
  "</head><body>"
  "<div class='container'>"
  "<h1>Debug Information</h1>"
  "<h2>Network</h2><pre>";

const char DEBUG_HTML_FOOTER[] PROGMEM =
  "<h2>Actions</h2>"
  "<button onclick=\"location.href='/test-ntp'\">Test NTP Now</button>"
  "<button onclick=\"location.href='/test-display'\">Test Display (8888)</button>"
  "<button onclick=\"location.reload()\">Refresh</button>"
  "<button onclick=\"location.href='/api/config'\">Download Config (JSON)</button>"
  "<button onclick=\"if(confirm('Clear EEPROM and reboot?')) fetch('/api/eeprom-clear', {method:'POST'}).then(()=>alert('Rebooting...'))\">Clear EEPROM</button>"
  "<button onclick=\"location.href='/'\">Back</button>"
  "</div></body></html>";

void ICACHE_FLASH_ATTR handleDebug() {
  char buf[200];

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent_P(DEBUG_HTML_HEADER);

  snprintf_P(buf, sizeof(buf), PSTR("SSID: %s\nIP: %s\nGateway: %s\nDNS: %s\nRSSI: %d dBm\nHostname: %s\n</pre>"),
    WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(),
    WiFi.dnsIP().toString().c_str(), WiFi.RSSI(), config.hostname);
  server.sendContent(buf);

  server.sendContent_P(PSTR("<h2>Internet Connectivity</h2><pre>Status: "));
  server.sendContent_P(internetConnected ? PSTR("<span class='ok'>Connected</span>\n</pre>") : PSTR("<span class='fail'>Not connected</span>\n</pre>"));

  server.sendContent_P(PSTR("<h2>NTP</h2><pre>"));
  snprintf_P(buf, sizeof(buf), PSTR("Server: %s\nUpdate interval: %u seconds\nSynced: "), config.ntp_server, config.ntp_interval);
  server.sendContent(buf);
  server.sendContent_P(timeClient.isTimeSet() ? PSTR("<span class='ok'>Yes</span>\n") : PSTR("<span class='fail'>No</span>\n"));

  snprintf_P(buf, sizeof(buf), PSTR("UTC time: %s\n"), timeClient.getFormattedTime().c_str());
  server.sendContent(buf);

  if (timeClient.isTimeSet()) {
    unsigned long epochTime = timeClient.getEpochTime();
    unsigned long localTime = epochTime + getTotalOffset(epochTime);
    snprintf_P(buf, sizeof(buf), PSTR("Local time: %02d:%02d:%02d\n"), (int)((localTime/3600)%24), (int)((localTime/60)%60), (int)(localTime%60));
    server.sendContent(buf);
  }

  snprintf_P(buf, sizeof(buf), PSTR("Attempts: %d\nSuccesses: %d\nLast error: %s\n</pre>"), ntpAttempts, ntpSuccesses, lastError.c_str());
  server.sendContent(buf);

  server.sendContent_P(PSTR("<h2>Timezone & DST</h2><pre>"));
  snprintf_P(buf, sizeof(buf), PSTR("Base offset: %.1f hours (%ld seconds)\nDST enabled: %s\n"),
    config.timezone_offset/3600.0, config.timezone_offset, config.dst_enabled ? "Yes" : "No");
  server.sendContent(buf);

  if (config.dst_enabled && timeClient.isTimeSet()) {
    unsigned long epochTime = timeClient.getEpochTime();
    bool inDST = isDST(epochTime);
    snprintf_P(buf, sizeof(buf), PSTR("DST active now: %s\nTotal offset: %.1f hours\n"),
      inDST ? "<span class='ok'>Yes (+1 hour)</span>" : "No", getTotalOffset(epochTime)/3600.0);
    server.sendContent(buf);
  }

  snprintf_P(buf, sizeof(buf), PSTR("Time format: %s\n</pre>"), config.hour_format_24 ? "24-hour" : "12-hour (AM/PM)");
  server.sendContent(buf);

  server.sendContent_P(PSTR("<h2>Weather</h2><pre>"));
  snprintf_P(buf, sizeof(buf), PSTR("Enabled: %s\nValid data: %s\n"),
    config.weather_enabled ? "Yes" : "No",
    weather.valid ? "<span class='ok'>Yes</span>" : "<span class='fail'>No</span>");
  server.sendContent(buf);

  if (weather.valid) {
    snprintf_P(buf, sizeof(buf), PSTR("Temperature: %.1f C\nWeather code: %d\nWind speed: %.1f km/h\nLast update: %lu sec ago\n"),
      weather.temperature, weather.weathercode, weather.windspeed, (millis() - weather.lastUpdate)/1000);
    server.sendContent(buf);
  }

  snprintf_P(buf, sizeof(buf), PSTR("City: %s\nLocation: %.6f, %.6f\nUpdate interval: %u seconds\n</pre>"),
    config.city_name, config.latitude, config.longitude, config.weather_interval);
  server.sendContent(buf);

  server.sendContent_P(PSTR("<h2>Sunrise/Sunset</h2><pre>"));
  snprintf_P(buf, sizeof(buf), PSTR("Enabled: %s\n"), config.show_sunrise_sunset ? "Yes" : "No");
  server.sendContent(buf);

  if (sunTimes.lastDay != -1) {
    snprintf_P(buf, sizeof(buf), PSTR("<span class='ok'>Data available</span>\nSunrise: %s (%d min)\nSunset: %s (%d min)\nLast update day: %d\n</pre>"),
      sunTimes.sunrise, sunTimes.sunriseMinutes, sunTimes.sunset, sunTimes.sunsetMinutes, sunTimes.lastDay);
  } else {
    snprintf_P(buf, sizeof(buf), PSTR("<span class='fail'>No data</span>\n</pre>"));
  }
  server.sendContent(buf);

  server.sendContent_P(PSTR("<h2>Display</h2><pre>"));
  const char* modeStr = (displayMode == 0) ? " (Time)" : (displayMode == 1) ? " (Weather)" : " (Sun times)";
  snprintf_P(buf, sizeof(buf), PSTR("Current mode: %d%s\nRotation interval: %u seconds\nBrightness: %d (0-7)\nShow weather: %s\nShow sun times: %s\nNTP synced: %s\n</pre>"),
    displayMode, modeStr, config.display_rotation_sec, config.brightness,
    config.show_weather ? "Yes" : "No", config.show_sunrise_sunset ? "Yes" : "No",
    timeClient.isTimeSet() ? "<span class='ok'>Yes</span>" : "<span class='fail'>No</span>");
  server.sendContent(buf);

  server.sendContent_P(PSTR("<h2>System</h2><pre>"));
  snprintf_P(buf, sizeof(buf), PSTR("Uptime: %lu seconds\nFree heap: %u bytes\nChip ID: %X\nFlash size: %u bytes\nSDK version: %s\n</pre>"),
    millis()/1000, ESP.getFreeHeap(), ESP.getChipId(), ESP.getFlashChipSize(), ESP.getSdkVersion());
  server.sendContent(buf);

  server.sendContent_P(DEBUG_HTML_FOOTER);
  server.sendContent("");
}

void ICACHE_FLASH_ATTR handleTestNTP() {
  testInternetConnectivity();
  updateNTPTime();

  server.sendHeader("Location", "/debug");
  server.send(303);
}

void ICACHE_FLASH_ATTR handleTestDisplay() {
  uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
  displaySegments(data);
  delay(3000);

  server.sendHeader("Location", "/debug");
  server.send(303);
}

// PROGMEM templates for handleConfig()
const char CONFIG_HTML_HEADER[] PROGMEM =
  "<!DOCTYPE html><html><head>"
  "<meta charset='UTF-8'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<title>Configuration</title>"
  "<style>"
  "body{font-family:Arial;margin:20px;background:#f0f0f0;}"
  ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
  "input,select{width:100%;padding:8px;margin:5px 0 15px 0;border:1px solid #ddd;border-radius:4px;box-sizing:border-box;}"
  "button{background:#0066cc;color:white;padding:12px 20px;border:none;border-radius:5px;cursor:pointer;width:100%;}"
  "button:hover{background:#0052a3;}"
  "label{font-weight:bold;}"
  "</style>"
  "</head><body>"
  "<div class='container'>"
  "<h1>Configuration</h1>"
  "<form method='POST' action='/config'>";

const char CONFIG_HTML_FOOTER[] PROGMEM =
  "<button type='submit'>Save & Reboot</button>"
  "</form>"
  "<p><a href='/'>Back to Home</a></p>"
  "</div></body></html>";

void ICACHE_FLASH_ATTR handleConfig() {
  char buf[150];

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent_P(CONFIG_HTML_HEADER);

  snprintf_P(buf, sizeof(buf), PSTR("<label>WiFi SSID:</label><input type='text' name='ssid' value='%s' required>"), config.ssid);
  server.sendContent(buf);
  snprintf_P(buf, sizeof(buf), PSTR("<label>WiFi Password:</label><input type='password' name='password' value='%s'>"), config.password);
  server.sendContent(buf);

  snprintf_P(buf, sizeof(buf), PSTR("<label>Timezone Offset (seconds):</label><input type='number' name='timezone' value='%ld'>"), config.timezone_offset);
  server.sendContent(buf);
  snprintf_P(buf, sizeof(buf), PSTR("<label>Brightness (0-7):</label><input type='number' name='brightness' min='0' max='7' value='%d'>"), config.brightness);
  server.sendContent(buf);
  snprintf_P(buf, sizeof(buf), PSTR("<label>Hostname:</label><input type='text' name='hostname' value='%s'>"), config.hostname);
  server.sendContent(buf);

  server.sendContent_P(PSTR("<h2 style='margin-top:20px;'>Weather Settings</h2>"));
  snprintf_P(buf, sizeof(buf), PSTR("<label>City Name:</label><input type='text' name='city_name' value='%s'>"), config.city_name);
  server.sendContent(buf);
  snprintf_P(buf, sizeof(buf), PSTR("<label>Latitude:</label><input type='number' step='0.000001' name='latitude' value='%.6f'>"), config.latitude);
  server.sendContent(buf);
  snprintf_P(buf, sizeof(buf), PSTR("<label>Longitude:</label><input type='number' step='0.000001' name='longitude' value='%.6f'>"), config.longitude);
  server.sendContent(buf);
  snprintf_P(buf, sizeof(buf), PSTR("<label>Weather Update Interval (seconds):</label><input type='number' name='weather_interval' value='%u'>"), config.weather_interval);
  server.sendContent(buf);

  server.sendContent_P(PSTR("<h2 style='margin-top:20px;'>Display Settings</h2>"));
  snprintf_P(buf, sizeof(buf), PSTR("<label>Screen Rotation Interval (seconds):</label><input type='number' name='display_rotation_sec' value='%u'>"), config.display_rotation_sec);
  server.sendContent(buf);

  server.sendContent_P(CONFIG_HTML_FOOTER);
  server.sendContent("");
}

// Validate SSID: 1-31 printable ASCII chars, not all-same-char (likely fuzz garbage)
static bool isValidSSID(const String& s) {
  size_t n = s.length();
  if (n == 0 || n > 31) return false;
  char first = s[0];
  bool allSame = true;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (c < 0x20 || c > 0x7E) return false;  // non-printable
    if (c != first) allSame = false;
  }
  return !allSame;  // reject "AAAAA...", "BBBBB...", etc.
}

void ICACHE_FLASH_ATTR handleConfigSave() {
  // Track if reboot is required (only WiFi/network changes need it)
  bool needsRestart = false;

  // Validate before saving — reject obviously bad input rather than brick the device
  if (server.hasArg("ssid")) {
    String s = server.arg("ssid");
    if (!isValidSSID(s)) {
      server.send(400, "text/plain", "Invalid SSID (1-31 printable chars, not all-same)");
      return;
    }
    if (s != String(config.ssid)) needsRestart = true;
    safeStringCopy(s, config.ssid, sizeof(config.ssid));
  }
  if (server.hasArg("password")) {
    String p = server.arg("password");
    if (p.length() > 63) {
      server.send(400, "text/plain", "Password too long (max 63 chars)");
      return;
    }
    if (p != String(config.password)) needsRestart = true;
    safeStringCopy(p, config.password, sizeof(config.password));
  }
  if (server.hasArg("timezone")) {
    long tz = server.arg("timezone").toInt();
    config.timezone_offset = constrain(tz, -43200L, 43200L);  // ±12h
  }
  if (server.hasArg("brightness")) {
    config.brightness = constrain(server.arg("brightness").toInt(), 0, 7);
  }
  if (server.hasArg("hostname")) {
    String h = server.arg("hostname");
    if (h.length() == 0 || h.length() > 31) {
      server.send(400, "text/plain", "Invalid hostname length (1-31)");
      return;
    }
    if (h != String(config.hostname)) needsRestart = true;  // mDNS bind on boot
    safeStringCopy(h, config.hostname, sizeof(config.hostname));
  }
  if (server.hasArg("city_name")) {
    String c = server.arg("city_name");
    if (c.length() > 31) {
      server.send(400, "text/plain", "City name too long (max 31)");
      return;
    }
    safeStringCopy(c, config.city_name, sizeof(config.city_name));
  }
  if (server.hasArg("latitude")) {
    float lat = server.arg("latitude").toFloat();
    config.latitude = constrain(lat, -90.0f, 90.0f);
  }
  if (server.hasArg("longitude")) {
    float lon = server.arg("longitude").toFloat();
    config.longitude = constrain(lon, -180.0f, 180.0f);
  }
  if (server.hasArg("weather_interval")) {
    long wi = server.arg("weather_interval").toInt();
    config.weather_interval = constrain(wi, 60L, 86400L);  // 1min to 1day
  }
  if (server.hasArg("ntp_interval")) {
    long ni = server.arg("ntp_interval").toInt();
    config.ntp_interval = constrain(ni, 60L, 86400L);
  }
  if (server.hasArg("ntp_server")) {
    String n = server.arg("ntp_server");
    if (n.length() == 0 || n.length() > 63) {
      server.send(400, "text/plain", "Invalid NTP server (1-63 chars)");
      return;
    }
    if (n != String(config.ntp_server)) needsRestart = true;  // NTPClient re-init
    safeStringCopy(n, config.ntp_server, sizeof(config.ntp_server));
  }
  if (server.hasArg("display_rotation_sec")) {
    config.display_rotation_sec = constrain(server.arg("display_rotation_sec").toInt(), 1, 60);
  }
  if (server.hasArg("display_orientation")) {
    config.display_orientation = constrain(server.arg("display_orientation").toInt(), 0, 3);
    display.setRotation(config.display_orientation);
  }

  saveConfig();

  // Only restart for WiFi/network changes; other settings apply live
  if (needsRestart) {
    server.send(200, "text/html",
      F("<!DOCTYPE html><meta charset='UTF-8'>"
        "<meta http-equiv='refresh' content='5;url=/'>"
        "<h1>Configuration Saved!</h1>"
        "<p>WiFi/network changed — device will reboot in 5 seconds...</p>"));
    delay(1000);
    ESP.restart();
  } else {
    server.send(200, "text/html",
      F("<!DOCTYPE html><meta charset='UTF-8'>"
        "<meta http-equiv='refresh' content='2;url=/'>"
        "<h1>Configuration Saved</h1>"
        "<p>Applied without reboot. Returning to main page...</p>"));
  }
}

void ICACHE_FLASH_ATTR handleAPITime() {
  char buf[256];

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  snprintf_P(buf, sizeof(buf),
    PSTR("{\"time\":\"%s\",\"hours\":%d,\"minutes\":%d,\"seconds\":%d,\"epoch\":%lu}"),
    timeClient.getFormattedTime().c_str(),
    timeClient.getHours(),
    timeClient.getMinutes(),
    timeClient.getSeconds(),
    timeClient.getEpochTime());
  server.sendContent(buf);

  server.sendContent("");
}

void ICACHE_FLASH_ATTR handleAPIStatus() {
  char buf[256];

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  snprintf_P(buf, sizeof(buf),
    PSTR("{\"wifi\":{\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"hostname\":\"%s\"},"),
    WiFi.SSID().c_str(),
    WiFi.localIP().toString().c_str(),
    WiFi.RSSI(),
    config.hostname);
  server.sendContent(buf);

  snprintf_P(buf, sizeof(buf),
    PSTR("\"time\":{\"current\":\"%s\",\"timezone_offset\":%ld,\"ntp_synced\":%s},"),
    timeClient.getFormattedTime().c_str(),
    config.timezone_offset,
    timeClient.isTimeSet() ? "true" : "false");
  server.sendContent(buf);

  snprintf_P(buf, sizeof(buf),
    PSTR("\"system\":{\"uptime\":%lu,\"free_heap\":%u,\"chip_id\":\"%x\"}}"),
    millis() / 1000,
    ESP.getFreeHeap(),
    ESP.getChipId());
  server.sendContent(buf);

  server.sendContent("");
}

void ICACHE_FLASH_ATTR handleAPIDebug() {
  char buf[256];
  char errBuf[80];

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  // Clamp lastError to prevent snprintf truncation causing malformed JSON
  strncpy(errBuf, lastError.c_str(), sizeof(errBuf) - 1);
  errBuf[sizeof(errBuf) - 1] = '\0';

  snprintf_P(buf, sizeof(buf),
    PSTR("{\"internet_connected\":%s,\"ntp_attempts\":%d,\"ntp_successes\":%d,\"last_error\":\"%s\",\"gateway\":\"%s\",\"dns\":\"%s\"}"),
    internetConnected ? "true" : "false",
    ntpAttempts,
    ntpSuccesses,
    errBuf,
    WiFi.gatewayIP().toString().c_str(),
    WiFi.dnsIP().toString().c_str());
  server.sendContent(buf);

  server.sendContent("");
}

void ICACHE_FLASH_ATTR handleAPIWeather() {
  char buf[256];

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  snprintf_P(buf, sizeof(buf),
    PSTR("{\"enabled\":%s,\"valid\":%s,\"temperature\":%.1f,\"weathercode\":%d,\"windspeed\":%.1f,\"last_update\":%lu,"),
    config.weather_enabled ? "true" : "false",
    weather.valid ? "true" : "false",
    weather.temperature,
    weather.weathercode,
    weather.windspeed,
    weather.lastUpdate);
  server.sendContent(buf);

  snprintf_P(buf, sizeof(buf),
    PSTR("\"sunrise\":\"%s\",\"sunset\":\"%s\",\"sunrise_minutes\":%d,\"sunset_minutes\":%d}"),
    sunTimes.sunrise,
    sunTimes.sunset,
    sunTimes.sunriseMinutes,
    sunTimes.sunsetMinutes);
  server.sendContent(buf);

  server.sendContent("");
}

void ICACHE_FLASH_ATTR handleAPIConfigExport() {
  String json = "{";
  json += "\"firmware_version\":\"" FIRMWARE_VERSION "\",";
  json += "\"magic\":\"0x" + String(config.magic, HEX) + "\",";
  json += "\"ssid\":\"" + String(config.ssid) + "\",";
  json += "\"password\":\"" + String(config.password) + "\",";
  json += "\"timezone_offset\":" + String(config.timezone_offset) + ",";
  json += "\"dst_enabled\":" + String(config.dst_enabled ? "true" : "false") + ",";
  json += "\"brightness\":" + String(config.brightness) + ",";
  json += "\"ntp_server\":\"" + String(config.ntp_server) + "\",";
  json += "\"ntp_interval\":" + String(config.ntp_interval) + ",";
  json += "\"hour_format_24\":" + String(config.hour_format_24 ? "true" : "false") + ",";
  json += "\"hostname\":\"" + String(config.hostname) + "\",";
  json += "\"latitude\":" + String(config.latitude, 6) + ",";
  json += "\"longitude\":" + String(config.longitude, 6) + ",";
  json += "\"city_name\":\"" + String(config.city_name) + "\",";
  json += "\"weather_enabled\":" + String(config.weather_enabled ? "true" : "false") + ",";
  json += "\"weather_interval\":" + String(config.weather_interval) + ",";
  json += "\"display_rotation_sec\":" + String(config.display_rotation_sec) + ",";
  json += "\"show_weather\":" + String(config.show_weather ? "true" : "false") + ",";
  json += "\"show_sunrise_sunset\":" + String(config.show_sunrise_sunset ? "true" : "false");
  json += "}";

  server.sendHeader("Content-Disposition", "attachment; filename=clock-config.json");
  server.send(200, "application/json", json);
}

void ICACHE_FLASH_ATTR handleAPIConfigImport() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "No config data received");
    return;
  }

  String body = server.arg("plain");
  Serial.println("Received config: " + body);

  // Parse various fields (simplified parsing)
  int pos;

  pos = body.indexOf("\"ssid\":\"");
  if (pos >= 0) {
    int start = pos + 8;
    int end = body.indexOf("\"", start);
    if (end > start) {
      safeStringCopy(body.substring(start, end), config.ssid, sizeof(config.ssid));
    }
  }

  pos = body.indexOf("\"password\":\"");
  if (pos >= 0) {
    int start = pos + 12;
    int end = body.indexOf("\"", start);
    if (end > start) {
      safeStringCopy(body.substring(start, end), config.password, sizeof(config.password));
    }
  }

  pos = body.indexOf("\"timezone_offset\":");
  if (pos >= 0) {
    int start = pos + 18;
    int end = body.indexOf(",", start);
    if (end < 0) end = body.indexOf("}", start);
    if (end > start) {
      config.timezone_offset = body.substring(start, end).toInt();
    }
  }

  pos = body.indexOf("\"brightness\":");
  if (pos >= 0) {
    int start = pos + 13;
    int end = body.indexOf(",", start);
    if (end < 0) end = body.indexOf("}", start);
    if (end > start) {
      config.brightness = body.substring(start, end).toInt();
    }
  }

  pos = body.indexOf("\"hostname\":\"");
  if (pos >= 0) {
    int start = pos + 12;
    int end = body.indexOf("\"", start);
    if (end > start) {
      safeStringCopy(body.substring(start, end), config.hostname, sizeof(config.hostname));
    }
  }

  pos = body.indexOf("\"city_name\":\"");
  if (pos >= 0) {
    int start = pos + 13;
    int end = body.indexOf("\"", start);
    if (end > start) {
      safeStringCopy(body.substring(start, end), config.city_name, sizeof(config.city_name));
    }
  }

  pos = body.indexOf("\"latitude\":");
  if (pos >= 0) {
    int start = pos + 11;
    int end = body.indexOf(",", start);
    if (end < 0) end = body.indexOf("}", start);
    if (end > start) {
      config.latitude = body.substring(start, end).toFloat();
    }
  }

  pos = body.indexOf("\"longitude\":");
  if (pos >= 0) {
    int start = pos + 12;
    int end = body.indexOf(",", start);
    if (end < 0) end = body.indexOf("}", start);
    if (end > start) {
      config.longitude = body.substring(start, end).toFloat();
    }
  }

  config.magic = CONFIG_MAGIC;
  saveConfig();

  server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Config imported and saved. Reboot recommended.\"}");
}

void ICACHE_FLASH_ATTR handleEEPROMClear() {
  EEPROM.begin(512);
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0xFF);
  }
  EEPROM.commit();
  EEPROM.end();

  Serial.println("EEPROM cleared!");

  server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"EEPROM cleared, device will reboot\"}");

  delay(1000);
  ESP.restart();
}

void ICACHE_FLASH_ATTR handleReboot() {
  Serial.println("Reboot requested via web interface");

  server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Device rebooting...\"}");

  delay(1000);
  ESP.restart();
}

void ICACHE_FLASH_ATTR handleI2CScan() {
  String json = "{\"i2c_scan\":{\"devices\":[";

  int deviceCount = 0;

  for (uint8_t address = 0x08; address <= 0x77; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      if (deviceCount > 0) json += ",";
      json += "{\"address\":\"0x";
      if (address < 16) json += "0";
      json += String(address, HEX);
      json += "\",\"decimal\":" + String(address) + "}";
      deviceCount++;
    }
    delay(1);
  }

  json += "],\"count\":" + String(deviceCount);

  json += ",\"oled_test\":{";
  Wire.beginTransmission(0x3C);
  json += "\"0x3C\":\"" + String(Wire.endTransmission() == 0 ? "FOUND" : "not found") + "\",";
  Wire.beginTransmission(0x3D);
  json += "\"0x3D\":\"" + String(Wire.endTransmission() == 0 ? "FOUND" : "not found") + "\"";
  json += "}}}";

  Serial.println("I2C Scan results: " + json);

  server.send(200, "application/json", json);
}

// Setup web server
void ICACHE_FLASH_ATTR setupWebServer() {
  httpUpdater.setup(&server, "/update", "admin", "admin");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/config", HTTP_POST, handleConfigSave);
  server.on("/debug", HTTP_GET, handleDebug);
  server.on("/test-ntp", HTTP_GET, handleTestNTP);
  server.on("/test-display", HTTP_GET, handleTestDisplay);
  server.on("/api/time", HTTP_GET, handleAPITime);
  server.on("/api/status", HTTP_GET, handleAPIStatus);
  server.on("/api/debug", HTTP_GET, handleAPIDebug);
  server.on("/api/weather", HTTP_GET, handleAPIWeather);
  server.on("/api/config", HTTP_GET, handleAPIConfigExport);
  server.on("/api/config", HTTP_POST, handleAPIConfigImport);
  server.on("/api/eeprom-clear", HTTP_POST, handleEEPROMClear);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/i2c-scan", HTTP_GET, handleI2CScan);

  server.begin();
  Serial.println("Web server started");

  if (MDNS.begin(config.hostname)) {
    Serial.printf("mDNS responder started: %s.local\n", config.hostname);
    MDNS.addService("http", "tcp", 80);
  }
}
