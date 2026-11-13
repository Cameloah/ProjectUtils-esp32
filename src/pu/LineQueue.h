#pragma once

#include <cstddef>
#include <cstdint>

namespace pu {

/// Fixed-size FIFO that takes a byte stream in and hands whole lines out.
///
/// Lines end at '\n'; '\r' and NUL bytes are discarded. Bytes that do not fit are dropped by
/// push(). A line longer than the queue or than the caller's buffer comes out in several pieces.
/// Not thread-safe on its own. Pure logic so it can be unit-tested on the host.
template <size_t Capacity> class LineQueue {
  public:
    /// Returns the number of bytes consumed (stored or deliberately discarded).
    size_t push(const uint8_t* data, size_t length) {
        size_t consumed = 0;
        for (; consumed < length; ++consumed) {
            const uint8_t value = data[consumed];
            if (value == '\r' || value == '\0') {
                continue;
            }
            if (count_ == Capacity) {
                break;
            }
            buffer_[(head_ + count_) % Capacity] = value;
            ++count_;
            if (value == '\n') {
                ++lines_;
            }
        }
        return consumed;
    }

    /// True if pop() would return a line.
    bool hasLine() const { return lines_ > 0 || count_ == Capacity; }

    /// Copies the next line into `out` (NUL-terminated, without the line break) and returns its
    /// length, or -1 if no complete line is waiting.
    int pop(char* out, size_t outSize) {
        if (!hasLine() || outSize == 0) {
            return -1;
        }
        size_t length = 0;
        while (count_ > 0 && length + 1 < outSize) {
            const uint8_t value = buffer_[head_];
            head_ = (head_ + 1) % Capacity;
            --count_;
            if (value == '\n') {
                --lines_;
                break;
            }
            out[length] = static_cast<char>(value);
            ++length;
        }
        out[length] = '\0';
        return static_cast<int>(length);
    }

    size_t size() const { return count_; }

  private:
    uint8_t buffer_[Capacity] = {};
    size_t head_ = 0;
    size_t count_ = 0;
    size_t lines_ = 0;
};

} // namespace pu
