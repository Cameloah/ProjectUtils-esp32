#pragma once

#include <cstddef>
#include <string>

namespace pu {

/// Derives a network hostname from a free-text device name: whitespace is dropped, letters are
/// lowercased, and everything outside [a-z0-9-] is removed. Falls back to "esp32" if nothing
/// usable remains. Pure logic so it can be unit-tested on the host.
inline std::string makeHostname(const char* deviceName) {
    static const size_t kMaxLength = 32;
    std::string result;
    if (deviceName != nullptr) {
        for (const char* p = deviceName; *p != '\0' && result.size() < kMaxLength; ++p) {
            char c = *p;
            if (c >= 'A' && c <= 'Z') {
                c = static_cast<char>(c - 'A' + 'a');
            }
            const bool isAlnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
            if (isAlnum || (c == '-' && !result.empty())) {
                result += c;
            }
        }
    }
    while (!result.empty() && result.back() == '-') {
        result.pop_back();
    }
    if (result.empty()) {
        result = "esp32";
    }
    return result;
}

} // namespace pu
