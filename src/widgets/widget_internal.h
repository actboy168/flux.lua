#pragma once

#include <cstdint>

#include "widgets/widget_runtime.h"

namespace flux::widgets {

widget_runtime_state& wr();
void draw_text_block(float x, float y, float width, float height, const char* text,
                     uint32_t color, float size, int align, int valign,
                     bool wrap, bool ellipsis, int max_lines, float line_height);

}
