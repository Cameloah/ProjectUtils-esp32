#include "Console.h"

namespace pu {

size_t Console::injectInput(const uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    return input_.push(data, length);
}

int Console::available() {
    const int primaryAvailable = primary_->available();
    std::lock_guard<std::mutex> lock(inputMutex_);
    return primaryAvailable + static_cast<int>(input_.size());
}

int Console::read() {
    if (primary_->available() > 0) {
        return primary_->read();
    }
    std::lock_guard<std::mutex> lock(inputMutex_);
    return input_.pop();
}

int Console::peek() {
    if (primary_->available() > 0) {
        return primary_->peek();
    }
    std::lock_guard<std::mutex> lock(inputMutex_);
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
