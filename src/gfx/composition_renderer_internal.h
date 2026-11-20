#pragma once

#include <format>
#include <string>

namespace flux::gfx::detail {

inline bool composition_fail(std::string& error, const char* what, HRESULT hr) {
    error = std::format("{} failed: 0x{:08X}", what, static_cast<uint32_t>(hr));
    return false;
}

}
