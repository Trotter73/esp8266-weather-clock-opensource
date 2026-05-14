# Contributing to ESP8266 Weather Clock

Thank you for your interest in contributing! This project welcomes improvements, bug fixes, and new features.

## How to Contribute

### Reporting Bugs

If you find a bug, please open an issue with:

- Clear description of the problem
- Steps to reproduce
- Expected vs actual behavior
- Serial console output (if applicable)
- Firmware version

### Suggesting Features

Feature requests are welcome! Please include:

- Use case description
- Why this would be useful
- Any implementation ideas

### Pull Requests

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/my-new-feature`
3. **Test your changes**:
   - Compile successfully
   - Test on real hardware if possible
   - Check memory usage (IRAM must stay < 95%)
4. **Follow the code style**:
   - Use `ICACHE_FLASH_ATTR` for non-critical functions
   - Avoid String concatenation in loops
   - Document state machines with comments
5. **Commit with clear messages**: Explain what and why, not how
6. **Submit PR** with description of changes

### Code Guidelines

**Memory Safety:**

- Check IRAM usage after adding code
- Use fixed-size buffers instead of dynamic allocation where possible
- Prefer `snprintf` over String concatenation

**Async Architecture:**

- Keep loop() non-blocking (no delay() calls)
- Use state machines for multi-step operations
- Add exponential backoff to network operations

**Testing:**

- Test on ESP-01S hardware (1MB flash, 80KB RAM)
- Verify OTA updates work
- Check 24h stability

## Development Setup

### Requirements

- Arduino IDE 1.8.x or 2.x
- ESP8266 board support (v3.0.0+)
- Libraries (see README)

### Building

```bash
# Arduino IDE: Sketch → Verify/Compile
# Or use arduino-cli:
arduino-cli compile --fqbn esp8266:esp8266:generic firmware/weather_clock
```

### Testing

```bash
# Upload via FTDI (first time)
arduino-cli upload -p /dev/cu.usbserial* --fqbn esp8266:esp8266:generic

# Upload via OTA (subsequent)
curl -u admin:admin -F "file=@build/*.bin" http://192.168.x.x/update
```

## Project Structure

```
esp8266-weather-clock-opensource/
├── firmware/               # Main firmware source (weather_clock/)
├── docs/                   # Documentation
├── images/                 # Photos and screenshots
├── README.md               # Main documentation
└── LICENSE                 # MIT License
```

## Communication

- **Issues**: Bug reports and feature requests
- **Discussions**: General questions and ideas
- **Pull Requests**: Code contributions

## Code of Conduct

Be respectful, constructive, and helpful. We're all here to learn and build cool stuff.

## Questions?

Open an issue or discussion - happy to help!
