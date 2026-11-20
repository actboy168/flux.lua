#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace flux::text {

inline std::string int_to_string(int value) {
    char buffer[16];
    const std::to_chars_result result =
        std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, static_cast<std::size_t>(result.ptr - buffer));
}

// Equivalent to std::format("{:.{}g}", value, precision).
inline std::string float_to_string(double value, int precision = 6) {
    char buffer[64];
    const std::to_chars_result result =
        std::to_chars(buffer, buffer + sizeof(buffer), value,
                      std::chars_format::general, precision);
    if (result.ec != std::errc()) {
        return {};
    }
    return std::string(buffer, static_cast<std::size_t>(result.ptr - buffer));
}

// Writes 16 zero-padded lowercase hex digits to out.
inline char* uint64_to_hex(std::uint64_t value, char* out) {
    char digits[16];
    const std::to_chars_result result =
        std::to_chars(digits, digits + sizeof(digits), value, 16);
    const std::size_t length = static_cast<std::size_t>(result.ptr - digits);
    std::memset(out, '0', 16 - length);
    std::memcpy(out + (16 - length), digits, length);
    return out + 16;
}

}
