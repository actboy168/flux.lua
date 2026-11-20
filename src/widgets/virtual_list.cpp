#include "widgets/virtual_list.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include "runtime/services.h"
#include "text/charconv.h"
#include "widgets/control_anim.h"
#include "widgets/widget_runtime.h"
#include "widgets/widget_paint.h"

namespace flux::widgets {

namespace {

std::string make_row_label(flux::ui::ui_id list_id, int index) {
    return std::format("vl{:016x}_{}", list_id, index);
}

flux::ui::ui_id row_id_for(flux::ui::ui_context& ui, flux::ui::ui_id list_id, int index) {
    const std::string label = make_row_label(list_id, index);
    return ui.make_id_absolute(label.c_str());
}

float max_scroll_for(float content_height, float height) {
    return std::max(0.0f, content_height - height);
}

void scroll_to_index(flux::ui::ui_context& ui, flux::ui::ui_id list_id, float row_height,
                     float height, float content_height, int index,
                     list_scroll_align align) {
    const float max_scroll = max_scroll_for(content_height, height);
    float target = 0.0f;
    switch (align) {
    case list_scroll_align::top:
        target = static_cast<float>(index) * row_height;
        break;
    case list_scroll_align::center:
        target = static_cast<float>(index) * row_height
            - (height - row_height) * 0.5f;
        break;
    case list_scroll_align::bottom:
        target = static_cast<float>(index + 1) * row_height - height;
        break;
    }
    ui.set_child_scroll(list_id, std::clamp(target, 0.0f, max_scroll));
}

list_state* find_list(flux::ui::ui_context& ui, flux::ui::ui_id id) {
    return widget_runtime().states.find<list_state>(id);
}

list_state* find_list(flux::ui::ui_context& ui, const char* label) {
    if (label == nullptr) {
        return nullptr;
    }
    return find_list(ui, ui.make_id_absolute(label));
}

}

begin_list_result begin_list(const char* label, int count, float row_height,
                             float width, float height, selection_mode mode,
                             flux::ui::ui_id explicit_id) {
    begin_list_result result;
    if (label == nullptr || count < 0 || row_height <= 0.0f
        || width <= 0.0f || height <= 0.0f) {
        return result;
    }

    flux::ui::ui_context& ui = widget_ui();
    const flux::ui::ui_id id = explicit_id != 0 ? explicit_id : ui.make_id_absolute(label);
    list_state& state = widget_runtime().states.get<list_state>(id, ui.frame_index());
    state.changed = false;
    if (state.selection.count != count || state.selection.mode != mode) {
        if (state.selection.resize(count, mode)) {
            state.changed = true;
        }
    }
    state.focus_row_id = state.selection.focused >= 0
        ? row_id_for(ui, id, state.selection.focused)
        : 0;

    const float content_height = static_cast<float>(count) * row_height;

    if (state.pending_scroll_index >= 0 && count > 0) {
        const int index = std::clamp(state.pending_scroll_index, 0, count - 1);
        scroll_to_index(ui, id, row_height, height, content_height, index,
                        state.pending_scroll_align);
        state.pending_scroll_index = -1;
    }

    if (!ui.modal() && count > 0 && state.focus_row_id != 0
        && ui.keyboard_focus_id() == state.focus_row_id) {
        const flux::ui::input_state& input = ui.input;
        int target = state.selection.focused >= 0 ? state.selection.focused : 0;
        const int visible = std::max(1, static_cast<int>(height / row_height));
        bool moved = false;
        if (input.key_pressed[flux::ui::keys::up]) {
            target -= 1;
            moved = true;
        } else if (input.key_pressed[flux::ui::keys::down]) {
            target += 1;
            moved = true;
        } else if (input.key_pressed[flux::ui::keys::home]) {
            target = 0;
            moved = true;
        } else if (input.key_pressed[flux::ui::keys::end]) {
            target = count - 1;
            moved = true;
        } else if (input.key_pressed[flux::ui::keys::page_up]) {
            target -= visible;
            moved = true;
        } else if (input.key_pressed[flux::ui::keys::page_down]) {
            target += visible;
            moved = true;
        }

        if (input.key_ctrl[flux::ui::keys::a] && input.key_pressed[flux::ui::keys::a]) {
            if (state.selection.mode == selection_mode::single) {
                const int index = state.selection.focused >= 0
                    ? state.selection.focused : 0;
                if (state.selection.select_only(index)) {
                    state.changed = true;
                }
            } else if (state.selection.select_all()) {
                state.changed = true;
            }
        }

        if (moved) {
            target = std::clamp(target, 0, count - 1);
            if (state.selection.apply_keyboard_move(
                    target, input.ctrl, input.shift)) {
                state.changed = true;
            }
            const flux::ui::ui_id target_id = row_id_for(ui, id, target);
            state.focus_row_id = target_id;
            ui.set_keyboard_focus_id(target_id);

            const float max_scroll = max_scroll_for(content_height, height);
            float scroll = ui.child_scroll(id);
            const int first = std::clamp(
                static_cast<int>(std::floor(scroll / row_height)), 0, count - 1);
            if (target < first) {
                scroll = static_cast<float>(target) * row_height;
            } else if (target >= first + visible) {
                scroll = static_cast<float>(target - visible + 1) * row_height;
            }
            ui.set_child_scroll(id, std::clamp(scroll, 0.0f, max_scroll));
        }
    }

    const float x = ui.cursor_x();
    const float y = ui.cursor_y();
    if (!ui.begin_child_fixed(label, width, height, content_height, id)) {
        return result;
    }

    result.active = true;
    result.count = count;
    result.content_height = content_height;
    result.scroll = ui.child_scroll(id);
    if (count > 0) {
        constexpr float epsilon = 1e-4f;
        result.first = std::clamp(
            static_cast<int>(std::floor(result.scroll / row_height + epsilon)),
            0, count - 1);
        result.last = std::clamp(
            static_cast<int>(std::ceil(
                (result.scroll + height) / row_height - epsilon)) - 1,
            result.first, count - 1);
    }

    const float row_width = std::max(
        1.0f, width - (content_height > height ? ui.style().scrollbar_size : 0.0f));

    list_scope frame;
    frame.id = id;
    frame.state = &state;
    frame.x = x;
    frame.content_top = ui.cursor_y();
    frame.row_height = row_height;
    frame.row_width = row_width;
    frame.count = count;
    frame.first = result.first;
    frame.last = result.last;
    widget_runtime().list_scopes.push_back(std::move(frame));
    return result;
}

list_item_result list_item(int index) {
    list_item_options options;
    return list_item_ex(index, options);
}

list_item_result list_item_ex(int index, const list_item_options& options) {
    list_item_result result;
    if (widget_runtime().list_scopes.empty()) {
        return result;
    }
    list_scope& frame = widget_runtime().list_scopes.back();
    if (index < frame.first || index > frame.last || index < 0
        || index >= frame.count) {
        return result;
    }

    flux::ui::ui_context& ui = widget_ui();
    list_state& state = *frame.state;
    const std::string row_label = make_row_label(frame.id, index);
    const flux::ui::ui_id row_id = ui.make_id_absolute(row_label.c_str());

    ui.set_cursor_pos(frame.x, frame.content_top + static_cast<float>(index) * frame.row_height);
    bool hovered = false;
    bool active = false;
    bool clicked = false;
    bool double_clicked = false;
    bool focused = false;
    flux::ui::ui_context::widget_input_options input_options;
    input_options.interactive = true;
    input_options.focusable = options.focusable;
    input_options.tab_stop = options.tab_stop;
    ui.widget_ex(row_label.c_str(), frame.row_width, frame.row_height, input_options,
                 hovered, active, clicked, double_clicked, focused, row_id);

    const float row_top = frame.content_top + static_cast<float>(index) * frame.row_height;

    result.visible = ui.is_item_visible();
    result.clicked = clicked;
    result.double_clicked = double_clicked;
    result.focused = focused;
    result.hovered = hovered;
    result.active = active;

    bool changed = false;
    if (!ui.modal()) {
        if (clicked) {
            changed = state.selection.apply_pointer(
                index, ui.input.ctrl, ui.input.shift) || changed;
        }

        if (result.visible) {
            const flux::ui::input_state& input = ui.input;
            const bool inside_row = ui.is_rect_visible(
                input.mouse_x, input.mouse_y, 1.0f, 1.0f)
                && input.mouse_x >= frame.x
                && input.mouse_x < frame.x + frame.row_width
                && input.mouse_y >= row_top
                && input.mouse_y < row_top + frame.row_height;
            if (input.mouse_pressed[1] && inside_row) {
                result.right_clicked = true;
                changed = state.selection.apply_right_click(index) || changed;
                state.focus_row_id = row_id;
                ui.set_keyboard_focus_id(row_id);
                ui.set_focus_visible(false);
            }
        }

        if (focused || clicked) {
            state.selection.focused = index;
            state.focus_row_id = row_id;
        }
    }

    if (changed) {
        state.changed = true;
    }
    result.changed = changed;
    result.selected = state.selection.is_selected(index);

    if (result.visible) {
        float rx = 0.0f;
        float ry = 0.0f;
        float rw = 0.0f;
        float rh = 0.0f;
        ui.last_item_rect(rx, ry, rw, rh);
        const flux::ui::style_sheet& style = ui.style();
        const float rounding = std::max(0.0f, style.rounding - 3.0f);

        if (options.chrome) {
            control_anim_targets targets;
            targets.hovered = hovered;
            targets.active = active;
            targets.focused = focused;
            targets.has_value = true;
            targets.value = result.selected;
            const control_anim_state& anim =
                control_anim(row_id, targets, ui.frame_index());
            const float highlight = std::max(anim.hover, anim.press);
            const uint32_t hover_fill = mix_color(0x00000000, style.panel_hover, highlight);
            const uint32_t fill = mix_color(hover_fill, style.panel_active, anim.value);
            if ((fill & 0xFF000000u) != 0) {
                flux::runtime::services().draw.rect_filled(rx, ry, rw, rh, fill, rounding);
            }
            if (ui.focus_visible() && anim.focus > 0.002f) {
                flux::runtime::services().draw.rect(rx, ry, rw, rh, with_alpha(style.accent, anim.focus),
                                 rounding, 1.0f);
            }
            flux::runtime::services().draw.rect_filled(
                rx, ry + rh - std::max(1.0f, style.border_size),
                rw, std::max(1.0f, style.border_size), style.border, 0.0f);
        }

    }

    return result;
}

bool end_list() {
    if (widget_runtime().list_scopes.empty()) {
        widget_runtime().fail("end_list called without begin_list");
        return false;
    }
    flux::ui::ui_context& ui = widget_ui();
    ui.end_child();
    widget_runtime().list_scopes.pop_back();
    return true;
}

bool list_exists(const char* label) {
    return find_list(widget_ui(), label) != nullptr;
}

bool list_exists(flux::ui::ui_id id) {
    return find_list(widget_ui(), id) != nullptr;
}

bool list_selected(const char* label, int index) {
    return list_selected(widget_ui().make_id_absolute(label), index);
}

bool list_selected(flux::ui::ui_id id, int index) {
    const list_state* state = find_list(widget_ui(), id);
    return state != nullptr && state->selection.is_selected(index);
}

bool list_set_selected(const char* label, int index) {
    return list_set_selected(widget_ui().make_id_absolute(label), index);
}

bool list_set_selected(flux::ui::ui_id id, int index) {
    flux::ui::ui_context& ui = widget_ui();
    list_state* state = find_list(ui, id);
    if (state == nullptr) {
        return false;
    }
    bool changed = false;
    if (index >= 0 && index < state->selection.count) {
        changed = state->selection.select_only(index);
    } else {
        changed = state->selection.clear();
        state->selection.focused = -1;
    }
    if (changed) {
        state->changed = true;
    }
    state->focus_row_id = state->selection.focused >= 0
        ? row_id_for(ui, id, state->selection.focused) : 0;
    return changed;
}

int list_selected_count(const char* label) {
    return list_selected_count(widget_ui().make_id_absolute(label));
}

int list_selected_count(flux::ui::ui_id id) {
    const list_state* state = find_list(widget_ui(), id);
    return state != nullptr ? state->selection.selected_count : 0;
}

bool list_selection_changed(const char* label) {
    return list_selection_changed(widget_ui().make_id_absolute(label));
}

bool list_selection_changed(flux::ui::ui_id id) {
    const list_state* state = find_list(widget_ui(), id);
    return state != nullptr && state->changed;
}

bool list_clear_selection(const char* label) {
    return list_clear_selection(widget_ui().make_id_absolute(label));
}

bool list_clear_selection(flux::ui::ui_id id) {
    list_state* state = find_list(widget_ui(), id);
    if (state == nullptr) {
        return false;
    }
    const bool changed = state->selection.clear();
    if (changed) {
        state->changed = true;
    }
    return changed;
}

bool list_select_all(const char* label) {
    return list_select_all(widget_ui().make_id_absolute(label));
}

bool list_select_all(flux::ui::ui_id id) {
    list_state* state = find_list(widget_ui(), id);
    if (state == nullptr) {
        return false;
    }
    const bool changed = state->selection.select_all();
    if (changed) {
        state->changed = true;
    }
    return changed;
}

int list_focused(const char* label) {
    return list_focused(widget_ui().make_id_absolute(label));
}

int list_focused(flux::ui::ui_id id) {
    const list_state* state = find_list(widget_ui(), id);
    return state != nullptr ? state->selection.focused : -1;
}

bool list_scroll_to(const char* label, int index, list_scroll_align align) {
    return list_scroll_to(widget_ui().make_id_absolute(label), index, align);
}

bool list_scroll_to(flux::ui::ui_id id, int index, list_scroll_align align) {
    list_state* state = find_list(widget_ui(), id);
    if (state == nullptr || state->selection.count <= 0) {
        return false;
    }
    state->pending_scroll_index = std::clamp(
        index, 0, state->selection.count - 1);
    state->pending_scroll_align = align;
    return true;
}

}
