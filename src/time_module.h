#pragma once

#include "Arduino.h"
#include <WiFi.h>
#include <time.h>
#include "NTP.h"

#include "webserial_monitor.h"
#include "network_manager.h"
#include "../../../include/main.h"



typedef enum{
    TIME_MODULE_ERROR_NO_ERROR        = 0x00,
    TIME_MODULE_ERROR_PULL            = 0x01,
    TIME_MODULE_ERROR_NOT_INITIALIZED = 0x02,
    TIME_MODULE_ERROR_UNKNOWN         = 0xFF
} TIME_MODULEE_ERROR_t;



TIME_MODULEE_ERROR_t time_module_init();

String time_formatted_pulled();

String time_convert_micros(unsigned long eventMillis);

bool time_module_is_init();
