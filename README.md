# Project Utilities for the ESP32

[![CI](https://github.com/Cameloah/ProjectUtils-esp32/actions/workflows/ci.yml/badge.svg)](https://github.com/Cameloah/ProjectUtils-esp32/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/built%20with-PlatformIO-orange)](https://platformio.org/)

A project starter library for ESP32 firmware (Arduino framework, PlatformIO). One `begin()` call
gives a project:

- **WiFi setup** with a captive portal: an unconfigured device opens an access point and shows
  its settings page, with a list of the networks in range
- **A set of web pages with one shared menu and style**, all embedded in the firmware (no
  filesystem image, no internet connection needed):
  settings, firmware update, console, device info and log
- **Your own home page declared in C++**: inputs bound to settings, buttons, hold-to-run
  buttons and live values, without writing HTML or request handlers
- **Persistent typed settings** stored in NVS (`pu::Settings`)
- **Web console** that mirrors serial output and input
- **RAM log** of recent events with real timestamps
- **Time** via NTP with a configurable timezone
- **Firmware updates** by upload in the browser, from PlatformIO (ArduinoOTA) and from GitHub
  releases
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
lib_deps = https://github.com/Cameloah/ProjectUtils-esp32.git#v2.2.0
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

    pu::Page& home = ProjectUtils.web().home("My ESP32");
    home.button("Say hello", []() { ProjectUtils.console().println("Hello World"); });
    home.value("Running for", []() { return millis() / 1000.0; }).unit("s");
}

void loop() {
    ProjectUtils.loop();
}
```

First start:

1. The device opens an access point named after the device, e.g. `My ESP32 A1B2C3`.
2. Connect to it; the settings page opens (or browse to `http://192.168.4.1/settings`).
3. Enter the WiFi name and password and submit. The device restarts and joins the network.
4. From then on it is reachable at `http://<devicename>.local/` (the name without spaces,
   lowercase) or via its IP address, which is printed on the serial console.

A complete project is in [`examples/basic`](examples/basic).

## Web pages

Every page has the same style and the same list of pages: a rail on the left on wide screens,
a drawer behind the menu button on phones. Light and dark follow the browser. The list shows
your project's pages first, then the built-in ones:

| Page | Address | Contents |
|---|---|---|
| Your home page | `/` | Whatever you declare, see below. Without one, `/` shows the device info |
| Settings | `/settings` | Device name, WiFi (with network search), static IP address, access point |
| Update | `/update` | Upload a firmware or filesystem file; online updates if `githubRepo` is set |
| Memory | `/memory` | Every stored value: change it, reset it to its default, delete leftovers, add new ones |
| Console | `/console` | Serial output and input over the full window width, with timestamps, scroll lock, line wrap and download |
| Device info | `/info` | Firmware, network, memory, uptime, reason of the last restart, log |

### Static IP address

Switching "Static IP address" on fills in what the router currently hands out; change the IP
address and save. The device checks that address, subnet mask and gateway fit together, then
restarts at the new address, and the settings page follows it there. Confirm the address with
"Keep" within two minutes. If nobody confirms (because the device cannot be reached at the new
address, for example), it goes back to the previous settings by itself and the page reloads at
the old address. Changed through the setup access point, the address applies right away
without this trial, since nobody could reach the device at its new address from there.

### Memory

Lists everything the device keeps in flash, by area (the NVS namespace of each `pu::Settings`
object): your project's areas first, the library's own (`project_utils`) behind "Show library
entries". Each entry shows its type and whether it is still at the firmware's default.

- Entries your firmware defines can be changed and **reset to default** (the arrow button). That
  erases the stored copy, so the default from the code applies right away and also follows a
  changed default in a later firmware. At the default, the button is greyed out.
- Entries no firmware defines (left over from an older firmware, or added here) can be changed
  and **deleted** (the bin).
- **Add** creates an entry in one of your areas or in a new one, as a whole number, decimal
  number, on/off or text. A later firmware reads it if it defines the same name and type in the
  same area. The library's own area takes no new entries.
- Secrets never reach the browser: the WiFi and access point passwords, everything declared with
  `Page::password()`, and keys you mark with `settings.markSecret("key")`. They can only be
  overwritten or reset.
- Library entries that decide whether the device stays reachable (WiFi name and password, device
  name, IP settings) ask for confirmation first, and take effect after a restart.

### Setup access point

The device can open its own WiFi network (named after the device, settings page at
`http://192.168.4.1/settings`):

| Mode | At start-up | While running |
|---|---|---|
| Never | Only while no WiFi is stored | Never |
| Only on WiFi failure (default) | No WiFi stored, or joining fails | After WiFi has been lost for the grace period (2 minutes) |
| Always | Always | Always open |

In every mode the access point closes after the timeout once nobody has been connected to it
for that long (off by default), and it stays open as long as somebody is connected. Opened
because of a WiFi failure, it closes as soon as WiFi is back. After the timeout closed it, it
opens again only once WiFi was back and is lost again.

The `ap*` fields of `pu::Config` are the defaults for every device running the firmware. Each
device can change them on its settings page; "Use firmware defaults" goes back to the values in
the code, which then also follow future firmware updates. `apLocked = true` shows them there
read-only instead.

### Your own home page

Declare the page in `setup()`. The library renders it, validates input, stores settings and
calls your code:

```cpp
pu::Settings config;                       // your project's settings, see "Settings" below
volatile double liters = 0;

void buildHomePage() {
    pu::Page& home = ProjectUtils.web().home("Toilet flush");

    home.section("Settings");              // inputs bound to a setting: loaded, checked, saved
    home.number(config, "flush-water", "Flush volume").unit("L").min(0).step(0.1);
    home.number(config, "pump-delay", "Pump run-on").unit("s").min(0).max(30);
    home.toggle(config, "auto-flush", "Flush automatically");

    home.section("Manual control");
    home.holdButton("Water", [](bool pressed) { digitalWrite(PIN_VALVE, pressed); });
    home.button("Flush", []() { flushRequested = true; });
    home.button("Emergency stop", stopEverything).color(pu::Color::Red).immediate();

    home.section("Status");
    home.value("Flow", []() { return liters; }).unit("L").decimals(1);
    home.indicator("Pump running", []() { return digitalRead(PIN_PUMP) == HIGH; });
}
```

| Element | Purpose |
|---|---|
| `section(title)` | Heading for a group |
| `number`, `slider`, `toggle`, `text`, `select` `(settings, key, label)` | Input bound to a `pu::Settings` key of matching type. Changes are checked (`min`, `max`, whole numbers for `int`) and saved |
| `password(settings, key, label)` | Like `text`, but the stored value is never sent to the browser; the page only shows whether one is set |
| `number`, `slider`, `toggle` `(label, getter, setter)` | Input bound to your own code; nothing is stored |
| `button(label, onClick)` | Runs `onClick` once per click |
| `holdButton(label, onHold)` | `onHold(true)` when pressed, `onHold(false)` when released, and also if the browser stops responding for a second (closed tab, lost WiFi) |
| `value(label, getter)` | Live number or text, refreshed about once a second |
| `indicator(label, getter)` | Status light, lit while `getter` returns true |

Details chain onto any element: `.unit("L")`, `.min()`, `.max()`, `.step()`, `.decimals()`,
`.color(pu::Color::Red)`, `.onChange(callback)`.

Your callbacks and getters run from `ProjectUtils.loop()`, so they may use anything your
`loop()` uses. The consequence: while `loop()` is blocked (a long `delay()`), buttons wait.
Mark the few that must not wait with `.immediate()`; those run right away on the network task
and must therefore be short and safe to run alongside `loop()`.

Add more pages with `ProjectUtils.web().page("/tuning", "Tuning")`.

### Pages written by hand

For anything the elements above do not cover, serve your own HTML. Including the two shared
files gives it the menu and the style:

```cpp
const char kLedHtml[] = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>LED</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="/pu/app.css">
  <script src="/pu/app.js"></script>
</head>
<body>
  <h1>LED</h1>
  <div class="content"><div class="card">
    <button onclick="pu.post('/led/toggle').catch((e) => pu.toast(e.message, true))">Toggle</button>
  </div></div>
</body>
</html>)HTML";

ProjectUtils.web().addPage("/led", "LED", kLedHtml);          // served and listed in the menu
ProjectUtils.server().on("/led/toggle", HTTP_POST, onToggle); // your own route
```

`app.js` provides `pu.get(path)`, `pu.post(path, params)`, `pu.el(tag, props, ...children)`,
`pu.icon(name)`, `pu.toast(text, isError)` and `pu.wrapToggle(checkbox, element)` (the
remembered "Wrap lines" switch of the console). `<body class="wide">` lets a page use the full
window width. `app.css` has the building blocks of the built-in
pages: `.content` (the page column), `.group` with an `.eyebrow` heading, `.panel` with `.row`s,
`.tiles` with `.tile`s, `.card`, and buttons (`primary`, `red`). Use `addMenuLink("Title", "/path")` to list a route you serve
entirely yourself.

## Configuration

Everything is set through `pu::Config` before calling `begin()`.

| Field | Default | Meaning |
|---|---|---|
| `deviceName` | `"ESP32 Device"` | Default name; the user can change it on the settings page |
| `firmwareVersion` | `{0, 0, 0}` | Version of your firmware, compared against GitHub releases |
| `apMode` | `ApMode::OnFailure` | `Never`, `OnFailure` or `Always`, see [Setup access point](#setup-access-point) |
| `apPassword` | empty | Password of the setup access point (8 to 63 characters). Empty = open |
| `apTimeoutMs` | `0` | Close the access point once nobody has used it this long. `0` = never |
| `apGraceMs` | `120000` | `OnFailure`: open the access point after WiFi has been lost this long |
| `apLocked` | `false` | `true`: the `ap*` values cannot be changed on the settings page |
| `wifiConnectTimeoutMs` | `10000` | How long `begin()` waits for the WiFi connection |
| `adminUser` / `adminPassword` | `"admin"` / empty | See [Security](#security) |
| `timezone` | Central Europe | POSIX TZ string, e.g. `"EST5EDT,M3.2.0,M11.1.0"` |
| `ntpServer` | `"pool.ntp.org"` | Time server |
| `githubRepo` | empty | `"user/repository"`; enables updates from GitHub releases |
| `githubFirmwareAsset` | `"firmware.bin"` | Release asset name; `<version>` is replaced by the tag |
| `githubFilesystemAsset` | empty | Optional filesystem image asset of your project |
| `enableConsole`, `enableOta` | `true` | Switch the web console or ArduinoOTA off |
| `serial` | `nullptr` | Stream used by the console; `nullptr` = `Serial` |

## Security

Without `adminPassword`, anyone on the same network can change the WiFi settings, read the
console and upload firmware. That is acceptable on a trusted home network and nowhere else; the
device logs a warning at boot.

With `config.adminPassword = "..."` all pages (yours included), the JSON API, the web console
and ArduinoOTA uploads require it (user name `admin` unless changed). Protect your own routes
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
`getBlob`. `reset("key")` erases the stored value so the default applies again;
`markSecret("key")` hides the value on the memory page. Getters return the value, or the given fallback if the key is unknown or has another
type (which is also logged). `ProjectUtils.settings()` holds the library's own settings.

### Console

`ProjectUtils.console()` is a `Stream` like `Serial`. Output goes to USB serial and to the web
console at `/console`; input from both arrives in the same stream (a line typed in the web
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

**In the browser:** open `/update`, choose the `firmware.bin` that PlatformIO wrote to
`.pio/build/<env>/`, and upload it. The file is checked for size and validity before the device
switches to it. From a script:
`curl -F file=@firmware.bin http://<device>/api/pu/update/upload`.

**From PlatformIO:** `pio run -t upload --upload-port <devicename>.local`
(plus `upload_flags = --auth=<adminPassword>` if a password is set).

**From GitHub releases:** set `config.githubRepo`, then for every release

1. raise `config.firmwareVersion`,
2. publish a public GitHub release tagged `vX.Y.Z` with the firmware binary attached under the
   name given by `githubFirmwareAsset`.

The update page can then check for and install the latest release, or install any specific
version (downgrades included). With automatic updates enabled there, the device checks at
every boot.

### HTTP API

Used by the pages; requires the admin password if one is set.

| Request | Purpose |
|---|---|
| `GET /api/pu/status` | Device, WiFi and update state as JSON |
| `GET /api/pu/info` | Chip, memory, uptime, network details as JSON |
| `GET /api/pu/log` | RAM log as text |
| `GET /api/pu/nav` | Menu entries |
| `POST /api/pu/settings` | Form fields `deviceName`, `ssid`, `pass`, `clearPass`; `staticIP`, `ip`, `subnet`, `gateway`, `dns1`, `dns2`; `apMode`, `apTimeout`, `apGrace` (seconds), `apPassMode`, `apPass`. Restarts the device |
| `POST /api/pu/network/confirm`, `POST /api/pu/network/revert` | Keep or undo a new static IP address while it is being tried out |
| `POST /api/pu/wifi/scan`, `GET /api/pu/wifi/scan` | Start a network search; read its result |
| `POST /api/pu/update/upload?type=firmware\|filesystem` | Upload and install a file (multipart form). Optional `size`, `md5` |
| `POST /api/pu/autoupdates` | `enabled=true\|false` |
| `POST /api/pu/update/check` | Start an update check; poll `status` for the result |
| `POST /api/pu/update/install` | Install `version=vX.Y.Z`, or the latest if omitted |
| `POST /api/pu/restart` | Restart the device |
| `GET /api/pu/page?path=/`, `GET /api/pu/page/values?path=/` | Layout and current values of a page declared in C++ |
| `POST /api/pu/page/widget` | `path`, `widget` (position), `action` (`set`, `click`, `press`, `hold`, `release`), `value` |
| `GET /api/pu/memory` | All areas and entries with type, value, default, stored/defined (no secret values) |
| `POST /api/pu/memory/set` | `area`, `key`, `value` |
| `POST /api/pu/memory/reset` | `area`, `key`: back to the default, or deleted if the firmware does not define it |
| `POST /api/pu/memory/add` | `area`, `key`, `type` (`int`, `float`, `bool`, `text`), `value` |
| `/api/pu/console/ws` | Console websocket (text lines); `GET /api/pu/console/token` returns the token a browser passes as `?token=` |

## Migrating from v1

Devices keep their stored WiFi credentials, device name and auto-update choice when flashed
with a v2-based firmware.

| v1 | v2 |
|---|---|
| `extra_scripts = pre:lib/ProjectUtils-esp32/copy_lib_data.py` | Remove the line. Dependencies come from the library manifest |
| Library pages copied into your `data/` folder, `spiffs.bin` required | Remove `settings.html` and `style.css` from `data/`; the pages are part of the firmware. For your own pages use `/pu/app.css` and `/pu/app.js`, see [Pages written by hand](#pages-written-by-hand) |
| `include/main.h` macros, `include/version.h` | `pu::Config` fields (table below) |
| `#include "main_project_utils.h"` | `#include <ProjectUtils.h>` |
| `project_utils_init("name")` / `project_utils_update()` | `ProjectUtils.begin(config)` / `ProjectUtils.loop()` |
| `server.on(...)` | `ProjectUtils.server().on(...)` |
| Hand-written control page (`data/control.html` plus route handlers) | Declare it in C++, see [Your own home page](#your-own-home-page) |
| Burger menu copied into every HTML file | Drawn by `/pu/app.js` on every page |
| Web console at `/webserial` (WebSerial library) | Own console at `/console`; `/webserial` redirects there. The WebSerial dependency is gone |
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
| `SYS_CONTROL_STAT_IP` | Removed. "Static IP address" on the settings page |
| `SYS_CONTROL_WEBSERIAL` | `enableConsole` (on by default) |
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
pio run -d examples/basic           # build the example for all boards
pio check -e check --fail-on-defect=low
uv tool install clang-tidy==23.1.4  # the version CI uses
python3 scripts/run_clang_tidy.py
```

### On-device tests

`integration/` holds a pytest suite that talks to a real ESP32 running `examples/basic` over
WiFi: all pages and the JSON API, the home page elements, web console, network search, firmware
upload, and (optionally) restarts, stored settings and the admin login. It cannot run on GitHub Actions; run it before a release.

Flash the example once over USB and connect the board to your WiFi. From then on:

```sh
alias devtest='uv run --no-project --with-requirements integration/requirements.txt pytest integration'

devtest --device 192.168.1.210                     # quick, read-only checks
devtest --device 192.168.1.210 --disruptive        # also restart and save settings
devtest --device 192.168.1.210 --flash esp32s3     # build and flash over WiFi first
```

To test the admin login, let the suite build a protected firmware, and remove it afterwards:

```sh
devtest --device 192.168.1.210 --flash esp32s3 --admin-password test1234
devtest --device 192.168.1.210 --flash esp32s3 --ota-password test1234    # back to open
```

The suite can also open every page in a headless browser and click through it (menu, buttons,
hold button, console). That needs Playwright; without it those tests are skipped:

```sh
uv run --with playwright playwright install chromium-headless-shell    # once
uv run --no-project --with-requirements integration/requirements.txt --with playwright \
    pytest integration --device 192.168.1.210
```

`--flash` takes any environment from `examples/basic/platformio.ini`. The address can also be
given as `PU_DEVICE`. Without a device, every test is skipped.

Not covered: the first-time setup through the access point (the test machine would have to
change WiFi networks) and installing an update from a GitHub release.

The web pages live in `web/`. After editing them, `scripts/embed_web_assets.py` regenerates
`src/pu/web_assets.gen.h`; the pre-commit hook does this automatically. GitHub Actions runs
everything except the on-device tests on every push and pull request.

## License

MIT, see [LICENSE](LICENSE).
