#pragma once

#include <cmath>
#include <cstdlib>

namespace pu {

/// Parses a complete decimal number ("12", "-0.5", "1e3"). Rejects empty text, trailing
/// characters, surrounding whitespace, infinity and NaN. Pure logic, unit-tested on the host.
inline bool parseNumber(const char* text, double& out) {
    if (text == nullptr || *text == '\0' || *text == ' ' || *text == '\t' || *text == '\n') {
        return false;
    }
    char* end = nullptr;
    const double value = std::strtod(text, &end);
    if (end == text || *end != '\0' || !std::isfinite(value)) {
        return false;
    }
    out = value;
    return true;
}

/// True if `value` has no fractional part and fits a 32-bit signed integer.
inline bool isWholeNumber(double value) {
    return std::floor(value) == value && value >= -2147483648.0 && value <= 2147483647.0;
}

/// Optional lower and upper bound for a number entered by the user.
struct NumberRange {
    bool hasMin = false;
    bool hasMax = false;
    double min = 0.0;
    double max = 0.0;

    bool contains(double value) const {
        return (!hasMin || value >= min) && (!hasMax || value <= max);
    }
};

} // namespace pu
