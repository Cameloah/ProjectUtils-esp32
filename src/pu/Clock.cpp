#include "Clock.h"

#include <esp_timer.h>
#include <sys/time.h>
#include <time.h>

namespace pu {

namespace {
// Any time after 2020-09-13 means the clock has been set; an unsynced ESP32 starts at 1970.
const time_t kPlausibleEpoch = 1600000000;
} // namespace

void Clock::begin(const char* timezone, const char* ntpServer) {
    configTzTime(timezone, ntpServer);
}

bool Clock::isSynced() const {
    return time(nullptr) > kPlausibleEpoch;
}

size_t Clock::formatUptime(int64_t uptimeUs, char* buffer, size_t size) const {
    if (size == 0) {
        return 0;
    }
    int written = 0;
    if (isSynced()) {
        struct timeval nowTv = {};
        gettimeofday(&nowTv, nullptr);
        const int64_t nowWallUs = static_cast<int64_t>(nowTv.tv_sec) * 1000000 + nowTv.tv_usec;
        const int64_t eventWallUs = nowWallUs - (esp_timer_get_time() - uptimeUs);
        const time_t eventSec = static_cast<time_t>(eventWallUs / 1000000);
        const int eventMs = static_cast<int>((eventWallUs / 1000) % 1000);
        struct tm local = {};
        localtime_r(&eventSec, &local);
        char date[24];
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &local);
        written = snprintf(buffer, size, "%s.%03d", date, eventMs);
    } else {
        const int64_t totalMs = uptimeUs / 1000;
        const int64_t totalSec = totalMs / 1000;
        written = snprintf(
            buffer, size, "%lldd %02d:%02d:%02d.%03d", static_cast<long long>(totalSec / 86400),
            static_cast<int>((totalSec / 3600) % 24), static_cast<int>((totalSec / 60) % 60),
            static_cast<int>(totalSec % 60), static_cast<int>(totalMs % 1000));
    }
    if (written < 0) {
        buffer[0] = '\0';
        return 0;
    }
    return static_cast<size_t>(written) < size ? static_cast<size_t>(written) : size - 1;
}

String Clock::now() const {
    char buffer[32];
    formatUptime(esp_timer_get_time(), buffer, sizeof(buffer));
    return String(buffer);
}

} // namespace pu
