#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace flux::ui::detail {

inline constexpr float pointer_tolerance = 0.05f;

inline uint32_t scale_alpha_color(uint32_t color, float factor) {
    if (factor >= 1.0f) {
        return color;
    }
    const uint32_t alpha = static_cast<uint32_t>(std::lround(
        ((color >> 24) & 0xFFu) * std::clamp(factor, 0.0f, 1.0f)));
    return (color & 0x00FFFFFFu) | (alpha << 24);
}

}
