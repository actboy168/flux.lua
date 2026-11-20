#include "ui/ui_context.h"

#include "ui/draw_list.h"
#include "ui/ui_internal.h"

namespace flux::ui {

void ui_context::set_cursor_pos(float x, float y) {
    cursor_x_ = x;
    cursor_y_ = y;
    line_start_x_ = x;
}

void ui_context::same_line() {
    cursor_x_ = item_x_ + item_w_ + style().item_spacing_x;
    cursor_y_ = item_y_;
}

void ui_context::dummy(float width, float height) {
    advance_cursor(cursor_x_, cursor_y_, width, height);
}

void ui_context::set_item_spacing(float x, float y) {
    set_base_style_var(style_var_item_spacing_x, x);
    set_base_style_var(style_var_item_spacing_y, y);
}

void ui_context::indent(float width) {
    cursor_x_ += width;
    line_start_x_ += width;
}

void ui_context::unindent(float width) {
    cursor_x_ -= width;
    line_start_x_ -= width;
}

void ui_context::begin_row() {
    layout_frame frame;
    frame.row = true;
    frame.start_x = cursor_x_;
    frame.start_y = cursor_y_;
    frame.max_height = 0.0f;
    layout_frames_.push_back(frame);
    line_start_x_ = cursor_x_;
}

bool ui_context::end_row() {
    if (layout_frames_.empty() || !layout_frames_.back().row) {
        fail_frame("end_row called without matching begin_row");
        return false;
    }
    layout_frame frame = layout_frames_.back();
    layout_frames_.pop_back();
    cursor_x_ = frame.start_x;
    const float row_height = frame.form
        ? std::max(frame.max_height, frame.form_row_height)
        : frame.max_height;
    cursor_y_ = frame.start_y + row_height + style().item_spacing_y;
    line_start_x_ = frame.start_x;
    return true;
}

void ui_context::begin_form_row(float label_width, float gap, float row_height) {
    layout_frame frame;
    frame.row = true;
    frame.form = true;
    frame.start_x = cursor_x_;
    frame.start_y = cursor_y_;
    frame.max_height = 0.0f;
    frame.form_label_width = std::max(0.0f, label_width);
    frame.form_row_height = std::max(0.0f, row_height);
    if (gap < 0.0f) {
        gap = style().item_spacing_x;
    }
    frame.form_control_x = cursor_x_ + frame.form_label_width + std::max(0.0f, gap);
    layout_frames_.push_back(frame);
    line_start_x_ = cursor_x_;
}

void ui_context::form_align(float x) {
    if (layout_frames_.empty() || !layout_frames_.back().form) {
        return;
    }
    const layout_frame& frame = layout_frames_.back();
    cursor_x_ = x < 0.0f ? frame.form_control_x : x;
    cursor_y_ = frame.start_y;
}

bool ui_context::end_form_row() {
    if (layout_frames_.empty() || !layout_frames_.back().row
        || !layout_frames_.back().form) {
        fail_frame("end_form_row called without matching begin_form_row");
        return false;
    }
    return end_row();
}

float ui_context::form_label_x() const {
    return (!layout_frames_.empty() && layout_frames_.back().form)
        ? layout_frames_.back().start_x : cursor_x_;
}

float ui_context::form_control_x() const {
    return (!layout_frames_.empty() && layout_frames_.back().form)
        ? layout_frames_.back().form_control_x : cursor_x_;
}

float ui_context::form_row_y() const {
    return (!layout_frames_.empty() && layout_frames_.back().form)
        ? layout_frames_.back().start_y : cursor_y_;
}

float ui_context::form_row_height() const {
    return (!layout_frames_.empty() && layout_frames_.back().form)
        ? layout_frames_.back().form_row_height : 0.0f;
}

void ui_context::begin_column() {
    layout_frame frame;
    frame.row = false;
    frame.start_x = cursor_x_;
    frame.start_y = cursor_y_;
    layout_frames_.push_back(frame);
    line_start_x_ = cursor_x_;
}

bool ui_context::end_column() {
    if (layout_frames_.empty() || layout_frames_.back().row) {
        fail_frame("end_column called without matching begin_column");
        return false;
    }
    layout_frames_.pop_back();
    return true;
}

void ui_context::advance_cursor(float x, float y, float width, float height) {
    if (!layout_frames_.empty() && layout_frames_.back().row) {
        layout_frame& frame = layout_frames_.back();
        frame.max_height = std::max(frame.max_height, height);
        cursor_x_ = x + width + style().item_spacing_x;
        cursor_y_ = y;
        return;
    }
    cursor_x_ = line_start_x_;
    cursor_y_ = y + height + style().item_spacing_y;
}

}
