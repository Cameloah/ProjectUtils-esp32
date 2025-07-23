# Project Utilities for the ESP32 Microcontroller

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/built%20with-PlatformIO-orange)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/target-ESP32-blue)](https://www.espressif.com/en/products/socs/esp32)

A custom utility library to supercharge your ESP32 development with robust network management, OTA updates, memory handling, web-based serial, and more.

## 📑 Table of Contents

- [Introduction](#introduction)
- [Features](#-features)
- [Integration Guide](#️-integration-guide)
  - [1️⃣ Add the library](#1️⃣-add-the-library)
  - [2️⃣ Modify `platformio.ini`](#2️⃣-modify-platformioini)
  - [3️⃣ Define control macros](#3️⃣-define-control-macros)
  - [4️⃣ Add versioning (`version.h`)](#4️⃣-add-versioning-versionh)
- [🚦 Quick Start](#-quick-start)
  - [Initialize WiFi + Tools](#initialize-wifi--tools)
  - [Connect & Configure](#-connect--configure)
  - [Using the AsyncWebServer](#using-the-asyncwebserver)
- [OTA GitHub Updates](#-ota-github-updates)
- [Memory Module (NVS)](#-memory-module-nvs)
- [Tools & Utilities](#-tools--utilities)
  - [DualSerial](#dualserial)
  - [RAM Log](#ram-log)
  - [Time Module](#time-module)
- [License](#-license)

## Introduction

The [ESP32](https://www.espressif.com/en/products/socs/esp32) is a powerful microcontroller with configurable GPIOs, WiFi, BLE, SPI, I2C, and more — ideal for both small and large-scale projects that require real-time processing.

This utility package is a **pre-configured project starter** built to accelerate ESP32 development by including essential modules commonly used across most of my projects.

---

## ✨ Features

-  **Network management** with captive portal and web-based WiFi configuration
-  **Async TCP server** to serve internal/external webpages
-  **WebSerial** interface that mirrors serial communication between web and USB
-  **Memory module** to safely store and retrieve parameters in the NVS
-  **RAM log module** to track runtime events
-  **Time sync module** with local time support for logs and events
-  **OTA support** via PlatformIO + GitHub releases
-  **Update routines** for firmware & SPIFFS from GitHub

_Modular architecture: enable/disable features as needed!_

---

## ⚙️ Integration Guide

> 💡 **Recommended:** Use [PlatformIO](https://platformio.org/platformio-ide) with VSCode or CLion for seamless integration.

### 1️⃣ Add the library

Clone the repo as a submodule inside your project:

```
/lib/ProjectUtils-esp32
```

See [PlatformIO docs](https://docs.platformio.org/en/latest/projectconf/sections/platformio/options/directory/lib_dir.html) for setup help.

---

### 2️⃣ Modify `platformio.ini`

Add this line to enable automatic config & web data setup:

```ini
extra_scripts = pre:lib/ProjectUtils-esp32/copy_lib_data.py
```

---

### 3️⃣ Define control macros

In your `/include/main.h` file (create it if missing):

```cpp
// ---------- DEBUG AND SYSTEM CONTROL ---------- //
#define SYS_CONTROL_STAT_IP                   // Enable static IP
#define SYS_CONTROL_WEBSERIAL                 // Enable WebSerial (use 'DualSerial')

#define AP_PASSWORD             nullptr       // Use nullptr to disable password
#define AP_VERBOSITY            1             // 1: spawn if WiFi fails, 2: always spawn AP
//#define AP_TIMEOUT            300000        // Optional: turn off AP after 5 min
#define TIMEOUT_WIFI_CONNECT_MS 5000          // WiFi connect timeout

// Time settings
#define TIMEZONE_SEC_OFFSET     3600          // UTC+1
#define DAYLIGHT_SAVING_SEC_OFFSET 3600       // DST offset (UTC+2 total)
```

---

### 4️⃣ Add versioning (`version.h`)

In `/include/version.h`:

```cpp
#define FW_VERSION_MAJOR        0
#define FW_VERSION_MINOR        0
#define FW_VERSION_PATCH        1

#define GITHUB_REPO             "your-user/your-repository"
#define GITHUB_FW_BIN           "project_<version>.bin"
#define GITHUB_FS_BIN           "spiffs.bin"
```

Replace `your-user/your-repository` with your GitHub details.

---

## 🚦 Quick Start

### Initialize WiFi + Tools

```cpp
#include "main_project_utils.h"

void setup() {
  DualSerial.println("Starting Wifi...");
  project_utils_init("My new ESP32");
}

void loop() {
  project_utils_update();
}
```

- `project_utils_init()` sets up all modules
- `project_utils_update()` must be called periodically

🔧 The ESP32 creates an AP with your device name + random suffix to avoid conflicts.

###  Connect & Configure

1. Connect to the ESP32 AP (e.g., `MyESP32-ABC123`)
2. Captive portal opens → configure device name + WiFi
3. Connect to the network → IP is printed over serial
4. Open browser:

```
http://example-esp/settings
http://example-esp/webserial
```

(DNS resolving may work via device name on modern routers)

---

### Using the AsyncWebServer

```cpp
void handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/html", "Hello World");
}

void setup() {
  server.on("/", HTTP_GET, handleRoot);
}
```

---

## 📦 OTA GitHub Updates

Once configured:

1. Update version numbers in `version.h`
2. Create a public GitHub release with:
   - `project_<version>.bin`
   - `spiffs.bin` (includes all web files)

On boot, the device checks for the **latest GitHub release**. If newer, it downloads and installs both binaries.

- Manual checks and downgrades are also supported

---

## 💾 Memory Module (NVS)

```cpp
#include "memory_module.h"

MemoryModule config_data;

config_data.addParameter("timeout_boot", 20);
config_data.addParameter("factor", 1.23f);
config_data.addParameter("milage", 12345.3);
config_data.addParameter("name", "test device");
config_data.addParameter("flag_autostart", true);

config_data.set("flag_autostart", false);

int timeout = *config_data.getInt("timeout_boot");
Serial.println(config_data.getString("name"));

config_data.saveAll();
config_data.loadAll();
```

Use `.save("key")` and `.loadAllStrict()` as needed. Typed getters available:
```cpp
getInt(), getFloat(), getDouble(), getString()
```

---

## 🛠 Tools & Utilities

### DualSerial

```cpp
DualSerial.begin(115200);
DualSerial.println("Hello World");
```

Mirrors messages between USB and `/webserial`. Works as a direct `Serial` replacement.

---

### RAM Log

Logs runtime events (30 entries max, circular buffer):

```cpp
ram_log_notify(RAM_LOG_ERROR_MEMORY, example_error);
ram_log_notify(RAM_LOG_INFO, "Startup complete!", true);
```

Print logs:
```cpp
ram_log_print_log();
```

Timestamps switch to local time after sync.

---

### Time Module

After successful sync with NTP server, the module:

- Persists local time for the session
- Supports DST offset
- Powers timestamping in logs

---

## 📚 License

MIT License — use freely, modify, and contribute.
