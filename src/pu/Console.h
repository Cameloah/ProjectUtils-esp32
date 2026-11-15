#pragma once

#include <Arduino.h>
#include <functional>
#include <mutex>

#include "ByteQueue.h"

namespace pu {

/// Drop-in replacement for Serial that mirrors all output to a second sink (the web console)
/// and merges input typed there into the same stream.
///
/// Input from the primary stream (USB serial) is handed on line by line, so a line filter can
/// take out commands that must only work with physical access (see setUsbCommand()). Input
/// from the web console never passes that filter.
class Console : public Stream {
  public:
    using Mirror = std::function<void(const uint8_t*, size_t)>;
    /// Gets each complete line typed on the primary stream; returns true if it handled the line,
    /// which then does not reach the program.
    using LineFilter = std::function<bool(const String& line)>;
    static const size_t kInputCapacity = 256;

    /// Selects the underlying stream. Until called, the console uses Serial.
    void begin(Stream& primary) { primary_ = &primary; }

    /// Output written to the console is additionally passed to `mirror`.
    void setMirror(Mirror mirror) { mirror_ = std::move(mirror); }

    /// Lines from the primary stream are offered to `filter` first.
    void setLineFilter(LineFilter filter) { filter_ = std::move(filter); }

    /// Reads waiting input from the primary stream. Called by ProjectUtils.loop(), so filtered
    /// commands work even if the program never reads the console.
    void poll();

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
    static const size_t kLineCapacity = 32; // longer lines are no filtered command

    void releaseLine(const uint8_t* end, size_t endLength); // caller holds inputMutex_

    Stream* primary_ = &Serial;
    Mirror mirror_;
    LineFilter filter_;
    std::mutex inputMutex_;
    ByteQueue<kInputCapacity> input_;       // from the web console
    ByteQueue<kInputCapacity> fromPrimary_; // from the primary stream, whole lines
    char line_[kLineCapacity] = {};
    size_t lineLength_ = 0;
    bool lineTooLong_ = false;
    bool skipNewline_ = false; // "\r\n": the "\n" belongs to the line ended by "\r"
};

} // namespace pu
