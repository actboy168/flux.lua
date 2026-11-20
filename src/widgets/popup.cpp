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

float menu_measure(const menu_measure_options& options) {
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    const int items = std::max(0, options.items);
    const int separators = std::max(0, options.separators);
    const int text_rows = std::max(0, options.text_rows);
    const int rows = items + separators + text_rows;
    const float spacing = context.item_spacing_y();
    const int gaps = std::max(0, rows - 1);
    return style.popup_padding_y * 2.0f
        + static_cast<float>(text_rows) * options.text_row_height
        + static_cast<float>(items) * style.menu_item_height
        + static_cast<float>(separators) * style.menu_separator_height
        + static_cast<float>(gaps) * spacing
        + options.extra_height;
}
popup_rect place_popup(const popup_anchor& anchor, float width, float height,
                       const popup_place_options& options,
                       float viewport_width, float viewport_height) {
    popup_rect result;
    result.w = std::max(1.0f, width);
    result.h = std::max(1.0f, height);
    const float margin = std::max(0.0f, options.margin);
    const float gap = std::max(0.0f, options.gap);
    if (viewport_width > 0.0f) {
        result.w = std::min(result.w, std::max(1.0f, viewport_width - margin * 2.0f));
    }
    if (viewport_height > 0.0f) {
        result.h = std::min(result.h, std::max(1.0f, viewport_height - margin * 2.0f));
    }

    result.x = anchor.x;
    result.y = anchor.y + anchor.h + gap;
    if (viewport_width > 0.0f && result.x + result.w > viewport_width - margin) {
        result.x = anchor.x + anchor.w - result.w;
    }
    if (viewport_height > 0.0f && result.y + result.h > viewport_height - margin) {
        result.y = anchor.y - result.h - gap;
    }
    if (viewport_width > 0.0f) {
        result.x = std::max(margin,
            std::min(result.x, std::max(margin, viewport_width - result.w - margin)));
    }
    if (viewport_height > 0.0f) {
        result.y = std::max(margin,
            std::min(result.y, std::max(margin, viewport_height - result.h - margin)));
    }
    return result;
}
bool begin_popup(const char* label, float x, float y, float width, float height,
                 float alpha, flux::ui::ui_id id, bool modal) {
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    flux::ui::draw_list& draw = flux::runtime::services().draw;

    const flux::ui::ui_id popup_id = id != 0 ? id : context.make_id(label);
    if (!context.popup_is_open(popup_id)) {
        return false;
    }

    const flux::ui::render_layer previous = draw.active_layer();
    if (!draw.use_layer(flux::ui::render_layer::popup)) {
        return false;
    }
    if (modal) {
        context.set_popup_modal(popup_id, true);
    }
    context.update_popup_rect(popup_id, x, y, width, height);
    float effective_alpha = alpha;
    if (alpha < 0.0f) {
        popup_anim& anim = wr().states.get<popup_anim>(popup_id, context.frame_index());
        if (anim.last_frame == 0 || context.frame_index() - anim.last_frame > 1) {
            anim.reveal = 0.0f;
        }
        anim.reveal = control_anim_step(
            anim.reveal, 1.0f, context.delta_time(), flux::ui::motion::normal, context.frame_index());
        anim.last_frame = context.frame_index();
        effective_alpha = ease_out_cubic(anim.reveal);
    }

    popup_scope frame;
    frame.previous_layer = previous;
    frame.x = x;
    frame.y = y;
    frame.w = width;
    frame.h = height;
    frame.saved_cursor_x = context.cursor_x();
    frame.saved_cursor_y = context.cursor_y();
    frame.saved_line_start_x = context.line_start_x();
    frame.saved_alpha = draw.alpha;
    frame.saved_clips = context.save_clips();
    context.clear_clips();
    wr().popup_scopes.push_back(frame);

    draw.alpha = frame.saved_alpha * std::clamp(effective_alpha, 0.0f, 1.0f);
    draw.shadow_rect(x, y, width, height, style.rounding, style.popup_shadow,
                     style.popup_shadow_spread, style.popup_shadow_offset_y);
    draw.rect_filled(x, y, width, height, style.popup_bg, style.rounding);
    draw.rect(x, y, width, height, style.popup_border, style.rounding, style.border_size);
    context.push_clip(x, y, width, height);
    draw.push_clip(x, y, width, height);
    context.set_cursor_pos(x + style.popup_padding_x, y + style.popup_padding_y);
    context.set_popup_active(true);
    context.push_id(popup_id);
    return true;
}
bool end_popup() {
    if (wr().popup_scopes.empty()) {
        wr().fail("end_popup called without begin_popup");
        return false;
    }
    flux::ui::ui_context& context = widget_ui();
    flux::ui::draw_list& draw = flux::runtime::services().draw;
    const popup_scope frame = wr().popup_scopes.back();
    wr().popup_scopes.pop_back();

    context.pop_id();
    draw.pop_clip();
    context.pop_clip();
    context.restore_clips(std::move(frame.saved_clips));
    context.set_popup_active(!wr().popup_scopes.empty());
    context.set_cursor_pos(frame.saved_cursor_x, frame.saved_cursor_y);
    context.set_line_start_x(frame.saved_line_start_x);
    draw.alpha = frame.saved_alpha;
    draw.use_layer(frame.previous_layer);
    return true;
}
bool menu_item(const char* label, const char* shortcut, bool enabled, bool checked,
               bool checkable, flux::ui::ui_id id) {
    if (wr().popup_scopes.empty()) {
        return false;
    }
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    const popup_scope& frame = wr().popup_scopes.back();
    const float item_width = std::max(0.0f, frame.w - style.popup_padding_x * 2.0f);
    const float item_height = style.menu_item_height;

    bool hovered = false;
    bool active = false;
    bool clicked = false;
    bool double_clicked = false;
    bool focused = false;
    context.widget(label, item_width, item_height, enabled,
                   hovered, active, clicked, double_clicked, focused, false, id);
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
    const control_anim_state& anim =
        control_anim(context.last_item_id(), targets, context.frame_index());
    const float intensity = std::max(anim.hover, anim.press);
    if (intensity > 0.002f) {
        flux::runtime::services().draw.rect_filled(
            x, y, w, h,
            scale_alpha(style.menu_item_hover, intensity),
            std::max(0.0f, style.rounding - 2.0f));
    }
    const float item_pad = style.menu_item_padding_x;
    float text_x = x + item_pad;
    if (checked) {
        flux::runtime::services().draw.rect_filled(
            x + 8.0f, y + h * 0.5f - 4.0f, 8.0f, 8.0f,
            style.accent, 4.0f);
        text_x = x + item_pad + 14.0f;
    }

    const char* display = label != nullptr ? label : "";
    float shortcut_width = 0.0f;
    float shortcut_height = 0.0f;
    if (shortcut != nullptr && shortcut[0] != '\0') {
        flux::runtime::services().measure_text(
            shortcut, style.font_size - 1.0f, shortcut_width, shortcut_height);
    }
    draw_text_block(
        text_x, y,
        std::max(0.0f, w - (text_x - x) - shortcut_width - 16.0f),
        h, display,
        enabled ? style.menu_text : style.disabled, style.font_size,
        0, 1, false, true, 0, style.line_height);
    if (shortcut_width > 0.0f) {
        draw_text_block(
            x + w - shortcut_width - 10.0f, y, shortcut_width, h,
            shortcut, style.menu_shortcut, style.font_size - 1.0f,
            2, 1, false, true, 0, style.line_height);
    }

    return clicked && enabled;
}
bool begin_menu(const char* label, const menu_begin_options& options) {
    if (wr().popup_scopes.empty()) {
        wr().fail("begin_menu called outside a popup");
        return false;
    }
    flux::ui::ui_context& context = widget_ui();
    const bool clicked = menu_item(label, "\xE2\x80\xBA", options.enabled,
                                   false, false, options.id);
    const bool hovered = context.is_item_hovered();
    const flux::ui::ui_id submenu_id = context.make_id(label);
    if (options.enabled && (hovered || clicked)) {
        if (!context.popup_is_open(submenu_id)) {
            context.open_popup(submenu_id, false);
        }
    }
    if (!context.popup_is_open(submenu_id)) {
        return false;
    }

    float row_x = 0.0f;
    float row_y = 0.0f;
    float row_w = 0.0f;
    float row_h = 0.0f;
    context.last_item_rect(row_x, row_y, row_w, row_h);
    const float submenu_w = std::max(20.0f, options.width);
    const float submenu_h = std::max(20.0f, options.height);
    const float viewport_w = static_cast<float>(flux::runtime::services().width);
    const float viewport_h = static_cast<float>(flux::runtime::services().height);
    constexpr float margin = 8.0f;
    constexpr float gap = 4.0f;
    float x = row_x + row_w - gap;
    if (viewport_w > 0.0f && x + submenu_w > viewport_w - margin) {
        x = row_x - submenu_w + gap;
    }
    if (viewport_w > 0.0f) {
        x = std::max(margin, std::min(x, viewport_w - submenu_w - margin));
    }
    float y = row_y;
    if (viewport_h > 0.0f) {
        y = std::max(margin, std::min(y, viewport_h - submenu_h - margin));
    }
    const bool opened = begin_popup(label, x, y, submenu_w, submenu_h,
                                    -1.0f, submenu_id, false);
    wr().menu_scopes.push_back(opened);
    return opened;
}

bool end_menu() {
    if (wr().menu_scopes.empty()) {
        wr().fail("end_menu called without begin_menu");
        return false;
    }
    const bool opened = wr().menu_scopes.back();
    wr().menu_scopes.pop_back();
    if (opened && !end_popup()) {
        return false;
    }
    return true;
}

void menu_separator() {
    if (wr().popup_scopes.empty()) {
        return;
    }
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    const popup_scope& frame = wr().popup_scopes.back();
    const float width = std::max(0.0f, frame.w - style.popup_padding_x * 2.0f);
    const float y = context.cursor_y() + style.menu_separator_gap;
    flux::runtime::services().draw.rect_filled(
        context.cursor_x(), y, width, std::max(1.0f, style.border_size),
        style.menu_separator, 0.0f);
    context.dummy(width, style.menu_separator_height);
}
bool popup_hovered() {
    if (wr().popup_scopes.empty()) {
        return false;
    }
    const popup_scope& frame = wr().popup_scopes.back();
    const flux::ui::input_state& input = widget_ui().input;
    return input.mouse_x >= frame.x && input.mouse_x < frame.x + frame.w
        && input.mouse_y >= frame.y && input.mouse_y < frame.y + frame.h;
}
}
