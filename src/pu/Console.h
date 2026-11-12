#pragma once

#include <Arduino.h>
#include <functional>
#include <mutex>

#include "ByteQueue.h"

namespace pu {

/// Drop-in replacement for Serial that mirrors all output to a second sink (the web console)
/// and merges input typed there into the same stream.
class Console : public Stream {
  public:
    using Mirror = std::function<void(const uint8_t*, size_t)>;
    static const size_t kInputCapacity = 256;

    /// Selects the underlying stream. Until called, the console uses Serial.
    void begin(Stream& primary) { primary_ = &primary; }

    /// Output written to the console is additionally passed to `mirror`.
    void setMirror(Mirror mirror) { mirror_ = std::move(mirror); }

    /// Feeds input from the secondary source (e.g. the web console). Safe from any task.
    /// Returns the number of bytes accepted; excess input is dropped.
    size_t injectInput(const uint8_t* data, size_t length);

    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    size_t write(uint8_t value) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    using Print::write;

  private:
    Stream* primary_ = &Serial;
    Mirror mirror_;
    std::mutex inputMutex_;
    ByteQueue<kInputCapacity> input_;
};

} // namespace pu
