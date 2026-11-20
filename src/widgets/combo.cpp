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

int combo_first_enabled(const uint8_t* disabled, int count, int from, int direction) {
    if (count <= 0) {
        return -1;
    }
    int index = from;
    for (int steps = 0; steps < count; ++steps) {
        if (index < 0) {
            index = direction > 0 ? 0 : count - 1;
        } else if (index >= count) {
            index = direction > 0 ? 0 : count - 1;
        }
        if (disabled == nullptr || disabled[index] == 0) {
            return index;
        }
        index += direction;
    }
    return -1;
}
bool starts_with_ci(const char* text, std::string_view prefix) {
    if (prefix.empty()) {
        return true;
    }
    if (text == nullptr) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        const char value = text[i];
        if (value == '\0') {
            return false;
        }
        unsigned char a = static_cast<unsigned char>(value);
        unsigned char b = static_cast<unsigned char>(prefix[i]);
        if (a >= 'A' && a <= 'Z') {
            a = static_cast<unsigned char>(a + ('a' - 'A'));
        }
        if (b >= 'A' && b <= 'Z') {
            b = static_cast<unsigned char>(b + ('a' - 'A'));
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}
combo_result combo(const char* label, int selected, const char* const* items,
                  const uint8_t* disabled, int count, float width, float height, bool enabled,
                  flux::ui::ui_id id) {
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    flux::ui::draw_list& draw = flux::runtime::services().draw;
    const flux::ui::input_state& input = context.input;
    const flux::ui::ui_id resolved_id = id != 0 ? id : context.make_id(label);

    combo_result result;
    if (count <= 0 || items == nullptr) {
        selected = -1;
    } else if (selected >= 0) {
        selected = std::clamp(selected, 0, count - 1);
    }
    result.selected = selected;

    combo_state& state = wr().states.get<combo_state>(resolved_id, context.frame_index());

    float max_item_width = 0.0f;
    for (int i = 0; i < count; ++i) {
        float text_width = 0.0f;
        float text_height = 0.0f;
        flux::runtime::services().measure_text(
            items[i] != nullptr ? items[i] : "",
            style.font_size, text_width, text_height);
        max_item_width = std::max(max_item_width, text_width);
    }
    if (width <= 0.0f) {
        width = max_item_width + style.frame_padding_x * 2.0f + 28.0f;
    }
    width = std::max(width, 72.0f);
    if (height <= 0.0f) {
        height = 30.0f;
    }

    bool hovered = false;
    bool active = false;
    bool clicked = false;
    bool double_clicked = false;
    bool focused = false;
    context.widget(label, width, height, enabled, hovered, active, clicked,
                   double_clicked, focused, state.open, resolved_id);
    if (!context.is_item_visible()) {
        if (state.open) {
            state.open = false;
        }
        return result;
    }

    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    context.last_item_rect(x, y, w, h);

    if (!enabled || !focused || input.mouse_pressed[0] || input.key_pressed[flux::ui::keys::escape]) {
        state.search.clear();
    }

    if (enabled && focused && count > 0 && !input.ime_composing && !input.text.empty()) {
        const double now = context.time();
        const bool fresh = state.search.empty() || now - state.search_time > 1.0;
        if (fresh) {
            state.search.clear();
        }
        for (unsigned char value : input.text) {
            if (value >= 0x20 && value != 0x7F) {
                state.search.push_back(static_cast<char>(value));
            }
        }
        if (state.search.size() > 256) {
            state.search.erase(0, state.search.size() - 256);
        }
        state.search_time = now;
        if (!state.search.empty()) {
            int start = fresh
                ? (selected >= 0 ? selected + 1 : 0)
                : (selected >= 0 ? selected : 0);
            if (start >= count) {
                start = 0;
            }
            for (int step = 0; step < count; ++step) {
                const int index = (start + step) % count;
                if (disabled != nullptr && disabled[index] != 0) {
                    continue;
                }
                const char* item_text = items[index] != nullptr ? items[index] : "";
                if (starts_with_ci(item_text, state.search)) {
                    if (state.open) {
                        state.highlighted = index;
                    } else {
                        if (selected != index) {
                            selected = index;
                            result.changed = true;
                        }
                        state.highlighted = index;
                    }
                    break;
                }
            }
        }
    }

    const bool key_confirm = input.key_pressed[flux::ui::keys::enter] || input.key_pressed[flux::ui::keys::space];
    if (state.open && input.mouse_pressed[0]) {
        const bool inside_button = input.mouse_x >= x && input.mouse_x < x + w
            && input.mouse_y >= y && input.mouse_y < y + h;
        const bool inside_popup = input.mouse_x >= state.popup_x
            && input.mouse_x < state.popup_x + state.popup_w
            && input.mouse_y >= state.popup_y
            && input.mouse_y < state.popup_y + state.popup_h;
        if (!inside_button && !inside_popup) {
            state.open = false;
        }
    }

    bool just_opened = false;
    if (enabled && clicked && !(state.open && key_confirm)) {
        state.open = !state.open;
        if (state.open) {
            state.highlighted = combo_first_enabled(
                disabled, count, selected < 0 ? 0 : selected, 1);
            just_opened = true;
        }
    }

    if (state.open && !enabled) {
        state.open = false;
    }
    if (state.open && count <= 0) {
        state.open = false;
    }

    if (state.open && !just_opened) {
        if (input.key_pressed[flux::ui::keys::escape]) {
            state.open = false;
        }
        if (input.key_pressed[flux::ui::keys::up]) {
            state.highlighted = combo_first_enabled(
                disabled, count, (state.highlighted < 0 ? selected : state.highlighted) - 1, -1);
        }
        if (input.key_pressed[flux::ui::keys::down]) {
            state.highlighted = combo_first_enabled(
                disabled, count, (state.highlighted < 0 ? selected : state.highlighted) + 1, 1);
        }
        if (input.key_pressed[flux::ui::keys::home]) {
            state.highlighted = combo_first_enabled(disabled, count, 0, 1);
        }
        if (input.key_pressed[flux::ui::keys::end]) {
            state.highlighted = combo_first_enabled(disabled, count, count - 1, -1);
        }
        const bool ctrl_enter =
            input.key_pressed[flux::ui::keys::enter] && input.key_ctrl[flux::ui::keys::enter];
        if (key_confirm && state.highlighted >= 0
            && (disabled == nullptr || disabled[state.highlighted] == 0)) {
            if (ctrl_enter && selected == state.highlighted) {
                selected = -1;
                result.changed = true;
            } else if (selected != state.highlighted) {
                selected = state.highlighted;
                result.changed = true;
            }
            state.open = false;
        }
    }

    result.selected = selected;
    result.open = state.open;
    if (!state.open && wr().combo_open_id == resolved_id) {
        wr().combo_open_id = 0;
        context.close_popup();
    }

    if (state.open) {
        wr().combo_open_id = resolved_id;
    }

    const std::string selected_text = selected >= 0 && selected < count
        ? (items[selected] != nullptr ? items[selected] : "")
        : std::string();

    control_anim_targets trigger_targets;
    trigger_targets.hovered = hovered;
    trigger_targets.active = active;
    trigger_targets.focused = focused;
    const control_anim_state& trigger_anim =
        control_anim(resolved_id, trigger_targets, context.frame_index());
    uint32_t fill = style.panel;
    if (enabled) {
        fill = mix_color(fill, style.panel_hover, trigger_anim.hover);
        fill = mix_color(fill, style.panel_active, trigger_anim.press);
        if (state.open) {
            fill = style.panel_active;
        }
    }
    const float edge = state.open ? 1.0f : trigger_anim.focus;
    draw.rect_filled(x, y, w, h, fill, style.rounding);
    draw.rect(x, y, w, h, mix_color(style.border, style.accent, edge),
              style.rounding, style.border_size + (2.0f - style.border_size) * edge);
    if (context.focus_visible() && trigger_anim.focus > 0.002f) {
        draw.rect(x - 2.0f, y - 2.0f, w + 4.0f, h + 4.0f,
                  with_alpha(style.accent, trigger_anim.focus),
                  style.rounding + 2.0f, 1.0f);
    }

    draw_text_block(
        x + style.frame_padding_x, y,
        std::max(0.0f, w - style.frame_padding_x * 2.0f - 24.0f),
        h, selected_text.c_str(),
        enabled && !selected_text.empty() ? style.text : style.disabled,
        style.font_size, 0, 1, false, true, 0, style.line_height);
    draw_text_block(
        x + w - 22.0f, y, 16.0f, h, "\xE2\x96\xBE",
        enabled ? style.text_dim : style.disabled, style.font_size - 1.0f,
        1, 1, false, false, 0, style.line_height);

    if (state.open) {
        state.reveal = control_anim_step(
            state.reveal, 1.0f, context.delta_time(), flux::ui::motion::fast, context.frame_index());
    } else {
        state.reveal = 0.0f;
    }

    if (state.open) {
        const float item_height = style.combo_item_height;
        const float popup_padding = style.combo_popup_padding;
        const float scrollbar = style.scrollbar_size;
        float popup_w = std::max(w, max_item_width + popup_padding * 2.0f + scrollbar + 40.0f);
        float popup_h = std::min(static_cast<float>(count) * item_height + popup_padding * 2.0f,
                                 style.combo_popup_max_height);
        const float window_w = static_cast<float>(flux::runtime::services().width);
        const float window_h = static_cast<float>(flux::runtime::services().height);
        if (window_w > 0.0f) {
            popup_w = std::min(popup_w, std::max(style.combo_popup_min_width, window_w - 8.0f));
        }
        if (window_h > 0.0f) {
            popup_h = std::min(popup_h, std::max(20.0f, window_h - 8.0f));
        }
        popup_anchor anchor;
        anchor.x = x;
        anchor.y = y;
        anchor.w = w;
        anchor.h = h;
        popup_place_options place_options;
        place_options.gap = style.combo_popup_gap;
        popup_rect placed = place_popup(anchor, popup_w, popup_h, place_options,
                                        window_w, window_h);
        const float popup_x = placed.x;
        const float popup_y = placed.y;
        popup_w = placed.w;
        popup_h = placed.h;
        state.popup_x = popup_x;
        state.popup_y = popup_y;
        state.popup_w = popup_w;
        state.popup_h = popup_h;

        const std::string popup_key = std::format("combo_popup_{:016x}", resolved_id);
        const flux::ui::ui_id popup_id = context.make_id(popup_key.c_str());
        const flux::ui::ui_id combo_list_id = context.make_id_absolute("combo_list");
        if (!context.popup_is_open(popup_id)) {
            context.open_popup(popup_id, true);
        }
        if (begin_popup("选项", popup_x, popup_y, popup_w, popup_h,
                        ease_out_cubic(state.reveal), popup_id, true)) {
            const float list_w = std::max(20.0f, popup_w - popup_padding * 2.0f);
            const float list_h = std::max(20.0f, popup_h - popup_padding * 2.0f);
            context.set_cursor_pos(popup_x + popup_padding, popup_y + popup_padding);
            context.begin_child("选项列表", list_w, list_h, combo_list_id);
            const float item_w = std::max(20.0f, list_w - scrollbar);
            context.push_style_var(flux::ui::style_var_item_spacing_y, 0.0f);
            context.begin_column();
            bool selected_by_mouse = false;
            for (int i = 0; i < count; ++i) {
                const float item_x = context.cursor_x();
                const float item_y = context.cursor_y();
                const bool item_disabled = disabled != nullptr && disabled[i] != 0;
                const bool visible = context.is_rect_visible(item_x, item_y, item_w, item_height);
                const bool item_hovered = visible && input.mouse_inside
                    && input.mouse_x >= item_x && input.mouse_x < item_x + item_w
                    && input.mouse_y >= item_y && input.mouse_y < item_y + item_height;
                const std::string item_label = "combo_item_" + flux::text::int_to_string(i);
                const flux::ui::ui_id item_id = context.make_id(item_label.c_str());
                if (item_hovered && !item_disabled) {
                    state.highlighted = i;
                }
                if (!item_disabled && item_hovered && input.mouse_pressed[0]) {
                    if (selected != i) {
                        selected = i;
                        result.changed = true;
                    }
                    result.selected = selected;
                    selected_by_mouse = true;
                }
                if (visible) {
                    const bool is_selected = selected == i;
                    control_anim_targets item_targets;
                    item_targets.hovered = !item_disabled
                        && (item_hovered || state.highlighted == i);
                    item_targets.has_value = true;
                    item_targets.value = is_selected;
                    const control_anim_state& item_anim =
                        control_anim(item_id, item_targets, context.frame_index());
                    const float item_intensity = std::max(item_anim.hover, item_anim.value);
                    if (item_intensity > 0.002f) {
                        draw.rect_filled(
                            item_x, item_y, item_w, item_height,
                            scale_alpha(style.panel_hover, item_intensity),
                            std::max(0.0f, style.rounding - 2.0f));
                    }
                    float text_x = item_x + 10.0f;
                    if (is_selected) {
                        if (item_anim.value > 0.002f) {
                            draw.rect_filled(
                                item_x + 8.0f, item_y + item_height * 0.5f - 4.0f,
                                8.0f, 8.0f, with_alpha(style.accent, item_anim.value), 4.0f);
                        }
                        text_x = item_x + 24.0f;
                    }
                    draw_text_block(
                        text_x, item_y, std::max(0.0f, item_w - (text_x - item_x) - 8.0f),
                        item_height, items[i] != nullptr ? items[i] : "",
                        item_disabled ? style.disabled : style.text,
                        style.font_size, 0, 1, false, true, 0, style.line_height);
                }
                context.dummy(item_w, item_height);
            }
            context.end_column();
            context.pop_style_var(1);
            context.end_child();
            end_popup();
            if (selected_by_mouse) {
                state.open = false;
                result.open = false;
            }
        } else {
            state.open = false;
            result.open = false;
        }
    }

    return result;
}
bool combo_is_open(flux::ui::ui_id id) {
    const combo_state* state = wr().states.find<combo_state>(id);
    return state != nullptr && state->open;
}

void combo_set_open(flux::ui::ui_id id, bool open) {
    if (!open) {
        combo_state* state = wr().states.find<combo_state>(id);
        if (state != nullptr) {
            state->open = false;
        }
        if (wr().combo_open_id == id) {
            wr().combo_open_id = 0;
            widget_ui().close_popup();
        }
        return;
    }
    combo_state& state = wr().states.get<combo_state>(id, widget_ui().frame_index());
    state.open = true;
    if (state.highlighted < 0) {
        state.highlighted = 0;
    }
}


float combo_reveal(const char* label, flux::ui::ui_id id) {
    const flux::ui::ui_id resolved_id = id != 0 ? id : widget_ui().make_id(label);
    const combo_state* state = wr().states.find<combo_state>(resolved_id);
    return state != nullptr ? state->reveal : 0.0f;
}
}
