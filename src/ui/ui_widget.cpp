#include "ui/ui_context.h"

#include "ui/draw_list.h"
#include "ui/ui_internal.h"

namespace flux::ui {

using detail::scale_alpha_color;
constexpr float k_pointer_tolerance = detail::pointer_tolerance;

widget_view_state& ui_context::state_for(ui_id id) {
    return states_[id];
}

const widget_view_state* ui_context::find_state(ui_id id) const {
    auto it = states_.find(id);
    return it == states_.end() ? nullptr : &it->second;
}

void ui_context::register_focus(ui_id id) {
    if (std::find(focus_order_.begin(), focus_order_.end(), id) == focus_order_.end()) {
        focus_order_.push_back(id);
    }
}

void ui_context::widget(const char* label, float width, float height, bool focusable,
                       bool& hovered, bool& active, bool& clicked, bool& double_clicked,
                       bool& focused, bool allow_modal, ui_id id) {
    widget_input_options options;
    options.interactive = focusable;
    options.focusable = focusable;
    options.allow_modal = allow_modal;
    options.tab_stop = focusable;
    widget_ex(label, width, height, options, hovered, active, clicked, double_clicked, focused, id);
}

void ui_context::widget_ex(const char* label, float width, float height,
                           const widget_input_options& options,
                           bool& hovered, bool& active, bool& clicked, bool& double_clicked,
                           bool& focused, ui_id id) {
    const float x = cursor_x_;
    const float y = cursor_y_;
    item_x_ = x;
    item_y_ = y;
    item_w_ = width;
    item_h_ = height;

    if (!is_inside_clip(x, y, width, height)) {
        item_visible_ = false;
        last_item_id_ = 0;
        hovered = false;
        active = false;
        clicked = false;
        double_clicked = false;
        focused = false;
        advance_cursor(x, y, width, height);
        return;
    }
    item_visible_ = true;

    const ui_id resolved_id = id != 0 ? id : make_id(label);
    widget_view_state& state = state_for(resolved_id);
    state.last_frame = frame_index_;
    state.clicked = false;
    state.double_clicked = false;
    state.x = x;
    state.y = y;
    state.w = width;
    state.h = height;
    last_item_id_ = resolved_id;

    const bool modal_ok = (!modal_ || popup_active_) || options.allow_modal;
    const bool input_enabled = options.interactive && modal_ok;
    const bool focus_enabled = options.focusable && modal_ok;
    const bool clip_inside = is_inside_clip(input.mouse_x, input.mouse_y, 1.0f, 1.0f);
    const bool geometry_inside = !input.lost_focus && clip_inside
        && input.mouse_x >= x && input.mouse_x < x + width
        && input.mouse_y >= y && input.mouse_y < y + height;
    const bool active_inside = !input.lost_focus && clip_inside
        && input.mouse_x >= x - k_pointer_tolerance
        && input.mouse_x <= x + width + k_pointer_tolerance
        && input.mouse_y >= y - k_pointer_tolerance
        && input.mouse_y <= y + height + k_pointer_tolerance;
    state.hovered = input_enabled && geometry_inside
        && input.mouse_inside && input.window_active;

    if (input_enabled && geometry_inside && input.mouse_pressed[0]) {
        active_id_ = resolved_id;
        focus_visible_ = false;
        if (focus_enabled) {
            focused_id_ = resolved_id;
        }
    }

    if (input_enabled && active_id_ == resolved_id) {
        state.active = true;
        if (input.mouse_released[0]) {
            if (active_inside) {
                state.clicked = true;
            }
            active_id_ = 0;
            state.active = false;
        }
    } else {
        state.active = false;
    }

    if (input_enabled && geometry_inside && input.mouse_double_clicked[0]) {
        state.double_clicked = true;
    }

    state.focused = focus_enabled && focused_id_ == resolved_id;
    if (focus_enabled && state.focused
        && (input.key_pressed[keys::enter] || input.key_pressed[keys::space])) {
        state.clicked = true;
    }

    if (focus_enabled && options.tab_stop) {
        register_focus(resolved_id);
    }
    advance_cursor(x, y, width, height);

    hovered = state.hovered;
    active = state.active;
    clicked = state.clicked;
    double_clicked = state.double_clicked;
    focused = state.focused;
}

bool ui_context::is_item_hovered() const {
    const widget_view_state* state = find_state(last_item_id_);
    return state != nullptr && state->hovered;
}

bool ui_context::is_item_active() const {
    const widget_view_state* state = find_state(last_item_id_);
    return state != nullptr && state->active;
}

bool ui_context::is_item_clicked() const {
    const widget_view_state* state = find_state(last_item_id_);
    return state != nullptr && state->clicked;
}

bool ui_context::is_item_double_clicked() const {
    const widget_view_state* state = find_state(last_item_id_);
    return state != nullptr && state->double_clicked;
}

bool ui_context::is_item_focused() const {
    const widget_view_state* state = find_state(last_item_id_);
    return state != nullptr && state->focused;
}

bool ui_context::is_item_visible() const {
    return item_visible_;
}

void ui_context::last_item_rect(float& x, float& y, float& width, float& height) const {
    x = item_x_;
    y = item_y_;
    width = item_w_;
    height = item_h_;
}

void ui_context::set_keyboard_focus(const char* label) {
    focused_id_ = make_id(label);
    focus_visible_ = true;
}

void ui_context::set_keyboard_focus_id(ui_id id) {
    focused_id_ = id;
    focus_visible_ = true;
}

void ui_context::set_modal(bool modal) {
    modal_ = modal;
}

}
