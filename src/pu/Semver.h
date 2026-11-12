#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace pu {

/// Three-part version number as used for firmware releases ("v1.2.3" or "1.2.3").
/// Pure logic without Arduino dependencies so it can be unit-tested on the host.
struct Semver {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;

    /// Parses "v1.2.3" / "1.2.3". Anything else (missing parts, suffixes, overflow) is rejected.
    static bool parse(const char* text, Semver& out) {
        if (text == nullptr) {
            return false;
        }
        if (*text == 'v' || *text == 'V') {
            ++text;
        }
        uint16_t parts[3] = {0, 0, 0};
        for (int i = 0; i < 3; ++i) {
            if (*text < '0' || *text > '9') {
                return false;
            }
            uint32_t value = 0;
            while (*text >= '0' && *text <= '9') {
                value = value * 10 + static_cast<uint32_t>(*text - '0');
                if (value > UINT16_MAX) {
                    return false;
                }
                ++text;
            }
            parts[i] = static_cast<uint16_t>(value);
            if (i < 2) {
                if (*text != '.') {
                    return false;
                }
                ++text;
            }
        }
        if (*text != '\0') {
            return false;
        }
        out.major = parts[0];
        out.minor = parts[1];
        out.patch = parts[2];
        return true;
    }

    /// Returns <0, 0 or >0 like strcmp.
    int compare(const Semver& other) const {
        if (major != other.major) {
            return major < other.major ? -1 : 1;
        }
        if (minor != other.minor) {
            return minor < other.minor ? -1 : 1;
        }
        if (patch != other.patch) {
            return patch < other.patch ? -1 : 1;
        }
        return 0;
    }

    bool operator==(const Semver& other) const { return compare(other) == 0; }
    bool operator!=(const Semver& other) const { return compare(other) != 0; }
    bool operator<(const Semver& other) const { return compare(other) < 0; }
    bool operator>(const Semver& other) const { return compare(other) > 0; }

    /// Writes "v1.2.3" into buffer (always terminated if size > 0).
    void format(char* buffer, size_t size) const {
        snprintf(buffer, size, "v%u.%u.%u", static_cast<unsigned>(major),
                 static_cast<unsigned>(minor), static_cast<unsigned>(patch));
    }
};

} // namespace pu
