#include "widgets/widget_internal.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "runtime/services.h"
#include "ui/motion.h"
#include "text/charconv.h"
#include "widgets/control_anim.h"
#include "widgets/widget_runtime.h"
#include "widgets/widget_paint.h"
#include "widgets/widgets.h"

namespace flux::widgets {

struct tooltip_layout {
    float font_size = 14.0f;
    float padding_x = 0.0f;
    float padding_y = 0.0f;
    float text_width = 0.0f;
    float text_height = 0.0f;
    float wrap_width = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

tooltip_layout measure_tooltip(const std::string& value, float max_width) {
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    tooltip_layout layout;
    layout.font_size = style.font_size - 1.0f;
    layout.padding_x = style.tooltip_padding_x;
    layout.padding_y = style.tooltip_padding_y;
    flux::runtime::services().measure_text(value.c_str(), layout.font_size,
                        layout.text_width, layout.text_height);
    layout.width = layout.text_width + layout.padding_x * 2.0f;
    layout.height = layout.text_height + layout.padding_y * 2.0f;
    if (max_width > 0.0f && layout.width > max_width) {
        layout.wrap_width = std::max(20.0f, max_width - layout.padding_x * 2.0f);
        flux::text::text_options options;
        options.width = layout.wrap_width;
        options.wrap = true;
        options.ellipsis = false;
        options.line_height = style.line_height;
        flux::runtime::services().measure_text(value.c_str(), layout.font_size, options,
                            layout.text_width, layout.text_height);
        layout.width = layout.wrap_width + layout.padding_x * 2.0f;
        layout.height = layout.text_height + layout.padding_y * 2.0f;
    }
    return layout;
}
void draw_tooltip_at(const std::string& value, float x, float y, float delay,
                     const tooltip_layout& layout) {
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    const uint64_t frame = context.frame_index();
    if (wr().tooltip_frame == 0 || frame - wr().tooltip_frame > 1
        || wr().tooltip_text != value) {
        wr().tooltip_reveal = 0.0f;
        wr().tooltip_hover_start = context.time();
        wr().tooltip_text = value;
    }
    wr().tooltip_frame = frame;
    if (delay > 0.0f && context.time() - wr().tooltip_hover_start < delay) {
        return;
    }

    flux::ui::draw_list& draw = flux::runtime::services().draw;
    const flux::ui::render_layer previous = draw.active_layer();
    const bool switched = draw.use_layer(flux::ui::render_layer::tooltip);
    wr().tooltip_reveal = control_anim_step(
        wr().tooltip_reveal, 1.0f, context.delta_time(), flux::ui::motion::fast, frame);
    const float saved_alpha = draw.alpha;
    draw.alpha = saved_alpha * ease_out_cubic(wr().tooltip_reveal);
    draw.shadow_rect(x, y, layout.width, layout.height, 6.0f, style.tooltip_shadow,
                     style.tooltip_shadow_spread, style.tooltip_shadow_offset_y);
    draw.rect_filled(x, y, layout.width, layout.height, style.tooltip_bg, 6.0f);
    draw.rect(x, y, layout.width, layout.height, style.tooltip_border, 6.0f,
              style.border_size);
    draw_text_block(
        x + layout.padding_x, y + layout.padding_y,
        layout.wrap_width > 0.0f ? layout.wrap_width : layout.text_width,
        layout.text_height,
        value.c_str(), style.text, layout.font_size,
        0, 1, layout.wrap_width > 0.0f, false, 0, style.line_height);
    draw.alpha = saved_alpha;
    if (switched) {
        draw.use_layer(previous);
    }
}
float default_tooltip_max_width() {
    const float window_width = static_cast<float>(flux::runtime::services().width);
    if (window_width <= 0.0f) {
        return 0.0f;
    }
    return std::min(window_width - 8.0f, std::max(160.0f, window_width * 0.6f));
}
void tooltip(const char* text, float x, float y, float delay, float max_width) {
    const std::string value = text != nullptr ? text : "";
    const tooltip_layout layout = measure_tooltip(value, max_width);

    const float window_width = static_cast<float>(flux::runtime::services().width);
    const float window_height = static_cast<float>(flux::runtime::services().height);
    if (window_width > 0.0f && x + layout.width > window_width - 4.0f) {
        x = std::max(4.0f, window_width - layout.width - 4.0f);
    }
    if (window_height > 0.0f && y + layout.height > window_height - 4.0f) {
        y = std::max(4.0f, window_height - layout.height - 4.0f);
    }
    draw_tooltip_at(value, x, y, delay, layout);
}
void rect_tooltip(const char* text, float x, float y, float width, float height,
                  const tooltip_options& options) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::input_state& input = context.input;
    if (!input.window_active || !input.mouse_inside || context.modal()) {
        return;
    }
    if (input.mouse_x < x || input.mouse_x >= x + width
        || input.mouse_y < y || input.mouse_y >= y + height) {
        return;
    }
    tooltip_request& request = wr().tooltip;
    request.pending = true;
    request.frame = context.frame_index();
    request.text = text;
    request.x = x;
    request.y = y;
    request.w = width;
    request.h = height;
    request.delay = options.delay;
    request.max_width = options.max_width;
    request.offset_x = options.offset_x;
    request.offset_y = options.offset_y;
    request.follow_mouse = options.follow_mouse;
}
void flush_tooltips() {
    tooltip_request& request = wr().tooltip;
    if (!request.pending) {
        return;
    }
    if (request.frame != widget_ui().frame_index()) {
        request.pending = false;
        return;
    }
    const std::string text = request.text;
    const float delay = request.delay;
    const float offset_x = request.offset_x;
    const float offset_y = request.offset_y;
    const bool follow_mouse = request.follow_mouse;
    const float anchor_x = request.x;
    const float anchor_y = request.y;
    const float anchor_w = request.w;
    const float anchor_h = request.h;
    const float max_width = request.max_width > 0.0f
        ? request.max_width
        : default_tooltip_max_width();
    request.pending = false;
    if (widget_ui().modal()) {
        return;
    }

    const tooltip_layout layout = measure_tooltip(text, max_width);
    const flux::ui::input_state& input = widget_ui().input;
    const float window_width = static_cast<float>(flux::runtime::services().width);
    const float window_height = static_cast<float>(flux::runtime::services().height);
    float x;
    float y;
    if (follow_mouse) {
        x = input.mouse_x + offset_x;
        y = input.mouse_y + offset_y;
        if (window_width > 0.0f && x + layout.width > window_width - 4.0f) {
            x = input.mouse_x - layout.width - offset_x;
        }
        if (window_height > 0.0f && y + layout.height > window_height - 4.0f) {
            y = input.mouse_y - layout.height - offset_y;
        }
    } else {
        x = anchor_x;
        y = anchor_y + anchor_h + offset_y;
        if (window_width > 0.0f && x + layout.width > window_width - 4.0f) {
            x = anchor_x + anchor_w - layout.width;
        }
        if (window_height > 0.0f && y + layout.height > window_height - 4.0f) {
            y = anchor_y - layout.height - offset_y;
        }
    }
    if (window_width > 0.0f) {
        x = std::max(4.0f, std::min(x, window_width - layout.width - 4.0f));
    }
    if (window_height > 0.0f) {
        y = std::max(4.0f, std::min(y, window_height - layout.height - 4.0f));
    }
    draw_tooltip_at(text, x, y, delay, layout);
}
}
