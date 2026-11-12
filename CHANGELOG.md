# Changelog

## 2.0.0

Redesign of the library. Not source-compatible with 1.x; see "Migrating from v1" in the README.
Settings stored on devices by 1.x firmware (WiFi credentials, device name, auto-update choice)
are kept.

### Changed

- Packaged as a regular PlatformIO library (`library.json`). The build script that rewrote the
  project's `platformio.ini` and copied files into its `data/` folder is gone.
- The settings page and stylesheet are embedded in the firmware. No filesystem image is needed,
  so a freshly flashed device shows its setup page straight away.
- Configuration through a `pu::Config` struct instead of macros in `include/main.h` and
  `include/version.h`.
- One `ProjectUtils` object owns all modules; the global variables `server`, `wifi_config`,
  `wifi_info`, `device_settings`, `ntp` and `DualSerial` no longer exist.
- `MemoryModule` is now `pu::Settings`: getters return values instead of pointers, `set()`
  persists by default, string literals are stored as strings, access is thread-safe.
- The console wraps `Serial` instead of creating a second driver on the same UART. A line typed
  in the web console now ends with a newline.
- Log messages are printf-style with three levels. Device names get a suffix derived from the
  chip ID instead of a random number that changed on every boot.
- Time comes from the SNTP client of the ESP32 core with a POSIX timezone string; the
  `sstaub/NTP` dependency and the fixed Central European rules are gone.
- A static IP is a runtime setting: used when entered on the settings page, DHCP otherwise.
- WiFi networks without a password are accepted.
- The access point timeout counts from when the access point opened, and a fallback access
  point closes by itself once WiFi is back. Lost connections are retried.
- Web server libraries switched to the maintained `ESP32Async` forks with pinned versions;
  WebSerial updated to 2.x.
- The web interface uses a JSON API under `/api/pu/`; changes are made with POST requests.
- GitHub updates: firmware asset name defaults to `firmware.bin`; the filesystem image is
  optional and both images are written before the device restarts.

### Added

- Optional admin password for the settings page, API, web console and ArduinoOTA.
- `GET /api/pu/log` and `POST /api/pu/restart`.
- Unit tests for the hardware-independent parts, an example project, and CI that builds for
  Arduino core 2.x and 3.x.
- Enforced formatting and linting (clang-format, clang-tidy, cppcheck, ruff, pre-commit).

### Fixed

- Reading past the end of the log buffer once it had wrapped; log printed out of order.
- Web console output read past the end of the written data; console input could overflow the
  caller's buffer.
- Boolean settings could load a random value when not stored yet.
- Log timestamps were wrong after about 71 minutes of uptime.
- Saving settings or checking for updates blocked the network task, so replies often never
  reached the browser; one request never got a reply at all.
- The WiFi password was sent to every browser opening the settings page and printed to the
  serial and web console.
- Update check: memory leak on errors, crash on release tags not of the form `vX.Y.Z`, version
  parts above 255 compared wrongly, a failed installation broke the next attempt.
- Settings were not explicitly committed to flash.
