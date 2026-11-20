#pragma once

#include <d2d1.h>

#include <cstdint>

namespace flux::gfx {

inline D2D1_COLOR_F color_from_u32(uint32_t color) {
    D2D1_COLOR_F result;
    result.r = (color & 0xFFu) / 255.0f;
    result.g = ((color >> 8) & 0xFFu) / 255.0f;
    result.b = ((color >> 16) & 0xFFu) / 255.0f;
    result.a = ((color >> 24) & 0xFFu) / 255.0f;
    return result;
}

}
