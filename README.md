# How to use this library 

Include the following in your `main.h` file

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

And include the following in your `version.h` file

```
// define your current FW version here
#define FW_VERSION_MAJOR                    0
#define FW_VERSION_MINOR                    0
#define FW_VERSION_PATCH                    1

// set the github user and source repo here "user/repo"
#define GITHUB_REPO     "your-user/your-repository"
// enter the name of the firmware bin file here. A <version> string will be replaced by the FW version
#define GITHUB_FW_BIN   "project_<version>.bin"
// enter the name of the compiled filesystem binary here
#define GITHUB_FS_BIN   "spiffs.bin"
```


Add the following line to your main platformio.ini file

`extra_scripts = pre:lib/ProjecUtils-esp32/copy_lib_data.py`
