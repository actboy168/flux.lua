#pragma once

#include <string>
#include <vector>

#include "ui/types.h"

namespace flux::widgets {

struct table_scope {
    flux::ui::ui_id id = 0;
    int row_count = 0;
    float body_height = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float header_height = 30.0f;
    float row_height = 28.0f;
    float content_width = 0.0f;
    float row_x = 0.0f;
    float row_y = 0.0f;
    float row_w = 0.0f;
    float row_h = 0.0f;
    float cell_x = 0.0f;
    std::vector<float> columns;
    int columns_count = 0;
    int current_col = 0;
    int current_row = 0;
    bool row_visible = true;
};

}
