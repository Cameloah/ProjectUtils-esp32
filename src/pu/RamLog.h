#pragma once

#include <Arduino.h>
#include <cstdarg>
#include <mutex>

#include "RingLog.h"

namespace pu {

class Clock;

/// Keeps the most recent runtime events in RAM. Safe to call from any task (not from ISRs).
/// Every entry is also echoed to the console if one is attached.
class RamLog {
  public:
    static const size_t kCapacity = 30;

    void setEcho(Print* echo) { echo_ = echo; }
    void setClock(const Clock* clock) { clock_ = clock; }

    void info(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void warn(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void error(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void log(LogLevel level, const char* format, va_list args);

    size_t size() const;
    void clear();

    /// Prints all stored entries, oldest first, one per line with timestamp and level.
    void printTo(Print& out) const;

    static const char* levelName(LogLevel level);

  private:
    bool copyEntry(size_t index, LogEntry& out) const;

    mutable std::mutex mutex_;
    RingLog<kCapacity> ring_;
    Print* echo_ = nullptr;
    const Clock* clock_ = nullptr;
};

} // namespace pu
