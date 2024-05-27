# How to use this library 

Include the following in your ´main.h´ file

```
// ---------- DEBUG AND SYSTEM CONTROL ---------- //

#define SYS_CONTROL_STAT_IP
#define SYS_CONTROL_WEBSERIAL

// set this to any password to protect the AP
#define AP_PASSWORD                     nullptr

// AP verbosity:
// 1 - AP will spawn if esp cannot connect to existing wifi for any reason
// 2 - AP will always spawn, even if connected to existing wifi
#define AP_VERBOSITY                    1

// comment this out to have an always on AP once its spawned
// #define AP_TIMEOUT                      300000      // 5 min
#define TIMEOUT_WIFI_CONNECT_MS         5000
```



Add the following line to your main platformio.ini file

`extra_scripts = pre:lib/common_lib_wifi_debugger/copy_lib_data.py`
