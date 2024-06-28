# Project Utilities for the ESP32 microcontroller

## Introduction

The ESP32 is a fantastic small microcontroller unit (MCU) that comes with fully configurable GPIOs, WiFi, Bluetooth LE, SPI, I2C, SPIFFS, etc. just to name a few features. It is certainly my favorite when it comes to small and bigger projects that require real-time processing. 

A few features I use universally in almost all my projects can be found in this custom library. A pre-configured package in order to give ESP32 projects a kick-start.

## Features
- Network managment incl. captive portal AP and web page to configure WiFi credentials
- Async TCP server setup to host internal and external web pages
- Web based serial interface, that prints all serial communication to the web and usb connection both ways
- Memory module to safely store and retreive parameters from the NVS
- RAM log module to log runtime events and retreive list via serial comms
- Time module to sync MCU time to local time in order to reference runtime events
- OTA flash integration for PlatformIO
- Update routines to automatically or manually update Firmware and SPIFFS files from user github releases

Some of the more intensive modules can be enabled/disabled as desired by the user. 


# How to integrate this library 

It is highly recommended to use the PlatformIO extension for VSCode Studio or Clion in order to take full advantage of this library.

### 1. Get the files!
include library as a git submodule to your existing PlatformIO project under `/lib/ProjectUtils-esp32`. Further information on how to do this can be found [here]([https://docs.platformio.org/en/latest/projectconf/sections/platformio/options/directory/lib_dir.html]).

### 2. Setup platformIO configuration
The library requires its own dependencies and web page data. It comes with a script that, when run during any Build action, transfers the required dependencies and web files to your main `platformio.ini` file and `/data` folder automatically.

For this, add the following line to your main platformio.ini file

`extra_scripts = pre:lib/ProjectUtils-esp32/copy_lib_data.py`


### 3. Add control makros to your root project
Include the following in your `main.h` file. If you don't have one, you will need to make one in the `/include/` directory

```
// ---------- DEBUG AND SYSTEM CONTROL ---------- //

#define SYS_CONTROL_STAT_IP                     // enable static ip
#define SYS_CONTROL_WEBSERIAL                   // enable the webserial

// set this to any password to protect the AP
#define AP_PASSWORD                     nullptr // a nullptr removes the password

// AP verbosity:
// 1 - AP will spawn if esp cannot connect to existing wifi for any reason
// 2 - AP will always spawn, even if connected to existing wifi
#define AP_VERBOSITY                    1

// comment this out to have an always on AP once its spawned
// #define AP_TIMEOUT                      300000      // 5 min
#define TIMEOUT_WIFI_CONNECT_MS         5000

// time module settings
#define TIMEZONE_SEC_OFFSET             3600  // CET is UTC+1
#define DAYLIGHT_SAVING_SEC_OFFSET      3600  // Additional offset for DST (CEST is UTC+2)
```

### Give your project versioning
If you don't already have it, make a `version.h` file in the `/include/` directory and add the following code:

```
// define your current FW version here
#define FW_VERSION_MAJOR                    0
#define FW_VERSION_MINOR                    0
#define FW_VERSION_PATCH                    1

// set the github user and source repo here
#define GITHUB_REPO     "your-user/your-repository"
// enter the name of the firmware bin file here. A <version> string will be replaced by the FW version
#define GITHUB_FW_BIN   "project_<version>.bin"
// enter the name of the compiled filesystem binary here
#define GITHUB_FS_BIN   "spiffs.bin"
```

you will need to replace the `your-user` and `your-repository` with your github user name and the root project repository name respectively.

Congratulations, now you are all set!


## Quick start guide

### WiFi

WIP



