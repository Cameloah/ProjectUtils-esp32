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

/**
 * @brief Initializes the time module.
 * @return The error code indicating the result of the initialization.
 */
TIME_MODULEE_ERROR_t time_module_init();

/**
 * @brief Retrieves the current time in a formatted string.
 * @return The formatted string representing the current time.
 */
String time_formatted_pulled();

/**
 * @brief Converts the given event milliseconds to a formatted string.
 * @param eventMicros The event milliseconds to convert.
 * @return The formatted string representing the converted time.
 */
String time_convert_micros(unsigned long eventMicros);

/**
 * @brief Checks if the time module is initialized.
 * @return True if the time module is initialized, false otherwise.
 */
bool time_module_is_init();
