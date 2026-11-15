# Changelog

## Unreleased

### Added

- Factory reset on the Device info page: erases everything stored in flash (WiFi, admin password,
  all settings) and restarts into the setup access point. Confirmed by typing the device name,
  which the device checks as well. `POST /api/pu/factory-reset` with `confirm`, and
  `ProjectUtils.factoryReset()` for the project's own code (e.g. a reset button).
- `WifiManager::defaultName()`; `/api/pu/info` reports it as `defaultName`.

### Fixed

- "Search for networks" while the setup access point is open: a search then takes longer than
  the 6 s after which the Arduino core reports a failure, so the page showed "No networks found";
  and while the device kept trying to reach a stored but unreachable network, the search could
  not start at all. Searches now count as running for up to 15 s, a second request joins a
  running search, and connection attempts pause while searching.

## 2.3.0

### Added

- Admin password set on the settings page and stored on the device, instead of in the code. It
  stays out of the repository and the firmware file. Protects all pages, the API, the web console
  and OTA uploads, as before.
- `password --clear` on the USB serial console removes the admin password (recovery). Typed in
  the web console it has no effect.
- `Console::setLineFilter()` and `Console::poll()`: the console hands on input from the USB
  stream line by line so the library can take out its own commands.

### Changed

- Login uses HTTP digest authentication; basic logins are refused, so the password never travels
  readable over the network.
- `Config::adminPassword` is deprecated (readable in the firmware file); it only applies while no
  password is set on the device.
- Settings page: the WiFi password box shows a stand-in while a password is stored; emptying it
  means a network without password (the "Open network" switch is gone). `POST /api/pu/settings`:
  `pass` is stored as sent, empty included; `clearPass` is gone. New `adminProtect`, `adminPass`.
- Access point settings show the firmware's values directly instead of "Default: ...", without
  the "Empty: ..." hints; the password is a switch. Memory page: plain type names in "Add".
- Web pages wait through a restart that ends with a login prompt.

## 2.2.0

### Added

- Static IP address on the settings page as a switch, with subnet mask and DNS servers. It is
  prefilled with what the router hands out and checked on the device (valid mask, gateway in
  the same subnet, usable address). A new address is tried out: unless it is confirmed within
  two minutes at the new address, the device restores the previous settings. The page follows
  the device to its new address. New API: `POST /api/pu/network/confirm` and `.../revert`.
- Access point section on the settings page: mode, timeout, grace period and password per
  device, on top of the firmware defaults in `pu::Config`. `Config::apLocked` makes them
  read-only.
- `ApMode::Never` (access point only while no WiFi is stored) and `Config::apGraceMs`.
- Memory page (`/memory`): every value stored in flash, by area. Change values, reset entries
  the firmware defines to their default, delete entries it does not define, add new entries to
  the project's areas or a new one. Secret values never reach the browser.
- `Settings::reset(key)`, `Settings::markSecret(key)`, `Settings::forEach()`, `Settings::all()`.
- Console page: uses the full window width and height, "Wrap lines" switch (off by default,
  remembered by the browser; the log on the device info page shares it). Page options are
  switches instead of check boxes.

### Changed

- `ApMode::OnFailure` now also opens the access point while running, after WiFi has been lost
  for `apGraceMs` (2 minutes), not only at start-up.
- The access point timeout applies in every mode, including `Always`, and counts from when the
  last client left. When no WiFi is stored, the access point opens regardless of the mode.
- `/api/pu/status`: `wifi.localIP` and `wifi.gateway` are replaced by the `address` object;
  new `ap` object. A static IP is now only used while the switch is on; devices that had an
  address entered are switched on automatically.

## 2.1.1

### Fixed

- CI failed on master: GitHub's runner installed an older clang-tidy (18) than the one used
  locally (23.1.4), which reports different findings. CI now installs clang-tidy 23.1.4, and
  `scripts/run_clang_tidy.py` warns when a different version is used. No changes to the
  library code.

## 2.1.0

### Added

- `Page::password()`: an input for secrets such as API keys or MQTT passwords. The value is
  stored like any setting but never sent back to the browser.

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
  The WebSerial dependency is removed; the console is part of the library.
- The web interface uses a JSON API under `/api/pu/`; changes are made with POST requests.
- GitHub updates: firmware asset name defaults to `firmware.bin`; the filesystem image is
  optional and both images are written before the device restarts.

### Added

- A set of web pages with one shared menu and style, all embedded in the firmware: settings,
  update, console, device info. The menu is drawn on every page by `/pu/app.js`.
- `pu::Page`: a project home page (and further pages) declared in C++ with inputs bound to
  settings, buttons, hold-to-run buttons with a safety timeout, live values and status lights.
  `WebUi::addPage()` and `addMenuLink()` for hand-written pages.
- Firmware and filesystem upload from the browser (`/update`).
- Device info page and `GET /api/pu/info` (memory, uptime, reason of the last restart, ...).
- Network search on the settings page.
- Own web console (`/console`) with scrollback, timestamps, scroll lock and download.
- Optional admin password for the settings page, API, web console and ArduinoOTA.
- `GET /api/pu/log` and `POST /api/pu/restart`.
- Unit tests for the hardware-independent parts, an example project, and CI that builds for
  Arduino core 2.x and 3.x.
- Enforced formatting and linting (clang-format, clang-tidy, cppcheck, ruff, pre-commit).
- On-device test suite (`integration/`, pytest) that checks the example firmware on a real
  ESP32 over WiFi, and an `esp32s3` environment for the example.

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
