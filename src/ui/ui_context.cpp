#include "ui/ui_context.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "ui/draw_list.h"

namespace flux::ui {

namespace {

constexpr float k_pointer_tolerance = 0.05f;

uint32_t scale_alpha_color(uint32_t color, float factor) {
    if (factor >= 1.0f) {
        return color;
    }
    const uint32_t alpha = static_cast<uint32_t>(std::lround(
        ((color >> 24) & 0xFFu) * std::clamp(factor, 0.0f, 1.0f)));
    return (color & 0x00FFFFFFu) | (alpha << 24);
}

}

void ui_context::begin_frame(float delta_time, bool animations_enabled) {
    animations_enabled_ = animations_enabled;
    frame_index_ += 1;
    delta_time_ = delta_time;
    time_ += delta_time;
    if (input.mouse_pressed[0] || input.mouse_pressed[1] || input.mouse_pressed[2]) {
        focus_visible_ = false;
    }
    focus_order_.clear();
    auto_close_popups();
    modal_ = has_modal_popup();
    popup_active_ = false;
    edit_focused_ = false;
    frame_error_.clear();
    last_error_.clear();
    if (!child_frames_.empty()) {
        fail_frame("child scope was not balanced before frame begin");
        child_frames_.clear();
    }
    if (!clips_.empty()) {
        fail_frame("clip stack was not balanced before frame begin");
        clips_.clear();
    }
    if (!layout_frames_.empty()) {
        fail_frame("layout scope was not balanced before frame begin");
        layout_frames_.clear();
    }
    id_imbalance_ = !id_stack_.empty();
    if (id_imbalance_) {
        fail_frame("ID stack was not balanced before frame begin");
        id_stack_.clear();
    }
    if (style_depth() != 0) {
        style_imbalance_ = true;
        fail_frame("style stack was not balanced before frame begin");
        style_color_frames_.clear();
        style_var_frames_.clear();
    } else {
        style_imbalance_ = false;
    }
    cursor_x_ = 0.0f;
    cursor_y_ = 0.0f;
    line_start_x_ = 0.0f;
    item_x_ = 0.0f;
    item_y_ = 0.0f;
    item_w_ = 0.0f;
    item_h_ = 0.0f;
    last_item_id_ = 0;
    item_visible_ = true;
}

void ui_context::end_frame() {
    if (input.key_pressed[keys::tab] && !focus_order_.empty()) {
        const size_t count = focus_order_.size();
        auto found = std::find(focus_order_.begin(), focus_order_.end(), focused_id_);
        size_t index = 0;
        if (found != focus_order_.end()) {
            index = static_cast<size_t>(found - focus_order_.begin());
            index = input.key_shift[keys::tab]
                ? (index + count - 1) % count
                : (index + 1) % count;
        } else {
            index = input.key_shift[keys::tab] ? count - 1 : 0;
        }
        focused_id_ = focus_order_[index];
        focus_visible_ = true;
    }

    for (auto it = states_.begin(); it != states_.end();) {
        if (frame_index_ > it->second.last_frame + 2) {
            it = states_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = children_.begin(); it != children_.end();) {
        if (frame_index_ > it->second.last_frame + 2) {
            it = children_.erase(it);
        } else {
            ++it;
        }
    }
    if (focused_id_ != 0 && states_.find(focused_id_) == states_.end()) {
        focused_id_ = 0;
        focus_visible_ = false;
    }
    if (active_id_ != 0 && states_.find(active_id_) == states_.end()) {
        active_id_ = 0;
    }
    if (style_depth() != 0) {
        style_imbalance_ = true;
        fail_frame("style stack is not balanced");
        style_color_frames_.clear();
        style_var_frames_.clear();
    }
    if (!child_frames_.empty()) {
        fail_frame("child scope is not balanced");
        child_frames_.clear();
    }
    if (!clips_.empty()) {
        fail_frame("clip stack is not balanced");
        clips_.clear();
    }
    if (!layout_frames_.empty()) {
        fail_frame("layout scope is not balanced");
        layout_frames_.clear();
    }
    if (!id_stack_.empty()) {
        id_imbalance_ = true;
        fail_frame("ID stack is not balanced");
        id_stack_.clear();
    }
    input.end_frame();
}

void ui_context::open_popup(ui_id id, bool modal) {
    const int level = id_depth();
    while (!open_popups_.empty() && open_popups_.back().level >= level) {
        open_popups_.pop_back();
    }
    open_popups_.push_back({id, modal, level});
    if (modal) {
        modal_ = true;
    }
}

void ui_context::close_popup(ui_id id) {
    if (open_popups_.empty()) {
        return;
    }
    if (id == 0) {
        open_popups_.pop_back();
        return;
    }
    for (size_t index = open_popups_.size(); index-- > 0;) {
        if (open_popups_[index].id == id) {
            open_popups_.resize(index);
            break;
        }
    }
}

bool ui_context::popup_is_open(ui_id id) const {
    const int level = id_depth();
    for (size_t index = open_popups_.size(); index-- > 0;) {
        const open_popup_state& popup = open_popups_[index];
        if (popup.level == level) {
            return popup.id == id;
        }
    }
    return false;
}

bool ui_context::has_modal_popup() const {
    for (const open_popup_state& popup : open_popups_) {
        if (popup.modal) {
            return true;
        }
    }
    return false;
}

void ui_context::set_popup_modal(ui_id id, bool modal) {
    for (size_t index = open_popups_.size(); index-- > 0;) {
        if (open_popups_[index].id == id) {
            open_popups_[index].modal = modal;
            if (modal) {
                modal_ = true;
            }
            return;
        }
    }
}

void ui_context::update_popup_rect(ui_id id, float x, float y, float width, float height) {
    for (size_t index = open_popups_.size(); index-- > 0;) {
        if (open_popups_[index].id == id) {
            open_popup_state& popup = open_popups_[index];
            popup.has_rect = true;
            popup.x = x;
            popup.y = y;
            popup.w = width;
            popup.h = height;
            return;
        }
    }
}

void ui_context::auto_close_popups() {
    if (open_popups_.empty()) {
        return;
    }
    if (input.key_pressed[keys::escape]) {
        for (size_t index = open_popups_.size(); index-- > 0;) {
            if (!open_popups_[index].modal) {
                open_popups_.erase(open_popups_.begin() + index);
            }
        }
    }
    if (!input.mouse_pressed[0]) {
        return;
    }
    while (!open_popups_.empty()) {
        const open_popup_state& popup = open_popups_.back();
        if (popup.modal || !popup.has_rect) {
            break;
        }
        const bool inside = input.mouse_x >= popup.x && input.mouse_x < popup.x + popup.w
            && input.mouse_y >= popup.y && input.mouse_y < popup.y + popup.h;
        if (inside) {
            break;
        }
        open_popups_.pop_back();
    }
}

bool ui_context::has_active_animations() const {
    if (edit_focused_) {
        return true;
    }
    for (const auto& entry : children_) {
        if (entry.second.scroll_animating) {
            return true;
        }
    }
    return false;
}

void ui_context::clear_interaction() {
    input.reset();
    active_id_ = 0;
    focused_id_ = 0;
    focus_visible_ = false;
    modal_ = false;
    open_popups_.clear();
}

uint64_t ui_context::hash_bytes(const char* data, size_t length) {
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < length; ++i) {
        hash ^= static_cast<uint8_t>(data[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

ui_id ui_context::make_id(std::string_view label) const {
    uint64_t hash = hash_bytes(label.data(), label.size());
    for (ui_id parent : id_stack_) {
        hash ^= parent + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
    }
    return hash == 0 ? 1 : hash;
}

ui_id ui_context::make_id(const char* label) const {
    return label != nullptr ? make_id(std::string_view(label)) : make_id(std::string_view());
}

ui_id ui_context::make_id_absolute(std::string_view label) const {
    const uint64_t hash = hash_bytes(label.data(), label.size());
    return hash == 0 ? 1 : hash;
}

void ui_context::push_id(std::string_view label) {
    id_stack_.push_back(make_id(label));
}

void ui_context::push_id(const char* label) {
    push_id(label != nullptr ? std::string_view(label) : std::string_view());
}

void ui_context::push_id(ui_id id) {
    id_stack_.push_back(id != 0 ? id : 1);
}

void ui_context::pop_id() {
    if (id_stack_.empty()) {
        id_imbalance_ = true;
        fail_frame("pop_id called without push_id");
        return;
    }
    id_stack_.pop_back();
}

}
