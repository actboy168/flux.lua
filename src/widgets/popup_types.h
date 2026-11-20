#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui/render_layer.h"
#include "ui/types.h"
#include "widgets/widget_state_store.h"

namespace flux::widgets {

struct popup_anim {
    static constexpr widget_state_kind kind = widget_state_kind::popup;

    float reveal = 0.0f;
    uint64_t last_frame = 0;
};

struct popup_scope {
    flux::ui::render_layer previous_layer = flux::ui::render_layer::content;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float saved_cursor_x = 0.0f;
    float saved_cursor_y = 0.0f;
    float saved_line_start_x = 0.0f;
    float saved_alpha = 1.0f;
    std::vector<flux::ui::clip_rect> saved_clips;
};

struct tooltip_request {
    bool pending = false;
    uint64_t frame = 0;
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float delay = 0.35f;
    float max_width = 0.0f;
    float offset_x = 16.0f;
    float offset_y = 10.0f;
    bool follow_mouse = true;
};

}
