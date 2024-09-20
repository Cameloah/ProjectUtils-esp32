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

### Tools
```
DualSerial.begin(115200);
DualSerial.println("Hello World");
```
`DualSerial` is a drop-in replacement for Arduinos `Serial` class, extending the serial communication to a webinterface. Once wifi is connected, a console is available under `192.168.xxx.xxx/webserial`. Any message, either from the serial communication or from the web console is reflected on both channels simultaneously and can be used just like any other serial communication.

`ramlog` is a logbook for runtime events. It is volatile, that means it will not persist after reboot. It is incredibly useful to log and debug startup procedures or other events. The ramlog can hold 30 entries before wrapping around. Entries are referenced using a time stamp. Normally that is in runtime format. Once wifi is connected, timestamps are converted to local time.

To create an entry, one can pass predefined error handles, even with additional strings:

```
ram_log_notify(RAM_LOG_ERROR_MEMORY, example_error);
ram_log_notify(RAM_LOG_ERROR_MEMORY, example_error, "some_string");
```

or use custom messages. If `flag_print` is set, the message will also be printed to the serial comms immediately:
```
ram_log_notify(RAM_LOG_INFO, "hello world!, flag_print=true");
```

The ramlog can be printed using `ram_log_print_log()`

### WiFi

```
#include "main_project_utils.h"
#include "ramlog.h"

void serup() {
  DualSerial.println("Starting Wifi...");
  project_utils_init("My new ESP32");
}

void loop() {
  project_utils_update();
}
```

The `project_utils_init()` initializes all systems and tools according to settings in `main.h`. It takes the general device name that is used as a name for the initial access point. `project_utils_update();` needs to be called periodically.

After initial bootup, the ESP32 creates an AP with the general device name plus a random number. This way multiple devices can be setup up at once without interfering.

1. Connect the the AP. A captive portal should appear.
2. Setup a unique device name and the wifi credentials of your network. Ignore the other settings and submit.
3. connect to the device settings using your network. The IP address is printed over serial. Modern routers support DNS, which allows you to reach your device using the unique device name instead of the ip address.
```
https://example-esp/settings
```
4. visit advanced settings such as auto updates.
5. connect the device to your pc using serial and go to `https://example-esp/webserial`. anything written in either one should reflect in both channels.


WIP



