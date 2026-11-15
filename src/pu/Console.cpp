#include "Console.h"

namespace pu {

size_t Console::injectInput(const uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    return input_.push(data, length);
}

void Console::releaseLine(const uint8_t* end, size_t endLength) {
    fromPrimary_.push(reinterpret_cast<const uint8_t*>(line_), lineLength_);
    fromPrimary_.push(end, endLength);
    lineLength_ = 0;
}

void Console::poll() {
    std::lock_guard<std::mutex> lock(inputMutex_);
    while (primary_->available() > 0) {
        const int next = primary_->read();
        if (next < 0) {
            break;
        }
        const uint8_t value = static_cast<uint8_t>(next);
        const bool lineEnd = value == '\n' || value == '\r';
        if (value == '\n' && skipNewline_) {
            skipNewline_ = false;
            continue;
        }
        skipNewline_ = false;
        if (!lineEnd) {
            if (lineTooLong_) {
                fromPrimary_.push(&value, 1);
            } else if (lineLength_ < kLineCapacity - 1) {
                line_[lineLength_++] = static_cast<char>(value);
            } else { // too long for a command: hand on what we have and the rest as it comes
                releaseLine(&value, 1);
                lineTooLong_ = true;
            }
            continue;
        }
        if (lineTooLong_) {
            lineTooLong_ = false;
            fromPrimary_.push(&value, 1);
            continue;
        }
        line_[lineLength_] = '\0';
        String line(line_);
        line.trim();
        if (filter_ && !line.isEmpty() && filter_(line)) {
            lineLength_ = 0; // handled: the program does not see it
            skipNewline_ = value == '\r';
            continue;
        }
        releaseLine(&value, 1);
    }
}

int Console::available() {
    poll();
    std::lock_guard<std::mutex> lock(inputMutex_);
    return static_cast<int>(fromPrimary_.size() + input_.size());
}

int Console::read() {
    poll();
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (fromPrimary_.size() > 0) {
        return fromPrimary_.pop();
    }
    return input_.pop();
}

int Console::peek() {
    poll();
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (fromPrimary_.size() > 0) {
        return fromPrimary_.peek();
    }
    return input_.peek();
}

void Console::flush() {
    primary_->flush();
}

size_t Console::write(uint8_t value) {
    return write(&value, 1);
}

size_t Console::write(const uint8_t* buffer, size_t size) {
    const size_t written = primary_->write(buffer, size);
    if (mirror_) {
        mirror_(buffer, size);
    }
    return written;
}

} // namespace pu
