#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ui/input.h"
#include "ui/style.h"
#include "ui/types.h"

namespace flux::ui {

struct draw_list;

struct widget_view_state {
    bool hovered = false;
    bool active = false;
    bool focused = false;
    bool clicked = false;
    bool double_clicked = false;
    uint64_t last_frame = 0;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct child_view_state {
    uint64_t last_frame = 0;
    float scroll = 0.0f;
    float scroll_target = 0.0f;
    float scroll_anim_from = 0.0f;
    double scroll_anim_start = 0.0;
    bool scroll_animating = false;
    float content_height = 0.0f;
    float view_height = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    int depth = 0;
    ui_id parent = 0;
    bool popup = false;
    bool dragging = false;
    float drag_offset = 0.0f;
    float scrollbar_alpha = 1.0f;
    bool scrollbar_initialized = false;
};

class ui_context {
public:
    ui_context() {
        current_style_ = base_style_;
    }

    input_state input;

    void attach_draw_list(draw_list* list) {
        draw_list_ = list;
    }
    bool animations_enabled() const {
        return animations_enabled_;
    }
    bool has_active_animations() const;

    void begin_frame(float delta_time, bool animations_enabled);
    void end_frame();
    void clear_interaction();

    ui_id make_id(std::string_view label) const;
    ui_id make_id(const char* label) const;
    ui_id make_id_absolute(std::string_view label) const;
    void push_id(std::string_view label);
    void push_id(const char* label);
    void push_id(ui_id id);
    void pop_id();

    void set_cursor_pos(float x, float y);
    float cursor_x() const {
        return cursor_x_;
    }
    float cursor_y() const {
        return cursor_y_;
    }
    float line_start_x() const {
        return line_start_x_;
    }
    void set_line_start_x(float x) {
        line_start_x_ = x;
    }
    void same_line();
    void dummy(float width, float height);
    void set_item_spacing(float x, float y);
    float item_spacing_x() const { return current_style_.item_spacing_x; }
    float item_spacing_y() const { return current_style_.item_spacing_y; }
    void indent(float width);
    void unindent(float width);

    void begin_row();
    bool end_row();
    void begin_column();
    bool end_column();
    void begin_form_row(float label_width, float gap = -1.0f, float row_height = 0.0f);
    void form_align(float x = -1.0f);
    bool end_form_row();
    int child_depth() const {
        return static_cast<int>(child_frames_.size());
    }
    int layout_depth() const {
        return static_cast<int>(layout_frames_.size());
    }
    float form_label_x() const;
    float form_control_x() const;
    float form_row_y() const;
    float form_row_height() const;

    struct widget_input_options {
        bool interactive = true;
        bool focusable = true;
        bool allow_modal = false;
        bool tab_stop = true;
    };

    void widget(const char* label, float width, float height, bool focusable,
                bool& hovered, bool& active, bool& clicked, bool& double_clicked, bool& focused,
                bool allow_modal = false, ui_id id = 0);
    void widget_ex(const char* label, float width, float height,
                   const widget_input_options& options,
                   bool& hovered, bool& active, bool& clicked, bool& double_clicked,
                   bool& focused, ui_id id = 0);

    bool is_item_hovered() const;
    bool is_item_active() const;
    bool is_item_clicked() const;
    bool is_item_double_clicked() const;
    bool is_item_focused() const;
    bool is_item_visible() const;
    void last_item_rect(float& x, float& y, float& width, float& height) const;
    ui_id last_item_id() const {
        return last_item_id_;
    }
    ui_id keyboard_focus_id() const {
        return focused_id_;
    }
    bool focus_visible() const {
        return focus_visible_;
    }
    void set_focus_visible(bool visible) {
        focus_visible_ = visible;
    }
    void set_keyboard_focus(const char* label);
    void set_keyboard_focus_id(ui_id id);
    void set_modal(bool modal);
    int id_depth() const {
        return static_cast<int>(id_stack_.size());
    }
    bool id_imbalance() const {
        return id_imbalance_;
    }
    const std::string& frame_error() const {
        return frame_error_;
    }
    const std::string& last_error() const {
        return last_error_;
    }
    void open_popup(ui_id id, bool modal);
    void close_popup(ui_id id = 0);
    bool popup_is_open(ui_id id) const;
    bool has_modal_popup() const;
    void set_popup_modal(ui_id id, bool modal);
    void update_popup_rect(ui_id id, float x, float y, float width, float height);
    bool modal() const {
        return modal_;
    }
    void set_popup_active(bool active) {
        popup_active_ = active;
        if (active) {
            popup_child_depth_ = static_cast<int>(child_frames_.size());
        }
    }
    bool popup_active() const {
        return popup_active_;
    }
    void set_edit_focused(bool focused) {
        edit_focused_ = focused;
    }
    bool edit_focused() const {
        return edit_focused_;
    }

    bool begin_child(const char* label, float width, float height, ui_id id = 0);
    bool begin_child_fixed(const char* label, float width, float height,
                           float content_height, ui_id id = 0);
    bool end_child();
    float child_scroll(const char* label) const;
    float child_scroll(ui_id id) const;
    void set_child_scroll(ui_id id, float value);
    float child_content_height(const char* label) const;
    float child_content_height(ui_id id) const;
    void set_child_scroll(const char* label, float value);

    void push_clip(float x, float y, float width, float height);
    void pop_clip();
    bool is_rect_visible(float x, float y, float width, float height) const;
    int clip_depth() const {
        return static_cast<int>(clips_.size());
    }
    std::vector<clip_rect> save_clips() const {
        return clips_;
    }
    void restore_clips(std::vector<clip_rect> clips) {
        clips_ = std::move(clips);
    }
    void clear_clips() {
        clips_.clear();
    }

    const style_sheet& style() const;
    void push_style_color(int index, uint32_t color);
    void push_style_var(int index, float value);
    bool pop_style_color(int count);
    bool pop_style_var(int count);
    int style_depth() const {
        return static_cast<int>(style_color_frames_.size() + style_var_frames_.size());
    }
    bool style_imbalance() const {
        return style_imbalance_;
    }
    uint32_t style_color(int index) const;
    float style_var(int index) const;
    void set_base_style_color(int index, uint32_t color);
    void set_base_style_var(int index, float value);

    float delta_time() const {
        return delta_time_;
    }
    float time() const {
        return time_;
    }
    uint64_t frame_index() const {
        return frame_index_;
    }

private:
    struct style_color_frame {
        int index = 0;
        uint32_t color = 0;
    };

    struct style_var_frame {
        int index = 0;
        float value = 0.0f;
    };

    struct child_frame {
        ui_id id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        float saved_cursor_x = 0.0f;
        float saved_cursor_y = 0.0f;
        float saved_line_start_x = 0.0f;
        float content_top = 0.0f;
        bool fixed_content = false;
    };

    struct layout_frame {
        bool row = false;
        bool form = false;
        float start_x = 0.0f;
        float start_y = 0.0f;
        float max_height = 0.0f;
        float form_label_width = 0.0f;
        float form_control_x = 0.0f;
        float form_row_height = 0.0f;
    };

    static uint64_t hash_bytes(const char* data, size_t length);
    void fail_frame(const char* message) {
        last_error_ = message;
        if (frame_error_.empty()) {
            frame_error_ = message;
        }
    }
    void apply_style_color(style_sheet& style, const style_color_frame& frame) const;
    void apply_style_var(style_sheet& style, const style_var_frame& frame) const;
    void rebuild_style();
    bool begin_child_impl(const char* label, float width, float height,
                          const float* fixed_content, ui_id id = 0);
    widget_view_state& state_for(ui_id id);
    const widget_view_state* find_state(ui_id id) const;
    void register_focus(ui_id id);
    void advance_cursor(float x, float y, float width, float height);
    bool is_inside_clip(float x, float y, float width, float height) const;
    child_view_state& child_state(ui_id id);

    std::unordered_map<ui_id, widget_view_state> states_;
    std::unordered_map<ui_id, child_view_state> children_;
    std::vector<ui_id> id_stack_;
    bool id_imbalance_ = false;
    std::string frame_error_;
    std::string last_error_;
    std::vector<ui_id> focus_order_;
    std::vector<clip_rect> clips_;
    std::vector<child_frame> child_frames_;
    uint64_t wheel_route_frame_ = 0;
    float wheel_remaining_ = 0.0f;
    float scroll_animation_seconds_ = 0.16f;
    void route_wheel();
    std::vector<layout_frame> layout_frames_;
    std::vector<style_color_frame> style_color_frames_;
    std::vector<style_var_frame> style_var_frames_;
    style_sheet base_style_;
    style_sheet current_style_;
    bool style_imbalance_ = false;
    ui_id last_item_id_ = 0;
    ui_id focused_id_ = 0;
    ui_id active_id_ = 0;
    bool focus_visible_ = false;
    float cursor_x_ = 0.0f;
    float cursor_y_ = 0.0f;
    float line_start_x_ = 0.0f;
    float item_x_ = 0.0f;
    float item_y_ = 0.0f;
    float item_w_ = 0.0f;
    float item_h_ = 0.0f;
    bool item_visible_ = true;
    bool modal_ = false;

    struct open_popup_state {
        ui_id id = 0;
        bool modal = false;
        int level = 0;
        bool has_rect = false;
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
    };
    std::vector<open_popup_state> open_popups_;
    void auto_close_popups();
    bool popup_active_ = false;
    int popup_child_depth_ = 0;
    bool edit_focused_ = false;
    uint64_t frame_index_ = 0;
    float delta_time_ = 0.0f;
    float time_ = 0.0f;
    draw_list* draw_list_ = nullptr;
    bool animations_enabled_ = true;
};

}
