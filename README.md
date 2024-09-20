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

```cpp
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

```cpp
// define your current FW version here
#define FW_VERSION_MAJOR                    0
#define FW_VERSION_MINOR                    0
#define FW_VERSION_PATCH                    1

// set the github user and source repo here
#define GITHUB_REPO     "your-user/your-repository"
// enter the name of the firmware bin file here. A <version> string will be replaced by the FW version in the format of "vX.X.X"
#define GITHUB_FW_BIN   "project_<version>.bin"
// enter the name of the compiled filesystem binary here
#define GITHUB_FS_BIN   "spiffs.bin"
```

you will need to replace the `your-user` and `your-repository` with your github user name and the root project repository name respectively.

Congratulations, now you are all set!


## Quick start guide

### WiFi
```cpp
#include "main_project_utils.h"
#include "ramlog.h"

void setup() {
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

The AsyncWebserver is available outside of the library using the `server` object.
Example:
```cpp
void handleRoot(AsyncWebServerRequest *request) {
    request->send(200, "text/html", String("Hello World"));
}

void setup()
  server.on("/", HTTP_GET, handleRoot);
```

### Github update
Once `main.h` is set up, the module is ready to use. Once a new FW version is ready to be released, adjust the `version.h` file accordingly and publish a release on your github page. Make sure your repository is public. A release should contain at least the following files:

- `project_<version>.bin` - FW file
- `spiffs.bin` - compiled SPIFFS file, this must also contain all webfiles from the library. usually these were automatically pasted in your `root/data` folder

If auto update is enabled in `/settings`, once the device boots up, it will check for your *latest* release on github. It will be compared to its current version and download and install all .bin files if a newer release is detected.

Even if auto update is not enabled, you can still check manually for an update. Additionally, it allows you to downgrade, aka install a desired version in the format of `vX.X.X`

### Memory Module

The memory module can safely store parameters for runtime as well as in the NVS. Here is an example.

1. Include the necessary header file in your code:

```cpp
#include "memory_module.h"
```

2. Create memory object:
```cpp
MemoryModule config_data;
```

3. Add parameters:
```cpp
config_data.addParameter("timeout_boot", 20);
config_data.addParameter("factor", 1.23f);
config_data.addParameter("milage", (double) 12345.3);
config_data.addParameter("name", (String) "dies ist ein test");
config_data.addParameter("flag_autostart", true);
```

4. Change your parameters:
```cpp
config_data.set("flag_autostart", false);
```

5. Use your parameters. Use `.get()` function to return the data pointer or use dedicated functions such as `.getInt()`, `.getFloat()` etc.:
```cpp
int* timeout_ptr = static_cast<int*>(config_data.get("timeout_boot"));
int timeout = *config_data.getInt("timeout_boot");
DualSerial.println(*config_data.getFloat("factor"));
DualSerial.println(*config_data.getDouble("milage"));
DualSerial.println(config_data.getString("name"));
DualSerial.println(*static_cast<bool*>(config_data.get("flag_autostart")));
```

6. Save your parameters to NVS:
```cpp
config_data.saveAll();        // to save the entire data set
config_data.save("milage");   // save individual parameters
```

7. load your parameters from NVS. this does not work without step 1:
```cpp
config_data.loadAll();        // to load entire data set
config_data.load("milage");   // load individual parameters
config_data.loadAllStrict();  // load entire data set, cancel and return error, if key is not found
```


### Tools
```cpp
DualSerial.begin(115200);
DualSerial.println("Hello World");
```
`DualSerial` is a drop-in replacement for Arduinos `Serial` class, extending the serial communication to a webinterface. Once wifi is connected, a console is available under `/webserial`. Any message, either from the serial communication or from the web console is reflected on both channels simultaneously and can be used just like any other serial communication.

`ramlog` is a logbook for runtime events. It is volatile, that means it will not persist after reboot. It is incredibly useful to log and debug startup procedures or other events. The ramlog can hold 30 entries before wrapping around. Entries are referenced using a time stamp. Normally that is in runtime format. Once wifi is connected, timestamps are converted to local time.

To create an entry, one can pass predefined error handles, even with additional strings:

```cpp
ram_log_notify(RAM_LOG_ERROR_MEMORY, example_error);
ram_log_notify(RAM_LOG_ERROR_MEMORY, example_error, "some_string");
```

or use custom messages. If `flag_print` is set, the message will also be printed to the serial comms immediately:
```cpp
ram_log_notify(RAM_LOG_INFO, "hello world!, flag_print=true");
```

The ramlog can be printed using `ram_log_print_log()`


`time_module` provides access to accurate local time, once initialized. The module will not initialize if there is no connection to the time server. Once that connection is established, the local time format persists until next reboot, even after loss of internet.


