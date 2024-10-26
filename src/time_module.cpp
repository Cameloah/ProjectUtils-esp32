#include "time_module.h"
#include "ram_log.h"


// ------------- variables ------------- //

WiFiUDP wifiUdp;
NTP ntp(wifiUdp);

const long _gmtOffset_sec = TIMEZONE_SEC_OFFSET;
const long _daylightOffset_sec = DAYLIGHT_SAVING_SEC_OFFSET;

time_t local_epoch_buffer;
int64_t local_epoch_pull_timestamp;

bool _flag_is_initialized = false;



// ---------- private members ---------- //

time_t _local_epoch() {
	time_t utcCurrent = ntp.epoch();
    time_t local;
	if (ntp.isDST())
		local = utcCurrent + _daylightOffset_sec + _gmtOffset_sec;

	else
		local = utcCurrent + _gmtOffset_sec;

    return local;
	}

TIME_MODULEE_ERROR_t _time_module_pull() {
    if (!_flag_is_initialized) 
        return TIME_MODULE_ERROR_NOT_INITIALIZED;

    local_epoch_pull_timestamp = esp_timer_get_time();

    if (!ntp.update()) {
        ram_log_notify(RAM_LOG_ERROR_TIME, TIME_MODULE_ERROR_PULL);
        _flag_is_initialized = false;
        return TIME_MODULE_ERROR_PULL;
    }

    local_epoch_buffer = _local_epoch();
    return TIME_MODULE_ERROR_NO_ERROR;
}



// ----------- public members ------------ //

TIME_MODULEE_ERROR_t time_module_init() {

    if(!network_manager_is_connected()) return TIME_MODULE_ERROR_PULL;

    ntp.ruleDST("CEST", Last, Sun, Mar, 2, 120); // last sunday in march 2:00, timetone +120min (+1 GMT + 1h summertime offset)
    ntp.ruleSTD("CET", Last, Sun, Oct, 3, 60); // last sunday in october 3:00, timezone +60min (+1 GMT)
    ntp.begin();
    ntp.updateInterval(1);

    _flag_is_initialized = true;

    if (_time_module_pull() != TIME_MODULE_ERROR_NO_ERROR)
        return TIME_MODULE_ERROR_PULL;

    return TIME_MODULE_ERROR_NO_ERROR;
}

String time_formatted_pulled() {
    if (_time_module_pull() != TIME_MODULE_ERROR_NO_ERROR)
        return "Error fetching local time. ";

    String timeString = ntp.formattedTime("%F %T");

    // Get milliseconds
    int64_t milliseconds = (esp_timer_get_time() / 1000) % 1000;

    // Format the time string with milliseconds
    char formattedTime[35];
    snprintf(formattedTime, sizeof(formattedTime), "%s.%03lld", timeString.c_str(), milliseconds);
    
    return String(formattedTime);
}

String time_convert_micros(unsigned long eventMicros) {
    if (!_flag_is_initialized)
        return "Error fetching local time. ";

    time_t bootTime = local_epoch_buffer - (local_epoch_pull_timestamp / 1000000);

    // Convert elapsed microseconds to seconds and subtract from current time
    time_t eventTime = bootTime + (eventMicros / 1000000);
    struct tm *timeinfo = localtime(&eventTime);
    char timeString[30];
    strftime(timeString, sizeof(timeString), "%F %T", timeinfo);

    // Include microseconds
    int64_t eventMilliseconds = (eventMicros / 1000) % 1000;

    // Format the event time string with milliseconds
    char formattedTime[35];
    snprintf(formattedTime, sizeof(formattedTime), "%s.%03lld", timeString, eventMilliseconds);
    
    return String(formattedTime);
}

bool time_module_is_init() {
    return _flag_is_initialized;
}