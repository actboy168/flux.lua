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

bool button(const char* label, float width, float height, bool enabled, bool primary,
            flux::ui::ui_id id) {
    flux::ui::ui_context& context = widget_ui();
    bool hovered = false;
    bool active = false;
    bool clicked = false;
    bool double_clicked = false;
    bool focused = false;
    context.widget(label, width, height, enabled, hovered, active, clicked, double_clicked, focused,
                   false, id);
    if (!context.is_item_visible()) {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    context.last_item_rect(x, y, w, h);
    const flux::ui::style_sheet& style = context.style();
    control_anim_targets targets;
    targets.hovered = hovered;
    targets.active = active;
    targets.focused = focused;
    const control_anim_state& anim =
        control_anim(context.last_item_id(), targets, context.frame_index());
    const float hover = anim.hover;
    const float press = anim.press;
    const float focus = anim.focus;

    uint32_t fill = style.panel;
    if (enabled) {
        const uint32_t base = primary ? style.accent : style.panel;
        const uint32_t hover_color = primary ? style.accent_hover : style.panel_hover;
        const uint32_t press_color = primary ? style.accent_active : style.panel_active;
        fill = mix_color(base, hover_color, hover);
        fill = mix_color(fill, press_color, press);
    }

    if (primary && enabled) {
        const float elevation = hover * (1.0f - press * 0.5f);
        if (elevation > 0.004f) {
            flux::runtime::services().draw.rect_filled(
                x, y + 2.0f, w, h, with_alpha(0x00000000, 0.20f * elevation),
                style.rounding);
        }
    }

    const float inset = press * 1.0f;
    flux::runtime::services().draw.rect_filled(
        x + inset, y + inset, std::max(0.0f, w - inset * 2.0f),
        std::max(0.0f, h - inset * 2.0f), fill, style.rounding);
    if (!primary) {
        flux::runtime::services().draw.rect(
            x, y, w, h, style.border, style.rounding, style.border_size);
    }
    const float elevation = enabled
        ? (0.55f + 0.45f * hover) * (1.0f - 0.6f * press)
        : 0.25f;
    draw_bottom_elevation(x, y, w, h, style.rounding,
                          primary ? style.white : style.border, elevation);
    draw_focus_ring(x, y, w, h, style.rounding, style.accent, focus);

    uint32_t text_color = enabled ? (primary ? style.white : style.text) : style.disabled;
    const char* display = label != nullptr ? label : "";
    draw_text_block(
        x + style.frame_padding_x,
        y,
        std::max(0.0f, w - style.frame_padding_x * 2.0f),
        h,
        display,
        text_color,
        style.font_size,
        1,
        1,
        false,
        true,
        0,
        style.line_height);
    return clicked && enabled;
}
bool checkbox(const char* label, bool& checked, float width, float height, bool enabled,
              flux::ui::ui_id id) {
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    const char* display = label != nullptr ? label : "";
    float label_width = 0.0f;
    float label_height = 0.0f;
    if (width <= 0.0f || height <= 0.0f) {
        flux::runtime::services().measure_text(display, style.font_size, label_width, label_height);
    }

    float box = 18.0f;
    if (height > 0.0f) {
        box = std::clamp(height - 8.0f, 12.0f, 18.0f);
    }
    if (width <= 0.0f) {
        width = box + 8.0f + label_width + style.frame_padding_x * 2.0f;
    }
    if (height <= 0.0f) {
        height = std::max(box + style.frame_padding_y * 2.0f, label_height + style.frame_padding_y * 2.0f);
    }

    bool hovered = false;
    bool active = false;
    bool clicked = false;
    bool double_clicked = false;
    bool focused = false;
    context.widget(label, width, height, enabled, hovered, active, clicked, double_clicked, focused,
                   false, id);
    if (!context.is_item_visible()) {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    context.last_item_rect(x, y, w, h);
    if (clicked && focused && context.input.key_pressed[flux::ui::keys::enter]
        && !context.input.key_pressed[flux::ui::keys::space]) {
        clicked = false;
    }
    const float box_y = y + (h - box) * 0.5f;
    const float box_x = x + style.frame_padding_x;
    control_anim_targets targets;
    targets.hovered = hovered;
    targets.active = active;
    targets.focused = focused;
    targets.has_value = true;
    targets.value = checked;
    const control_anim_state& anim =
        control_anim(context.last_item_id(), targets, context.frame_index());
    const float value = anim.value;

    uint32_t fill = style.panel;
    if (enabled) {
        fill = mix_color(style.panel, style.accent, value);
        fill = mix_color(fill, mix_color(style.panel_hover, style.accent_hover, value), anim.hover);
        fill = mix_color(fill, mix_color(style.panel_active, style.accent_active, value), anim.press);
    }
    const float box_scale = 1.0f - 0.06f * anim.press + 0.02f * anim.hover;
    const float drawn_box = box * box_scale;
    const float drawn_x = box_x + (box - drawn_box) * 0.5f;
    const float drawn_y = box_y + (box - drawn_box) * 0.5f;
    flux::runtime::services().draw.rect_filled(drawn_x, drawn_y, drawn_box, drawn_box, fill,
                            style.rounding * 0.5f * box_scale);
    flux::runtime::services().draw.rect(
        drawn_x, drawn_y, drawn_box, drawn_box,
        mix_color(style.border, style.accent, anim.focus),
        style.rounding * 0.5f * box_scale, style.border_size + anim.focus);
    if (value > 0.002f) {
        const float glyph_size = std::min(14.0f, drawn_box) * (0.75f + 0.25f * value);
        draw_text_block(
            drawn_x, drawn_y, drawn_box, drawn_box, "✓",
            with_alpha(enabled ? style.white : style.disabled, value),
            glyph_size, 1, 1, false, false, 0, 0.0f);
    }
    draw_focus_ring(x, y, w, h, style.rounding, style.accent, anim.focus);

    draw_text_block(
        box_x + box + 8.0f,
        y,
        std::max(0.0f, x + w - (box_x + box + 8.0f) - style.frame_padding_x),
        h,
        display,
        enabled ? style.text : style.disabled,
        style.font_size,
        0,
        1,
        false,
        true,
        0,
        style.line_height);
    const bool changed = clicked && enabled;
    if (changed) {
        checked = !checked;
    }
    return changed;
}
bool selectable(const char* label, bool selected, float width, float height, bool enabled,
                flux::ui::ui_id id) {
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    const char* display = label != nullptr ? label : "";
    float label_width = 0.0f;
    float label_height = 0.0f;
    if (width <= 0.0f || height <= 0.0f) {
        flux::runtime::services().measure_text(display, style.font_size, label_width, label_height);
    }

    if (width <= 0.0f) {
        width = label_width + style.frame_padding_x * 2.0f;
    }
    if (height <= 0.0f) {
        height = std::max(24.0f, label_height + style.frame_padding_y * 2.0f);
    }

    bool hovered = false;
    bool active = false;
    bool clicked = false;
    bool double_clicked = false;
    bool focused = false;
    context.widget(label, width, height, enabled, hovered, active, clicked, double_clicked, focused,
                   false, id);
    if (!context.is_item_visible()) {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    context.last_item_rect(x, y, w, h);
    control_anim_targets targets;
    targets.hovered = hovered;
    targets.active = active;
    targets.focused = focused;
    targets.has_value = true;
    targets.value = selected;
    const control_anim_state& anim =
        control_anim(context.last_item_id(), targets, context.frame_index());
    const float intensity = std::max(std::max(anim.hover, anim.press), anim.value);

    uint32_t fill = style.panel;
    if (enabled) {
        fill = mix_color(fill, style.panel_hover, anim.hover);
        fill = mix_color(fill, style.panel_active, std::max(anim.press, anim.value));
    }
    const float press_scale = 1.0f - 0.01f * anim.press;
    const float fill_w = w * press_scale;
    const float fill_h = h * press_scale;
    flux::runtime::services().draw.rect_filled(x + (w - fill_w) * 0.5f, y + (h - fill_h) * 0.5f,
                            fill_w, fill_h, fill, style.rounding * press_scale);
    const float border_intensity = std::max(intensity, anim.focus);
    if (border_intensity > 0.002f) {
        uint32_t border = mix_color(style.border, style.accent, anim.value);
        border = mix_color(border, style.accent, anim.focus);
        flux::runtime::services().draw.rect(
            x, y, w, h, scale_alpha(border, border_intensity),
            style.rounding, style.border_size);
    }
    draw_focus_ring(x, y, w, h, style.rounding, style.accent, anim.focus);

    draw_text_block(
        x + style.frame_padding_x,
        y,
        std::max(0.0f, w - style.frame_padding_x * 2.0f),
        h,
        display,
        enabled ? style.text : style.disabled,
        style.font_size,
        0,
        1,
        false,
        true,
        0,
        style.line_height);
    return clicked && enabled;
}
void label(const char* text, uint32_t color, float size, float width, bool wrap,
           int max_lines) {
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    const uint32_t text_color = color == 0 ? style.text : color;
    const float font_size = size > 0.0f ? size : style.font_size;

    flux::text::text_options options;
    options.width = width;
    options.wrap = wrap;
    options.ellipsis = true;
    options.max_lines = max_lines;
    options.line_height = style.line_height;

    float measured_width = 0.0f;
    float measured_height = 0.0f;
    flux::runtime::services().measure_text(text != nullptr ? text : "", font_size, options, measured_width, measured_height);

    const float x = context.cursor_x();
    const float y = context.cursor_y();
    flux::runtime::services().draw.text_options(
        x, y, text != nullptr ? text : "", text_color, font_size,
        options.width, options.height, options.line_height, options.max_lines,
        options.align, options.valign, options.weight, options.wrap, options.ellipsis);
    const float advance_width = options.width > 0.0f ? options.width : measured_width;
    context.dummy(advance_width, measured_height);
}
void progress_bar(const char* label, float fraction, float width, float height,
                  bool indeterminate, const progress_bar_options& options) {
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    if (width <= 0.0f) {
        return;
    }
    if (height <= 0.0f) {
        height = 6.0f;
    }
    const float x = context.cursor_x();
    const float y = context.cursor_y();
    flux::ui::ui_id progress_id = options.id;
    if (progress_id == 0) {
        if (label != nullptr && label[0] != 0) {
            progress_id = context.make_id(label);
        } else {
            const std::string key = std::format("progress@{:.1f},{:.1f}", x, y);
            progress_id = context.make_id(key.c_str());
        }
    }
    flux::runtime::services().draw.rect_filled(x, y, width, height, style.track, height * 0.5f);

    if (indeterminate) {
        const float bar_width = width * 0.3f;
        const float phase = std::fmod(context.time() * 0.8f, 1.0f);
        const float left = x + (width + bar_width) * phase - bar_width;
        const float start = std::max(x, left);
        const float end = std::min(x + width, left + bar_width);
        if (end > start) {
            flux::runtime::services().draw.rect_filled(
                start, y, end - start, height, style.accent, height * 0.5f);
        }
    } else {
        const float clamped = std::clamp(fraction, 0.0f, 1.0f);
        progress_anim& anim =
            wr().states.get<progress_anim>(progress_id, context.frame_index());
        if (!anim.initialized) {
            anim.value = clamped;
            anim.initialized = true;
        } else {
            anim.value = control_anim_step(
                anim.value, clamped, context.delta_time(), flux::ui::motion::normal, context.frame_index());
        }
        if (anim.value > 0.0f) {
            flux::runtime::services().draw.rect_filled(
                x, y, width * anim.value, height, style.accent, height * 0.5f);
        }
    }

    context.dummy(width, height);
}
void separator(float width) {
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    const float x = context.cursor_x();
    const float y = context.cursor_y();
    flux::runtime::services().draw.rect_filled(x, y, width, std::max(1.0f, style.border_size), style.border, 0.0f);
    context.dummy(width, 1.0f);
}
void spacing(float height) {
    flux::ui::ui_context& context = widget_ui();
    context.dummy(0.0f, height > 0.0f ? height : context.style().item_spacing_y);
}
}
