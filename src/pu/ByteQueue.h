#pragma once

#include <cstddef>
#include <cstdint>

namespace pu {

/// Fixed-size FIFO of bytes. Bytes that do not fit are dropped by push().
/// Not thread-safe on its own.
template <size_t Capacity> class ByteQueue {
  public:
    /// Returns the number of bytes actually stored.
    size_t push(const uint8_t* data, size_t length) {
        size_t stored = 0;
        while (stored < length && count_ < Capacity) {
            buffer_[(head_ + count_) % Capacity] = data[stored];
            ++count_;
            ++stored;
        }
        return stored;
    }

    /// Returns the next byte, or -1 if empty.
    int pop() {
        if (count_ == 0) {
            return -1;
        }
        const uint8_t value = buffer_[head_];
        head_ = (head_ + 1) % Capacity;
        --count_;
        return value;
    }

    /// Returns the next byte without removing it, or -1 if empty.
    int peek() const { return count_ == 0 ? -1 : buffer_[head_]; }

    size_t size() const { return count_; }
    size_t capacity() const { return Capacity; }

  private:
    uint8_t buffer_[Capacity] = {};
    size_t head_ = 0;
    size_t count_ = 0;
};

} // namespace pu
