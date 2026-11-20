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

bool begin_table(const char* label, const char* const* headers, const float* widths,
                 int columns, float width, float height, float header_height, flux::ui::ui_id id) {
    flux::ui::ui_context& context = widget_ui();
    if (columns <= 0 || headers == nullptr) {
        return false;
    }
    const flux::ui::style_sheet& style = context.style();
    const float x = context.cursor_x();
    const float y = context.cursor_y();
    const float header_h = header_height > 0.0f ? header_height : 30.0f;
    const float total_h = height > 0.0f ? height : 300.0f;
    const float body_h = std::max(1.0f, total_h - header_h);
    const float padding = style.frame_padding_x;
    const float scrollbar = style.scrollbar_size;

    std::vector<float> column_widths(static_cast<size_t>(columns));
    bool has_widths = widths != nullptr;
    if (has_widths) {
        for (int i = 0; i < columns; ++i) {
            column_widths[static_cast<size_t>(i)] = widths[i];
            if (widths[i] <= 0.0f) {
                has_widths = false;
            }
        }
    }

    if (width <= 0.0f) {
        if (!has_widths) {
            return false;
        }
        width = padding * 2.0f + scrollbar;
        for (float value : column_widths) {
            width += value;
        }
    }
    const float content_width = std::max(20.0f, width - padding * 2.0f - scrollbar);
    if (!has_widths) {
        const float equal = content_width / static_cast<float>(columns);
        for (float& value : column_widths) {
            value = equal;
        }
    }

    flux::runtime::services().draw.rect_filled(
        x, y, width, header_h, style.panel, style.rounding);
    flux::runtime::services().draw.rect(
        x, y, width, header_h, style.border, style.rounding, style.border_size);
    flux::runtime::services().draw.rect_filled(
        x, y + header_h - std::max(1.0f, style.border_size), width,
        std::max(1.0f, style.border_size), style.border, 0.0f);

    const flux::ui::ui_id table_id = id != 0 ? id : context.make_id(label);

    float cell_x = x + padding;
    for (int i = 0; i < columns; ++i) {
        const char* display = headers[i] != nullptr ? headers[i] : "";
        const float cell_width = column_widths[static_cast<size_t>(i)];
        draw_text_block(
            cell_x + 8.0f, y, std::max(0.0f, cell_width - 16.0f), header_h,
            display, style.text_dim, style.font_size - 1.0f,
            0, 1, false, true, 0, style.line_height);
        cell_x += cell_width;
    }

    context.set_cursor_pos(x, y + header_h);
    context.begin_child(label, width, body_h, table_id);

    table_scope frame;
    frame.id = table_id;
    frame.body_height = body_h;
    frame.x = x;
    frame.y = y;
    frame.width = width;
    frame.header_height = header_h;
    frame.row_height = 28.0f;
    frame.content_width = content_width;
    frame.columns = std::move(column_widths);
    frame.columns_count = columns;
    frame.current_col = 0;
    wr().table_scopes.push_back(std::move(frame));
    return true;
}
void table_next_row(int index, bool selected, bool& clicked, bool& double_clicked,
                    bool& right_clicked) {
    clicked = false;
    double_clicked = false;
    right_clicked = false;
    if (wr().table_scopes.empty()) {
        return;
    }
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    table_scope& table = wr().table_scopes.back();

    table.current_row = index;
    table.row_count = std::max(table.row_count, index);
    table.current_col = 0;
    table.row_x = context.cursor_x();
    table.row_y = context.cursor_y();
    table.row_w = table.content_width;
    table.row_h = table.row_height;
    table.cell_x = table.row_x;

    const std::string row_label = "row" + flux::text::int_to_string(index);
    bool hovered = false;
    bool active = false;
    bool focused = false;
    context.widget(row_label.c_str(), table.row_w, table.row_h, true,
                   hovered, active, clicked, double_clicked, focused);
    table.row_visible = context.is_item_visible();
    const bool row_selected = selected || clicked;

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
    targets.value = row_selected;
    const control_anim_state& anim =
        control_anim(context.last_item_id(), targets, context.frame_index());
    const float highlight = std::max(anim.hover, anim.press);
    const uint32_t hover_fill = mix_color(0x00000000, style.panel_hover, highlight);
    const uint32_t row_fill = mix_color(hover_fill, style.panel_active, anim.value);
    if (table.row_visible && (row_fill & 0xFF000000u) != 0) {
        flux::runtime::services().draw.rect_filled(
            x, y, w, h, row_fill,
            std::max(0.0f, style.rounding - 3.0f));
    }
    if (table.row_visible && context.focus_visible() && anim.focus > 0.002f) {
        flux::runtime::services().draw.rect(
            x, y, w, h, with_alpha(style.accent, anim.focus),
            std::max(0.0f, style.rounding - 3.0f), 1.0f);
    }
    if (table.row_visible) {
        flux::runtime::services().draw.rect_filled(
            x, y + h - std::max(1.0f, style.border_size), w,
            std::max(1.0f, style.border_size), style.border, 0.0f);
    }

    if (table.row_visible) {
        const flux::ui::input_state& input = context.input;
        const bool input_enabled = !context.modal() || context.popup_active();
        right_clicked = input_enabled && input.mouse_pressed[1]
            && input.mouse_x >= x && input.mouse_x < x + w
            && input.mouse_y >= y && input.mouse_y < y + h;
    }

}
void table_cell_text(const char* text, int align) {
    if (wr().table_scopes.empty()) {
        return;
    }
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    table_scope& table = wr().table_scopes.back();
    if (table.current_col >= table.columns_count) {
        return;
    }
    const float width = table.columns[static_cast<size_t>(table.current_col)];
    if (table.row_visible) {
        const char* value = text != nullptr ? text : "";
        draw_text_block(
            table.cell_x + 8.0f, table.row_y,
            std::max(0.0f, width - 16.0f), table.row_h,
            value, style.text, style.font_size,
            align, 1, false, true, 0, style.line_height);
    }
    table.cell_x += width;
    table.current_col += 1;
}
bool end_table() {
    if (wr().table_scopes.empty()) {
        wr().fail("end_table called without begin_table");
        return false;
    }
    flux::ui::ui_context& context = widget_ui();
    table_scope& table = wr().table_scopes.back();
    context.end_child();
    wr().table_scopes.pop_back();
    return true;
}
}
