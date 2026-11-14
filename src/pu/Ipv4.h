#pragma once

#include <cstdint>

namespace pu {

/// Strict IPv4 parsing ("192.168.1.20": four decimal parts 0..255, nothing else) and checks for
/// a static address. Pure functions, tested on the host.
namespace ipv4 {

/// Parses `text` into `out` (first part in the highest byte). Returns false on anything that is
/// not exactly a dotted-quad address.
inline bool parse(const char* text, uint32_t& out) {
    if (text == nullptr) {
        return false;
    }
    uint32_t result = 0;
    for (int part = 0; part < 4; ++part) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        uint32_t value = 0;
        int digits = 0;
        while (*text >= '0' && *text <= '9') {
            value = value * 10U + static_cast<uint32_t>(*text - '0');
            ++text;
            if (++digits > 3 || value > 255U) {
                return false;
            }
        }
        if (digits > 1 && value < (digits == 2 ? 10U : 100U)) {
            return false; // leading zero ("010") is ambiguous, some tools read it as octal
        }
        result = (result << 8U) | value;
        if (part < 3) {
            if (*text != '.') {
                return false;
            }
            ++text;
        }
    }
    if (*text != '\0') {
        return false;
    }
    out = result;
    return true;
}

/// A subnet mask is a run of ones followed by zeros. /31 and /32 leave no room for a gateway.
inline bool isValidMask(uint32_t mask) {
    const uint32_t inverted = ~mask;
    const bool contiguous = (inverted & (inverted + 1U)) == 0U;
    return contiguous && mask != 0U && inverted > 1U;
}

enum class StaticIpError : uint8_t {
    None,
    InvalidMask,
    AddressNotUsable, ///< 0.x, 127.x, multicast, or the network/broadcast address of the subnet
    GatewayNotUsable, ///< same reasons as above
    GatewayOutsideSubnet,
    GatewayIsAddress,
};

/// Checks that address, mask and gateway form a usable static configuration.
inline StaticIpError checkStatic(uint32_t address, uint32_t mask, uint32_t gateway) {
    if (!isValidMask(mask)) {
        return StaticIpError::InvalidMask;
    }
    const auto usable = [mask](uint32_t ip) {
        const uint32_t first = ip >> 24U;
        const uint32_t host = ip & ~mask;
        return first != 0U && first != 127U && first < 224U && host != 0U && host != ~mask;
    };
    if (!usable(address)) {
        return StaticIpError::AddressNotUsable;
    }
    if (!usable(gateway)) {
        return StaticIpError::GatewayNotUsable;
    }
    if ((address & mask) != (gateway & mask)) {
        return StaticIpError::GatewayOutsideSubnet;
    }
    if (address == gateway) {
        return StaticIpError::GatewayIsAddress;
    }
    return StaticIpError::None;
}

inline const char* describe(StaticIpError error) {
    switch (error) {
    case StaticIpError::InvalidMask:
        return "the subnet mask is not valid";
    case StaticIpError::AddressNotUsable:
        return "the IP address cannot be used for a device in this subnet";
    case StaticIpError::GatewayNotUsable:
        return "the gateway address cannot be used in this subnet";
    case StaticIpError::GatewayOutsideSubnet:
        return "the gateway is not in the same subnet as the IP address";
    case StaticIpError::GatewayIsAddress:
        return "the IP address and the gateway must differ";
    default:
        return "ok";
    }
}

} // namespace ipv4
} // namespace pu
