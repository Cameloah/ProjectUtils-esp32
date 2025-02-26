//
// Created by Cameloah on 21/12/2022.
//

#include "ram_log.h"
#include "webserial_monitor.h"
#include "time_module.h"

ram_log_item_t _ringbuffer[RAM_LOG_RINGBUFFER_LEN];

uint8_t _ringbuffer_item_index = 0;
uint8_t _ringbuffer_item_count = 0;

String hex_string(uint32_t num) {
    String hex = String(num, HEX);
    if (hex.length() == 1)
        hex = "0" + hex;
    return "0x" + hex;
}

void ram_log_notify(RAM_LOG_ITEM_t item_type, uint32_t module_error, const char *user_payload, bool flag_print) {
    // save timestamp
    _ringbuffer[_ringbuffer_item_index].timestamp = esp_timer_get_time();
    // save item type
    _ringbuffer[_ringbuffer_item_index].item_type = item_type;
    // save payload as string
    String module_error_str = hex_string(module_error);

    switch (item_type) {
        //TODO: write proper parser
        case RAM_LOG_ERROR_MEMORY:
            _ringbuffer[_ringbuffer_item_index].payload = module_error_str + "/" + esp_err_to_name(module_error);
            if (user_payload != nullptr)
                _ringbuffer[_ringbuffer_item_index].payload += ", " + String(user_payload);
            break;

        default:
            _ringbuffer[_ringbuffer_item_index].payload = module_error_str;

    }

    if (flag_print)
        DualSerial.println(hex_string(item_type) + "/" + log_item_to_name(item_type) + " - " + _ringbuffer[_ringbuffer_item_index].payload);

    // increase item index
    _ringbuffer_item_index = (_ringbuffer_item_index + 1) % RAM_LOG_RINGBUFFER_LEN;
    _ringbuffer_item_count = (_ringbuffer_item_count > RAM_LOG_RINGBUFFER_LEN) ? RAM_LOG_RINGBUFFER_LEN : _ringbuffer_item_count+1;
}

void ram_log_notify(RAM_LOG_ITEM_t item_type, const char *user_payload, bool flag_print) {
    // save timestamp
    _ringbuffer[_ringbuffer_item_index].timestamp = esp_timer_get_time();
    // save item type
    _ringbuffer[_ringbuffer_item_index].item_type = item_type;
    // save payload as string
    _ringbuffer[_ringbuffer_item_index].payload = user_payload;
    // if wanted, also print string
    if (flag_print) DualSerial.println(user_payload);

    // increase item index
    _ringbuffer_item_index = (_ringbuffer_item_index + 1) % RAM_LOG_RINGBUFFER_LEN;
    _ringbuffer_item_count = (_ringbuffer_item_count > RAM_LOG_RINGBUFFER_LEN) ? RAM_LOG_RINGBUFFER_LEN : _ringbuffer_item_count+1;
}

void ram_log_print_log() {   
    // Pre-allocate a large enough String object to avoid frequent reallocations.
    // NOTE: Adjust the initial size based on your needs and available memory.
    String payload;
    payload.reserve(5000); // Example size, adjust based on expected log size

    payload += "Log items: ";
    payload += String(_ringbuffer_item_count);
    payload += '\n';

    for (int item = 0; item < _ringbuffer_item_count; ++item) {
        
        if(!time_module_is_init()) {
            unsigned long seconds = (_ringbuffer[item].timestamp / 1000000);

            uint16_t ms = (_ringbuffer[item].timestamp / 1000) % 1000;
            uint8_t sec = seconds % 60; seconds /= 60;
            uint8_t min = seconds % 60; seconds /= 60;
            uint8_t hrs = seconds % 24; seconds /= 24;
            uint16_t days = seconds;

            // Build the timestamp string
            payload += days;
            payload += "d:";
            payload += hrs;
            payload += "h:";
            payload += min;
            payload += "m:";
            payload += sec;
            payload += "s:";
            payload += ms;
            payload += "ms ";
        }

        else 
            payload += time_convert_micros(_ringbuffer[item].timestamp) + " ";

        // Append log level and message
        if (_ringbuffer[item].item_type == RAM_LOG_INFO) {
            payload += "INFO: ";
        }

        else if (_ringbuffer[item].item_type == RAM_LOG_WARNING) {
            payload += "WARNING: ";
        } 
        
        else {
            payload += "ERROR: ";
            payload += hex_string(_ringbuffer[item].item_type) + "/" + log_item_to_name(_ringbuffer[item].item_type);
            payload += " ";
        }

        // Append the log message and a newline
        payload += _ringbuffer[item].payload;
        payload += '\n';
    }

    // Print the entire log at once
    DualSerial.println(payload);
}

String ram_log_time_str(int64_t sys_us) {
    unsigned long int seconds = (sys_us / 1000000);
    uint16_t ms = (sys_us /1000) % 1000;
    uint8_t sec = seconds % 60;		seconds /= 60;
    uint8_t min = seconds % 60;		seconds /= 60;
    uint8_t hrs = seconds % 24;     seconds /= 24;
    uint16_t d = seconds;
    String time_str = String(d) + "d:" + String(hrs) + "h:" + String(min) + "m:" + String(sec) + "s:" + String(ms) + "ms";
    return time_str;
}

String log_item_to_name(RAM_LOG_ITEM_t item) {
    switch (item) {
        case RAM_LOG_INFO:
            return "INFO";
        case RAM_LOG_WARNING:
            return "WARNING";
        case RAM_LOG_ERROR_NETWORK_MANAGER:
            return "NETWORK_MANAGER";
        case RAM_LOG_ERROR_GITHUB_UPDATE:
            return "GITHUB_UPDATE";
        case RAM_LOG_ERROR_MEMORY:
            return "MEMORY";
        case RAM_LOG_ERROR_GPS_MANAGER:
            return "GPS_MANAGER";
        case RAM_LOG_ERROR_SYSTEM:
            return "SYSTEM";
        case RAM_LOG_ERROR_TIME:
            return "TIME";
        default:
            return "UNKNOWN";
    }
}