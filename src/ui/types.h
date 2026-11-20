#pragma once

#include <cstdint>

namespace flux::ui {

using ui_id = uint64_t;

struct clip_rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

}
