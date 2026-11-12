# Project Utilities for the ESP32

[![CI](https://github.com/Cameloah/ProjectUtils-esp32/actions/workflows/ci.yml/badge.svg)](https://github.com/Cameloah/ProjectUtils-esp32/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/built%20with-PlatformIO-orange)](https://platformio.org/)

A project starter library for ESP32 firmware (Arduino framework, PlatformIO). One `begin()` call
gives a project:

- **WiFi setup** with a captive portal: an unconfigured device opens an access point and shows
  its settings page
- **Settings page** at `/settings` (device name, WiFi, static IP, updates), embedded in the
  firmware, so no filesystem image is needed
- **Persistent typed settings** stored in NVS (`pu::Settings`)
- **Web console** at `/webserial` that mirrors serial output and input
- **RAM log** of recent events with real timestamps, readable at `/api/pu/log`
- **Time** via NTP with a configurable timezone
- **Firmware upload** from PlatformIO (ArduinoOTA) and **updates from GitHub releases**
- Optional **admin password** protecting all of the above

> Version 2 is a redesign and not source-compatible with version 1.
> See [Migrating from v1](#migrating-from-v1).

## Installation

Add the library to your project's `platformio.ini`. Its dependencies are installed automatically.

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = https://github.com/Cameloah/ProjectUtils-esp32.git#v2.0.0
; TLS, web server and web console need more room than the default app partition offers
board_build.partitions = min_spiffs.csv
```

Works with Arduino core 2.x (PlatformIO `espressif32`) and 3.x (pioarduino). A git submodule in
`lib/` works as well.

## Quick start

```cpp
#include <Arduino.h>
#include <ProjectUtils.h>

pu::Config config;

void setup() {
    Serial.begin(115200);

    config.deviceName = "My ESP32";
    config.firmwareVersion = pu::Semver{0, 0, 1};
    ProjectUtils.begin(config);

    ProjectUtils.server().on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "Hello World");
    });
}

void loop() {
    ProjectUtils.loop();
}
```

First start:

1. The device opens an access point named after the device, e.g. `My ESP32 A1B2C3`.
2. Connect to it; the settings page opens (or browse to `http://192.168.4.1/settings`).
3. Enter the WiFi name and password and submit. The device restarts and joins the network.
4. From then on it is reachable at `http://<devicename>.local/settings` (the name without
   spaces, lowercase) or via its IP address, which is printed on the serial console.

A complete project is in [`examples/basic`](examples/basic).

## Configuration

Everything is set through `pu::Config` before calling `begin()`.

| Field | Default | Meaning |
|---|---|---|
| `deviceName` | `"ESP32 Device"` | Default name; the user can change it on the settings page |
| `firmwareVersion` | `{0, 0, 0}` | Version of your firmware, compared against GitHub releases |
| `apMode` | `ApMode::OnFailure` | `OnFailure`: access point only if WiFi is unconfigured or unreachable. `Always`: keep it open |
| `apPassword` | empty | Password of the setup access point (at least 8 characters). Empty = open |
| `apTimeoutMs` | `0` | Close the access point after this time if nobody uses it. `0` = never |
| `wifiConnectTimeoutMs` | `10000` | How long `begin()` waits for the WiFi connection |
| `adminUser` / `adminPassword` | `"admin"` / empty | See [Security](#security) |
| `timezone` | Central Europe | POSIX TZ string, e.g. `"EST5EDT,M3.2.0,M11.1.0"` |
| `ntpServer` | `"pool.ntp.org"` | Time server |
| `githubRepo` | empty | `"user/repository"`; enables updates from GitHub releases |
| `githubFirmwareAsset` | `"firmware.bin"` | Release asset name; `<version>` is replaced by the tag |
| `githubFilesystemAsset` | empty | Optional filesystem image asset of your project |
| `enableWebSerial`, `enableOta` | `true` | Switch services off |
| `serial` | `nullptr` | Stream used by the console; `nullptr` = `Serial` |

Building with `-D PU_DISABLE_WEBSERIAL` removes the web console from the firmware entirely.

## Security

Without `adminPassword`, anyone on the same network can change the WiFi settings, read the
console and upload firmware. That is acceptable on a trusted home network and nowhere else; the
device logs a warning at boot.

With `config.adminPassword = "..."` the settings page, the JSON API, the web console and
ArduinoOTA uploads all require it (user name `admin` unless changed). Protect your own routes
the same way:

```cpp
ProjectUtils.server().on("/secret", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!ProjectUtils.web().authorize(request)) {
        return;
    }
    request->send(200, "text/plain", "for your eyes only");
});
```

The stored WiFi password is never sent to the browser or printed. Two limits to be aware of:
the web interface is plain HTTP, and downloads from GitHub do not verify the server certificate.

## Modules

### Settings (NVS)

```cpp
pu::Settings appSettings;

appSettings.begin("app", &ProjectUtils.log());   // NVS namespace, max. 15 characters
appSettings.define("timeoutBoot", 20);           // key (max. 15 characters) and default
appSettings.define("factor", 1.23f);
appSettings.define("name", "test device");
appSettings.define("autostart", true);
appSettings.loadAll();                           // stored values replace the defaults

int timeout = appSettings.getInt("timeoutBoot");
String name = appSettings.getString("name");
appSettings.set("autostart", false);             // saved to flash immediately
appSettings.set("factor", 2.5f, false);          // RAM only; persist later with save("factor")
```

Types: `int`, `float`, `double`, `bool`, `String`, and raw bytes via `defineBlob` / `setBlob` /
`getBlob`. Getters return the value, or the given fallback if the key is unknown or has another
type (which is also logged). `ProjectUtils.settings()` holds the library's own settings.

### Console

`ProjectUtils.console()` is a `Stream` like `Serial`. Output goes to USB serial and to the web
console at `/webserial`; input from both arrives in the same stream (a line typed in the web
console ends with `\n`).

```cpp
Stream& console = ProjectUtils.console();
console.println("Hello World");
if (console.available()) {
    String line = console.readStringUntil('\n');
}
```

### Log

Keeps the last 30 events in RAM and echoes them to the console.

```cpp
ProjectUtils.log().info("Startup complete, %d sensors", count);
ProjectUtils.log().warn("Battery low");
ProjectUtils.log().error("Sensor read failed: %d", code);
ProjectUtils.log().printTo(ProjectUtils.console());
```

Timestamps are uptime until the clock is synchronised and local time afterwards, including for
entries made before the sync. The log is also available at `http://<device>/api/pu/log`.

### Time

```cpp
if (ProjectUtils.clock().isSynced()) {
    String now = ProjectUtils.clock().now();   // "2026-01-31 12:34:56.789"
}
```

The standard C functions `time()` and `localtime_r()` return correct local time as well.

### Firmware updates

**From PlatformIO:** set `upload_protocol = espota` and `upload_port = <devicename>.local`
(plus `upload_flags = --auth=<adminPassword>` if a password is set).

**From GitHub releases:** set `config.githubRepo`, then for every release

1. raise `config.firmwareVersion`,
2. publish a public GitHub release tagged `vX.Y.Z` with the firmware binary attached under the
   name given by `githubFirmwareAsset` (PlatformIO writes it to `.pio/build/<env>/firmware.bin`).

The settings page can check for and install the latest release, or install any specific version
(downgrades included). With "Automatic Updates" enabled there, the device checks at every boot.

### HTTP API

Used by the settings page; requires the admin password if one is set.

| Request | Purpose |
|---|---|
| `GET /api/pu/status` | Device, WiFi and update state as JSON |
| `GET /api/pu/log` | RAM log as text |
| `POST /api/pu/settings` | Form fields `deviceName`, `ssid`, `pass`, `ip`, `gateway`; restarts the device |
| `POST /api/pu/autoupdates` | `enabled=true\|false` |
| `POST /api/pu/update/check` | Start an update check; poll `status` for the result |
| `POST /api/pu/update/install` | Install `version=vX.Y.Z`, or the latest if omitted |
| `POST /api/pu/restart` | Restart the device |

## Migrating from v1

Devices keep their stored WiFi credentials, device name and auto-update choice when flashed
with a v2-based firmware.

| v1 | v2 |
|---|---|
| `extra_scripts = pre:lib/ProjectUtils-esp32/copy_lib_data.py` | Remove the line. Dependencies come from the library manifest |
| Library pages copied into your `data/` folder, `spiffs.bin` required | Remove `settings.html` and `style.css` from `data/`; the pages are part of the firmware. `/style.css` is still served for your own pages |
| `include/main.h` macros, `include/version.h` | `pu::Config` fields (table below) |
| `#include "main_project_utils.h"` | `#include <ProjectUtils.h>` |
| `project_utils_init("name")` / `project_utils_update()` | `ProjectUtils.begin(config)` / `ProjectUtils.loop()` |
| `server.on(...)` | `ProjectUtils.server().on(...)` |
| `DualSerial.println(...)` | `ProjectUtils.console().println(...)`; call `Serial.begin()` yourself |
| `MemoryModule m("ns"); m.addParameter(k, v);` | `pu::Settings s; s.begin("ns"); s.define(k, v);` |
| `*m.getInt(k)`, `*m.getBool(k)` (pointers) | `s.getInt(k)`, `s.getBool(k)` (values) |
| `m.set(k, v)` kept the value in RAM only | `s.set(k, v)` saves immediately; pass `false` as third argument for RAM only |
| `m.loadAllStrict()` | `s.loadAll()`; missing keys keep their defaults |
| `ram_log_notify(RAM_LOG_INFO, "text", true)` | `ProjectUtils.log().info("text")` |
| `ram_log_notify(RAM_LOG_ERROR_..., code)` | `ProjectUtils.log().error("what failed: %d", code)` |
| `ram_log_print_log()` | `ProjectUtils.log().printTo(ProjectUtils.console())` |
| `network_manager_is_connected()` | `ProjectUtils.wifi().isConnected()` |
| `ntp` object (`sstaub/NTP`) | `ProjectUtils.clock()` or the C time functions |

| v1 macro | v2 `Config` field |
|---|---|
| `SYS_CONTROL_STAT_IP` | Removed. A static IP is used whenever one is entered on the settings page |
| `SYS_CONTROL_WEBSERIAL` | `enableWebSerial` (on by default) |
| `AP_PASSWORD` | `apPassword` |
| `AP_VERBOSITY` 1 / 2 | `apMode = ApMode::OnFailure` / `ApMode::Always` |
| `AP_TIMEOUT` | `apTimeoutMs` |
| `TIMEOUT_WIFI_CONNECT_MS` | `wifiConnectTimeoutMs` |
| `TIMEZONE_SEC_OFFSET`, `DAYLIGHT_SAVING_SEC_OFFSET` | `timezone` (POSIX TZ string) |
| `FW_VERSION_MAJOR/MINOR/PATCH` | `firmwareVersion = pu::Semver{major, minor, patch}` |
| `GITHUB_REPO` | `githubRepo` |
| `GITHUB_FW_BIN` | `githubFirmwareAsset` |
| `GITHUB_FS_BIN` | `githubFilesystemAsset` (only if your project ships its own filesystem image) |

Other changes in behaviour are listed in the [changelog](CHANGELOG.md).

## Development

Coding conventions are enforced by tools rather than described here: formatting by
`clang-format`, naming and bug-prone patterns by `clang-tidy` and `cppcheck`, Python by `ruff`.

```sh
uv tool install platformio          # or: pipx install platformio
uv tool install pre-commit
pre-commit install                  # format and lint on every commit

pio test -e native                  # unit tests on the host
pio run -d examples/basic           # build the example for both Arduino core versions
pio check -e check --fail-on-defect=low
python3 scripts/run_clang_tidy.py   # needs clang-tidy on the PATH
```

The web pages live in `web/`. After editing them, `scripts/embed_web_assets.py` regenerates
`src/pu/web_assets.gen.h`; the pre-commit hook does this automatically. GitHub Actions runs all
of the above on every push and pull request.

## License

MIT, see [LICENSE](LICENSE).
