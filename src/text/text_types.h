#pragma once

namespace flux::text {

struct text_options {
    float width = 0.0f;
    float height = 0.0f;
    float line_height = 0.0f;
    int max_lines = 0;
    int weight = 400;
    int align = 0;
    int valign = 0;
    bool wrap = false;
    bool ellipsis = false;
};

}
