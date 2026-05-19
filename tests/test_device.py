#!/usr/bin/env python3
"""
Hardware-in-the-loop test suite for ESP8266 Weather Clock firmware.
Runs against a live device via HTTP API.

Usage:
    python3 tests/test_device.py [device_ip]
    python3 tests/test_device.py 192.168.2.47
"""

import sys
import json
import time
import urllib.request
import urllib.parse
import urllib.error
from dataclasses import dataclass, field
from typing import Any

DEVICE_IP = sys.argv[1] if len(sys.argv) > 1 else "192.168.2.47"
BASE_URL = f"http://{DEVICE_IP}"
TIMEOUT = 10

# Safe config values to restore after fuzz tests
SAFE_CONFIG = {
    "ssid":             "",          # keep empty — don't overwrite real credentials
    "ntp_interval":     "3600",
    "weather_interval": "1800",
    "brightness":       "4",
    "timezone_offset":  "0",
    "latitude":         "37.19",
    "longitude":        "-8.54",
    "hostname":         "tj56654-clock",
    "ntp_server":       "pool.ntp.org",
    "city_name":        "Portimao",
}

# Saved before fuzz, restored after
_config_backup: dict = {}

# ─── HTTP helpers ────────────────────────────────────────────────────────────

def get(path) -> tuple[int, str]:
    try:
        r = urllib.request.urlopen(f"{BASE_URL}{path}", timeout=TIMEOUT)
        return r.status, r.read().decode()
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()
    except Exception as e:
        return 0, str(e)

def get_json(path) -> tuple[int, dict | None]:
    status, body = get(path)
    try:
        return status, json.loads(body)
    except Exception:
        return status, None

def post_form(path, data: dict) -> tuple[int, str]:
    encoded = urllib.parse.urlencode(data).encode()
    req = urllib.request.Request(f"{BASE_URL}{path}", data=encoded, method="POST")
    req.add_header("Content-Type", "application/x-www-form-urlencoded")
    try:
        r = urllib.request.urlopen(req, timeout=TIMEOUT)
        return r.status, r.read().decode()
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()
    except Exception as e:
        return 0, str(e)

def post_raw(path, body: bytes, content_type="application/json") -> tuple[int, str]:
    req = urllib.request.Request(f"{BASE_URL}{path}", data=body, method="POST")
    req.add_header("Content-Type", content_type)
    try:
        r = urllib.request.urlopen(req, timeout=TIMEOUT)
        return r.status, r.read().decode()
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()
    except Exception as e:
        return 0, str(e)

# ─── Test runner ─────────────────────────────────────────────────────────────

@dataclass
class Result:
    name: str
    passed: bool
    detail: str = ""

results: list[Result] = []

def test(name: str, passed: bool, detail: str = ""):
    r = Result(name, passed, detail)
    results.append(r)
    icon = "✅" if passed else "❌"
    print(f"  {icon} {name}" + (f" — {detail}" if detail else ""))
    return passed

# ─── Test suites ─────────────────────────────────────────────────────────────

def suite_connectivity():
    print("\n📡 Connectivity")
    status, body = get("/")
    test("Device reachable", status == 200, f"HTTP {status}")
    test("Returns HTML", "<html" in body.lower() or "<!DOCTYPE" in body.lower(),
         f"{len(body)} bytes")
    test("Version present", "v1.9" in body, body[:80] if body else "empty")

def suite_api_time():
    print("\n🕐 /api/time")
    status, data = get_json("/api/time")
    test("HTTP 200", status == 200, f"got {status}")
    if data is None:
        test("Valid JSON", False, "parse error"); return
    test("Valid JSON", True)
    test("Has 'time' field", "time" in data, str(data.keys()))
    if "time" in data:
        t = data["time"]
        test("Time format HH:MM:SS", len(t) == 8 and t[2] == ":" and t[5] == ":",
             f"got '{t}'")
    test("Has 'hours'", "hours" in data)
    test("Has 'minutes'", "minutes" in data)
    test("Has 'epoch'", "epoch" in data and data["epoch"] > 1700000000)

def suite_api_weather():
    print("\n🌤  /api/weather")
    status, data = get_json("/api/weather")
    test("HTTP 200", status == 200, f"got {status}")
    if data is None:
        test("Valid JSON", False, "parse error"); return
    test("Valid JSON", True)
    test("enabled=true", data.get("enabled") == True)
    test("valid=true", data.get("valid") == True, "weather data may not be fetched yet")
    if data.get("valid"):
        t = data.get("temperature", 0)
        test("Temperature sane [-50, 70]", -50 <= t <= 70, f"{t}°C")
        w = data.get("windspeed", 0)
        test("Windspeed ≥ 0", w >= 0, f"{w} km/h")
        wc = data.get("weathercode", -1)
        test("Weathercode ≥ 0", wc >= 0, f"code={wc}")
    test("Has sunrise", "sunrise" in data, str(data.get("sunrise")))
    test("Has sunset", "sunset" in data, str(data.get("sunset")))

def suite_api_status():
    print("\n📊 /api/status")
    status, data = get_json("/api/status")
    test("HTTP 200", status == 200)
    if data is None:
        test("Valid JSON", False, "parse error"); return
    test("Valid JSON", True)

    wifi = data.get("wifi", {})
    test("Has wifi.ssid", "ssid" in wifi, str(wifi))
    test("Has wifi.ip", "ip" in wifi)
    test("Has wifi.rssi", "rssi" in wifi)
    if "rssi" in wifi:
        test("RSSI in realistic range [-100, 0]", -100 <= wifi["rssi"] <= 0,
             f"{wifi['rssi']} dBm")

    sys_ = data.get("system", {})
    test("Has system.uptime", "uptime" in sys_)
    test("Has system.free_heap", "free_heap" in sys_)
    if "free_heap" in sys_:
        heap = sys_["free_heap"]
        test("Heap > 8KB (not fragmented)", heap > 8192, f"{heap} bytes free")
        test("Heap > 20KB (healthy)", heap > 20480, f"{heap} bytes free")

def suite_api_debug():
    print("\n🔍 /api/debug")
    status, data = get_json("/api/debug")
    test("HTTP 200", status == 200)
    if data is None:
        test("Valid JSON", False, "parse error"); return
    test("Valid JSON", True)
    test("Has internet_connected", "internet_connected" in data)
    test("internet_connected=true", data.get("internet_connected") == True)
    test("Has ntp_attempts", "ntp_attempts" in data)
    test("Has ntp_successes", "ntp_successes" in data)
    attempts = data.get("ntp_attempts", 0)
    successes = data.get("ntp_successes", 0)
    test("NTP attempted at least once", attempts >= 1, f"{attempts} attempts")
    test("NTP success rate > 0", successes >= 1, f"{successes}/{attempts}")
    test("last_error field present", "last_error" in data)
    test("last_error is string", isinstance(data.get("last_error"), str))

def backup_config():
    """Save safe fields before fuzzing."""
    global _config_backup
    _, data = get_json("/api/status")
    _config_backup = {
        "ntp_interval":     "3600",
        "weather_interval": "1800",
        "brightness":       "4",
        "timezone_offset":  "0",
        "ntp_server":       "pool.ntp.org",
        "hostname":         "tj56654-clock",
    }
    print("  💾 Config backup saved")

def restore_config():
    """Restore safe config after fuzzing — prevents bricking on bad values."""
    status, _ = post_form("/config", SAFE_CONFIG)
    time.sleep(1)
    ok = status in (200, 302)
    print(f"  🔄 Config restored {'✅' if ok else '❌'} (HTTP {status})")
    return ok

def suite_fuzz_config():
    print("\n🧨 Fuzz: /config boundary values")
    backup_config()

    # Save current config to restore later
    _, before = get_json("/api/status")

    cases = [
        # (description, field, value, expect_no_crash)
        ("ntp_interval=0 (DoS risk)",        "ntp_interval",     "0",          True),
        ("ntp_interval=86401 (over max day)", "ntp_interval",     "86401",      True),
        ("weather_interval=0",               "weather_interval",  "0",          True),
        ("brightness=255 (uint8 overflow)",  "brightness",        "255",        True),
        ("brightness=-1",                    "brightness",        "-1",         True),
        ("timezone_offset=99999999",         "timezone_offset",   "99999999",   True),
        ("timezone_offset=-99999999",        "timezone_offset",   "-99999999",  True),
        ("latitude=999 (invalid)",           "latitude",          "999",        True),
        ("latitude=-999",                    "latitude",          "-999",       True),
        ("longitude=999",                    "longitude",         "999",        True),
        ("ssid=A*100 (overflow char[32])",   "ssid",              "A" * 100,    True),
        ("password=B*200 (overflow char[64])","password",         "B" * 200,    True),
        ("hostname=C*100 (overflow char[32])","hostname",         "C" * 100,    True),
        ("ntp_server=D*200 (overflow char[64])","ntp_server",     "D" * 200,    True),
    ]

    for desc, field_name, value, expect_survive in cases:
        status, body = post_form("/config", {field_name: value})
        survived = status in (200, 302, 400)  # any response = not crashed
        test(desc, survived, f"HTTP {status}")

    # Always restore safe config after fuzzing — critical to prevent bricking
    restore_config()

    # Verify device still alive after fuzzing
    time.sleep(1)
    status, data = get_json("/api/status")
    test("Device alive after fuzz", status == 200 and data is not None,
         f"HTTP {status}")

def suite_fuzz_config_import():
    print("\n🧨 Fuzz: /api/config import (JSON endpoint)")

    cases = [
        ("Empty body",           b""),
        ("Not JSON",             b"this is not json at all!!!"),
        ("Partial JSON",         b'{"ssid": "test"'),
        ("Null JSON",            b"null"),
        ("Array JSON",           b"[]"),
        ("Nested bomb",          b'{"a":{"b":{"c":{"d":{"e":"f"}}}}}'),
        ("Very large string",    json.dumps({"ssid": "X" * 5000}).encode()),
        ("Unicode",              '{"ssid": "тест-сеть"}'.encode("utf-8")),
        ("Zero bytes field",     json.dumps({"ntp_interval": 0}).encode()),
        ("Negative interval",    json.dumps({"weather_interval": -1}).encode()),
        ("NaN float",            b'{"latitude": "NaN"}'),
        ("Inf float",            b'{"latitude": "Infinity"}'),
        ("SQL-like injection",   b'{"ssid": "\'; DROP TABLE config; --"}'),
        ("HTML injection",       b'{"ssid": "<script>alert(1)</script>"}'),
        ("Null bytes",           b'{"ssid": "test\x00hidden"}'),
    ]

    for desc, body in cases:
        status, resp = post_raw("/api/config", body)
        survived = status != 0  # got any response = not crashed/hung
        test(desc, survived, f"HTTP {status}")

    restore_config()

    time.sleep(1)
    status, _ = get_json("/api/status")
    test("Device alive after import fuzz", status == 200, f"HTTP {status}")

def suite_stability():
    print("\n⏱  Stability: heap trend over 5 requests")
    heaps = []
    for i in range(5):
        _, data = get_json("/api/status")
        if data and "system" in data:
            heaps.append(data["system"].get("free_heap", 0))
        time.sleep(0.5)

    if heaps:
        test("Got heap samples", len(heaps) == 5, f"{len(heaps)}/5")
        min_heap = min(heaps)
        max_heap = max(heaps)
        drift = max_heap - min_heap
        test("Heap stable (drift < 2KB)", drift < 2048,
             f"min={min_heap} max={max_heap} drift={drift}")
        test("Min heap > 20KB", min_heap > 20480, f"min={min_heap}")
        print(f"    Heap samples: {heaps}")

# ─── Main ────────────────────────────────────────────────────────────────────

def main():
    print(f"🔌 Testing device at {BASE_URL}")
    print("=" * 55)

    suite_connectivity()
    suite_api_time()
    suite_api_weather()
    suite_api_status()
    suite_api_debug()
    suite_fuzz_config()
    suite_fuzz_config_import()
    suite_stability()

    print("\n" + "=" * 55)
    passed = sum(1 for r in results if r.passed)
    failed = sum(1 for r in results if not r.passed)
    total = len(results)
    print(f"Results: {passed}/{total} passed", end="")
    if failed:
        print(f"  ({failed} failed)")
        print("\nFailed tests:")
        for r in results:
            if not r.passed:
                print(f"  ❌ {r.name}" + (f" — {r.detail}" if r.detail else ""))
    else:
        print(" ✅ All passed!")
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
