#pragma once

#include <algorithm>
#include <cmath>

namespace flux::ui::pixel {

inline float snap(float value, float dpi) {
    const float scale = dpi > 0.0f ? dpi : 1.0f;
    return (std::floor(value * scale) + 0.5f) / scale;
}

inline void snap_rect(float& x, float& y, float& width, float& height, float dpi) {
    const float x0 = snap(x, dpi);
    const float y0 = snap(y, dpi);
    const float x1 = snap(x + width, dpi);
    const float y1 = snap(y + height, dpi);
    x = x0;
    y = y0;
    width = std::max(0.0f, x1 - x0);
    height = std::max(0.0f, y1 - y0);
}

}
