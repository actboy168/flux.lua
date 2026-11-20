#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace flux::text {

inline size_t utf8_codepoint_length(unsigned char lead) {
    if (lead < 0x80) {
        return 1;
    }
    if ((lead & 0xE0) == 0xC0) {
        return 2;
    }
    if ((lead & 0xF0) == 0xE0) {
        return 3;
    }
    if ((lead & 0xF8) == 0xF0) {
        return 4;
    }
    return 1;
}

inline size_t utf8_next_offset(std::string_view text, size_t offset) {
    if (offset >= text.size()) {
        return text.size();
    }
    size_t length = utf8_codepoint_length(static_cast<unsigned char>(text[offset]));
    return offset + length < text.size() ? offset + length : text.size();
}

inline size_t utf8_prev_offset(std::string_view text, size_t offset) {
    if (offset == 0) {
        return 0;
    }
    size_t index = offset - 1;
    while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80) {
        --index;
    }
    return index;
}

inline uint32_t utf8_codepoint_at(std::string_view text, size_t offset, size_t* length) {
    if (offset >= text.size()) {
        if (length != nullptr) {
            *length = 0;
        }
        return 0;
    }
    const unsigned char lead = static_cast<unsigned char>(text[offset]);
    const size_t size = utf8_codepoint_length(lead);
    if (offset + size > text.size()) {
        if (length != nullptr) {
            *length = 1;
        }
        return lead;
    }
    uint32_t value = 0;
    switch (size) {
    case 1:
        value = lead;
        break;
    case 2:
        value = (static_cast<uint32_t>(lead & 0x1F) << 6)
            | static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3F);
        break;
    case 3:
        value = (static_cast<uint32_t>(lead & 0x0F) << 12)
            | (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3F) << 6)
            | static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 2]) & 0x3F);
        break;
    default:
        value = (static_cast<uint32_t>(lead & 0x07) << 18)
            | (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3F) << 12)
            | (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 2]) & 0x3F) << 6)
            | static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 3]) & 0x3F);
        break;
    }
    if (length != nullptr) {
        *length = size;
    }
    return value;
}

inline size_t utf8_to_utf16_offset(std::string_view text, size_t byte_offset) {
    size_t offset = 0;
    size_t units = 0;
    while (offset < text.size() && offset < byte_offset) {
        size_t length = 0;
        const uint32_t codepoint = utf8_codepoint_at(text, offset, &length);
        units += codepoint > 0xFFFF ? 2 : 1;
        offset += length;
    }
    return units;
}

inline size_t utf16_to_utf8_offset(std::string_view text, size_t unit_offset) {
    size_t offset = 0;
    size_t units = 0;
    while (offset < text.size() && units < unit_offset) {
        size_t length = 0;
        const uint32_t codepoint = utf8_codepoint_at(text, offset, &length);
        units += codepoint > 0xFFFF ? 2 : 1;
        offset += length;
    }
    return offset;
}

}
