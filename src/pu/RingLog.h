#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pu {

enum class LogLevel : uint8_t { Info, Warning, Error };

static const size_t kLogTextSize = 120;

struct LogEntry {
    int64_t uptimeUs;
    LogLevel level;
    char text[kLogTextSize];
};

/// Fixed-size ring buffer of log entries; the oldest entry is overwritten when full.
/// No heap use, not thread-safe on its own (see RamLog for the locked wrapper).
template <size_t Capacity> class RingLog {
  public:
    void push(LogLevel level, int64_t uptimeUs, const char* text) {
        LogEntry& entry = entries_[head_];
        entry.uptimeUs = uptimeUs;
        entry.level = level;
        strncpy(entry.text, text != nullptr ? text : "", kLogTextSize - 1);
        entry.text[kLogTextSize - 1] = '\0';
        head_ = (head_ + 1) % Capacity;
        if (count_ < Capacity) {
            ++count_;
        }
    }

    size_t size() const { return count_; }
    size_t capacity() const { return Capacity; }

    /// Chronological access: index 0 is the oldest stored entry. index must be < size().
    const LogEntry& at(size_t index) const {
        const size_t start = (head_ + Capacity - count_) % Capacity;
        return entries_[(start + index) % Capacity];
    }

    void clear() {
        head_ = 0;
        count_ = 0;
    }

  private:
    LogEntry entries_[Capacity] = {};
    size_t head_ = 0;
    size_t count_ = 0;
};

} // namespace pu
