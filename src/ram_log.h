//
// Created by Cameloah on 21/12/2022.
//

#pragma once

#include "Arduino.h"

#define RAM_LOG_RINGBUFFER_LEN                  30

typedef enum{
    RAM_LOG_INFO                        = 0x00,
    RAM_LOG_WARNING                     = 0x01,
    RAM_LOG_ERROR_NETWORK_MANAGER       = 0x02,
    RAM_LOG_ERROR_GITHUB_UPDATE         = 0x03,
    RAM_LOG_ERROR_MEMORY                = 0x04,
    RAM_LOG_ERROR_GPS_MANAGER           = 0x05,
    RAM_LOG_ERROR_SYSTEM                = 0x06,
} RAM_LOG_ITEM_t;

typedef struct {
    unsigned long int timestamp;
    RAM_LOG_ITEM_t item_type;
    String payload;
} ram_log_item_t;

/// \brief adds an entry to the ramlog
///
/// \param item_type what type of message
/// \param module_error error code
void ram_log_notify(RAM_LOG_ITEM_t item_type, uint32_t module_error);

/// \brief adds an entry to the ramlog, an additional string can be added
///
/// \param item_type what type of message
/// \param module_error error code
/// \param user_payload additional string
void ram_log_notify(RAM_LOG_ITEM_t item_type, uint32_t module_error, const char *user_payload);

/// \brief adds an entry to the ramlog that can contain a sting
///
/// \param item_type what type of message
/// \param user_payload message string
/// \param flag_print when set to true, message string will also be printed in console
void ram_log_notify(RAM_LOG_ITEM_t item_type, const char* user_payload, bool flag_print = false);

/// \brief prints the entire ramlog and all entries nicely formatted
void ram_log_print_log();

/// \brief returns the current uptime in a nice string format
///
/// \param sys_ms current system time such as millis()
/// \return string of time format
String ram_log_time_str(unsigned long int sys_ms);