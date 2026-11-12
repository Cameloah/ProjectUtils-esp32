#include "RamLog.h"

#include <esp_timer.h>

#include "Clock.h"

namespace pu {

const char* RamLog::levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARNING";
    case LogLevel::Error:
        return "ERROR";
    }
    return "?";
}

void RamLog::info(const char* format, ...) {
    va_list args; // NOLINT(cppcoreguidelines-init-variables): set by va_start
    va_start(args, format);
    log(LogLevel::Info, format, args);
    va_end(args);
}

void RamLog::warn(const char* format, ...) {
    va_list args; // NOLINT(cppcoreguidelines-init-variables): set by va_start
    va_start(args, format);
    log(LogLevel::Warning, format, args);
    va_end(args);
}

void RamLog::error(const char* format, ...) {
    va_list args; // NOLINT(cppcoreguidelines-init-variables): set by va_start
    va_start(args, format);
    log(LogLevel::Error, format, args);
    va_end(args);
}

void RamLog::log(LogLevel level, const char* format, va_list args) {
    char text[kLogTextSize];
    vsnprintf(text, sizeof(text), format, args);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ring_.push(level, esp_timer_get_time(), text);
    }
    if (echo_ != nullptr) {
        echo_->printf("[%s] %s\n", levelName(level), text);
    }
}

size_t RamLog::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ring_.size();
}

void RamLog::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    ring_.clear();
}

bool RamLog::copyEntry(size_t index, LogEntry& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= ring_.size()) {
        return false;
    }
    out = ring_.at(index);
    return true;
}

void RamLog::printTo(Print& out) const {
    // Copy one entry at a time so the lock is never held while writing to a slow output.
    LogEntry entry = {};
    for (size_t index = 0; copyEntry(index, entry); ++index) {
        char timestamp[32];
        if (clock_ != nullptr) {
            clock_->formatUptime(entry.uptimeUs, timestamp, sizeof(timestamp));
        } else {
            snprintf(timestamp, sizeof(timestamp), "%lld ms",
                     static_cast<long long>(entry.uptimeUs / 1000));
        }
        out.printf("%s %s: %s\n", timestamp, levelName(entry.level), entry.text);
    }
}

} // namespace pu
