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

std::string tab_item_key(const tab_item_desc& item, int index) {
    if (item.id != nullptr && item.id[0] != 0) {
        return item.id;
    }
    if (item.label != nullptr && item.label[0] != 0) {
        return item.label;
    }
    return "tab" + flux::text::int_to_string(index);
}
void draw_tab_shape(flux::ui::draw_list& draw, float x, float y, float w, float h,
                    uint32_t color, float rounding) {
    if (w <= 0.0f || h <= 0.0f || (color & 0xFF000000u) == 0) {
        return;
    }
    const float radius = std::min(rounding, std::min(w * 0.5f, h));
    draw.rect_filled(x, y, w, h, color, radius);
    if (radius > 0.0f) {
        draw.rect_filled(x, y + h - radius, w, radius, color, 0.0f);
    }
}
tab_bar_result tab_bar(const char* label, const tab_item_desc* items, int count,
                       const tab_bar_options& options) {
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    tab_bar_result result;
    result.selected = options.selected;
    if (count <= 0 || items == nullptr) {
        return result;
    }

    const float x = context.cursor_x();
    const float y = context.cursor_y();
    const float height = std::max(
        1.0f, options.height > 0.0f ? options.height : style.tab_height);
    const float padding = std::max(
        0.0f, options.padding_x >= 0.0f ? options.padding_x : style.tab_padding_x);
    const float gap = std::max(0.0f, options.gap >= 0.0f ? options.gap : style.tab_gap);
    const float min_width = std::max(
        0.0f, options.min_width >= 0.0f ? options.min_width : style.tab_min_width);
    const float rounding = std::max(
        0.0f, options.rounding >= 0.0f ? options.rounding : style.tab_rounding);
    const float font_size = style.tab_font_size > 0.0f ? style.tab_font_size : style.font_size;
    const float baseline_size = std::max(0.0f, style.tab_baseline_size);

    std::vector<const char*> displays(static_cast<size_t>(count));
    std::vector<std::string> labels(static_cast<size_t>(count));
    std::vector<float> widths(static_cast<size_t>(count));
    float natural_total = 0.0f;
    for (int i = 0; i < count; ++i) {
        const size_t index = static_cast<size_t>(i);
        const char* raw = items[i].label != nullptr ? items[i].label : "";
        displays[index] = raw;
        labels[index] = tab_item_key(items[i], i);
        float text_width = 0.0f;
        float text_height = 0.0f;
        flux::runtime::services().measure_text(displays[index], font_size, text_width, text_height);
        widths[index] = items[i].width > 0.0f
            ? items[i].width
            : std::max(min_width, text_width + padding * 2.0f);
        natural_total += widths[index];
        if (i + 1 < count) {
            natural_total += gap;
        }
    }

    const float bar_width = options.width > 0.0f ? options.width : natural_total;
    if (options.equal_width && options.width > 0.0f) {
        const float slot = std::max(1.0f,
            (options.width - gap * static_cast<float>(count - 1)) / static_cast<float>(count));
        for (float& width : widths) {
            width = slot;
        }
    } else if (options.width > 0.0f) {
        float fixed_width = 0.0f;
        float flexible_width = 0.0f;
        int flexible_count = 0;
        for (int i = 0; i < count; ++i) {
            if (items[i].width > 0.0f) {
                fixed_width += widths[static_cast<size_t>(i)];
            } else {
                flexible_width += widths[static_cast<size_t>(i)];
                ++flexible_count;
            }
        }
        const float available = std::max(0.0f,
            options.width - fixed_width - gap * static_cast<float>(count - 1));
        if (flexible_count > 0 && flexible_width > available) {
            if (available <= min_width * static_cast<float>(flexible_count)) {
                for (int i = 0; i < count; ++i) {
                    if (items[i].width <= 0.0f) {
                        widths[static_cast<size_t>(i)] = min_width;
                    }
                }
            } else {
                const float scale = available / flexible_width;
                for (int i = 0; i < count; ++i) {
                    if (items[i].width <= 0.0f) {
                        const size_t index = static_cast<size_t>(i);
                        widths[index] = std::max(min_width, widths[index] * scale);
                    }
                }
            }
        }
    }

    if (!context.is_rect_visible(x, y, bar_width, height)) {
        context.set_cursor_pos(x, y);
        context.dummy(bar_width, height);
        return result;
    }

    const flux::ui::ui_id bar_id = options.id != 0 ? options.id : context.make_id(label);
    context.push_id(bar_id);
    std::vector<flux::ui::ui_id> ids(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        ids[static_cast<size_t>(i)] = context.make_id(labels[static_cast<size_t>(i)].c_str());
    }

    const float bar_right = x + bar_width;
    auto tab_x_at = [&](int index) {
        float tab_x = x;
        for (int i = 0; i < index; ++i) {
            tab_x += widths[static_cast<size_t>(i)] + gap;
        }
        return tab_x;
    };
    auto tab_visible = [&](int index) {
        const float tab_x = tab_x_at(index);
        return tab_x + widths[static_cast<size_t>(index)] > x && tab_x < bar_right;
    };
    auto next_enabled = [&](int from, int direction) {
        for (int i = from + direction; i >= 0 && i < count; i += direction) {
            if (!items[i].disabled && tab_visible(i)) {
                return i;
            }
        }
        return -1;
    };

    int focused_index = -1;
    const flux::ui::ui_id focus_id = context.keyboard_focus_id();
    for (int i = 0; i < count; ++i) {
        if (ids[static_cast<size_t>(i)] == focus_id) {
            focused_index = i;
            break;
        }
    }
    const bool keyboard_allowed = !context.modal() || context.popup_active();
    if (keyboard_allowed && focused_index >= 0) {
        int target = -1;
        if (context.input.key_pressed[flux::ui::keys::left]) {
            target = next_enabled(focused_index, -1);
        } else if (context.input.key_pressed[flux::ui::keys::right]) {
            target = next_enabled(focused_index, 1);
        } else if (context.input.key_pressed[flux::ui::keys::home]) {
            for (int i = 0; i < count; ++i) {
                if (!items[i].disabled && tab_visible(i)) {
                    target = i;
                    break;
                }
            }
        } else if (context.input.key_pressed[flux::ui::keys::end]) {
            for (int i = count - 1; i >= 0; --i) {
                if (!items[i].disabled && tab_visible(i)) {
                    target = i;
                    break;
                }
            }
        }
        if (target >= 0 && target != focused_index) {
            result.selected = target + 1;
            context.set_keyboard_focus_id(ids[static_cast<size_t>(target)]);
            context.set_focus_visible(true);
            focused_index = target;
        }
    }
    result.focused = focused_index >= 0 ? focused_index + 1 : 0;

    int roving_index = -1;
    if (result.selected >= 1 && result.selected <= count) {
        const int index = result.selected - 1;
        if (!items[index].disabled && tab_visible(index)) {
            roving_index = index;
        }
    }
    if (roving_index < 0) {
        for (int i = 0; i < count; ++i) {
            if (!items[i].disabled && tab_visible(i)) {
                roving_index = i;
                break;
            }
        }
    }

    context.push_clip(x, y, bar_width, height);
    flux::runtime::services().draw.push_clip(x, y, bar_width, height);

    if ((style.tab_bar_bg & 0xFF000000u) != 0) {
        flux::runtime::services().draw.rect_filled(x, y, bar_width, height, style.tab_bar_bg, rounding);
    }
    if ((style.tab_bar_border & 0xFF000000u) != 0) {
        flux::runtime::services().draw.rect(x, y, bar_width, height, style.tab_bar_border, rounding,
                         style.border_size);
    }

    if (options.draw_baseline && baseline_size > 0.0f) {
        flux::runtime::services().draw.rect_filled(x, y + height - baseline_size, bar_width, baseline_size,
                                style.tab_baseline, 0.0f);
    }

    for (int i = 0; i < count; ++i) {
        const size_t index = static_cast<size_t>(i);
        const float tab_x = tab_x_at(i);
        const float tab_width = widths[index];
        const bool visible = tab_visible(i);
        bool hovered = false;
        bool active = false;
        bool clicked = false;
        bool double_clicked = false;
        bool focused = false;
        if (visible) {
            flux::ui::ui_context::widget_input_options input_options;
            input_options.interactive = !items[i].disabled;
            input_options.focusable = !items[i].disabled;
            input_options.tab_stop = !options.roving || i == roving_index;
            context.set_cursor_pos(tab_x, y);
            context.widget_ex(labels[index].c_str(), tab_width, height, input_options,
                              hovered, active, clicked, double_clicked, focused);
            if (focused) {
                result.focused = i + 1;
            }
            if (clicked) {
                result.clicked = i + 1;
                result.selected = i + 1;
            }
        }

        if (!visible) {
            continue;
        }

        const bool is_selected = result.selected == i + 1;
        control_anim_targets targets;
        targets.hovered = hovered;
        targets.active = active;
        targets.focused = focused;
        targets.has_value = true;
        targets.value = is_selected;
        const control_anim_state& anim =
            control_anim(ids[index], targets, context.frame_index());

        const uint32_t base = mix_color(style.tab_bg, style.tab_bg_hover, anim.hover);
        const uint32_t fill = mix_color(base, style.tab_bg_selected, anim.value);
        const float press_offset = anim.press * 0.5f;
        const float fill_height =
            std::max(1.0f, height - 2.0f + 2.0f * anim.value - press_offset);
        draw_tab_shape(flux::runtime::services().draw, tab_x, y + press_offset, tab_width, fill_height,
                       fill, rounding);
        draw_focus_ring_inset(tab_x, y, tab_width, height, rounding,
                              style.tab_focus, anim.focus, 2.0f);

        const uint32_t text_color = items[i].disabled
            ? style.disabled
            : mix_color(style.tab_text, style.tab_text_selected, anim.value);
        draw_text_block(
            tab_x + padding, y + anim.press * 0.5f,
            std::max(0.0f, tab_width - padding * 2.0f), height,
            displays[index], text_color, font_size,
            1, 1, false, true, 0, style.line_height);

    }

    flux::runtime::services().draw.pop_clip();
    context.pop_clip();
    context.pop_id();

    result.changed = result.selected != options.selected;
    context.set_cursor_pos(x, y);
    context.dummy(bar_width, height);
    return result;
}
}
