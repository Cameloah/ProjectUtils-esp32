#pragma once

#include <Arduino.h>

namespace pu {

/// Wall-clock time via the SNTP client built into the ESP32 core.
class Clock {
  public:
    /// Starts time synchronisation. Call once the network stack is up.
    /// timezone is a POSIX TZ string, e.g. "CET-1CEST,M3.5.0,M10.5.0/3".
    void begin(const char* timezone, const char* ntpServer);

    /// True once the system time has been set from the network.
    bool isSynced() const;

    /// Formats the moment `uptimeUs` (microseconds since boot, as from esp_timer_get_time()).
    /// Local time "YYYY-MM-DD HH:MM:SS.mmm" when synced, otherwise uptime "0d 00:00:00.000".
    size_t formatUptime(int64_t uptimeUs, char* buffer, size_t size) const;

    /// Current time in the same format as formatUptime().
    String now() const;
};

} // namespace pu
