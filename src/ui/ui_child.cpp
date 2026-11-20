#include "ui/ui_context.h"

#include "ui/draw_list.h"
#include "ui/ui_internal.h"

namespace flux::ui {

using detail::scale_alpha_color;
constexpr float k_pointer_tolerance = detail::pointer_tolerance;

bool ui_context::is_inside_clip(float x, float y, float width, float height) const {
    for (const clip_rect& clip : clips_) {
        if (x + width <= clip.x || x >= clip.x + clip.w
            || y + height <= clip.y || y >= clip.y + clip.h) {
            return false;
        }
    }
    return true;
}

bool ui_context::is_rect_visible(float x, float y, float width, float height) const {
    return is_inside_clip(x, y, width, height);
}

void ui_context::push_clip(float x, float y, float width, float height) {
    clip_rect clip;
    clip.x = x;
    clip.y = y;
    clip.w = width;
    clip.h = height;
    clips_.push_back(clip);
}

void ui_context::pop_clip() {
    if (!clips_.empty()) {
        clips_.pop_back();
    }
}

child_view_state& ui_context::child_state(ui_id id) {
    return children_[id];
}

bool ui_context::begin_child(const char* label, float width, float height, ui_id id) {
    return begin_child_impl(label, width, height, nullptr, id);
}

bool ui_context::begin_child_fixed(const char* label, float width, float height,
                                   float content_height, ui_id id) {
    return begin_child_impl(label, width, height, &content_height, id);
}

void ui_context::route_wheel() {
    if (wheel_route_frame_ == frame_index_) {
        return;
    }
    wheel_remaining_ = 0.0f;
    if (input.wheel == 0.0f || !input.mouse_inside || !input.window_active) {
        wheel_route_frame_ = frame_index_;
        return;
    }
    if (modal_ && !popup_active_) {
        return;
    }
    wheel_route_frame_ = frame_index_;
    const bool popup_scope = popup_active_;

    ui_id deepest = 0;
    int deepest_depth = -1;
    for (const auto& entry : children_) {
        const child_view_state& child = entry.second;
        if (child.last_frame + 1 < frame_index_) {
            continue;
        }
        if (child.popup != popup_scope) {
            continue;
        }
        if (input.mouse_x < child.x || input.mouse_x >= child.x + child.w
            || input.mouse_y < child.y || input.mouse_y >= child.y + child.h) {
            continue;
        }
        if (child.depth > deepest_depth) {
            deepest = entry.first;
            deepest_depth = child.depth;
        }
    }
    if (deepest == 0) {
        return;
    }

    std::vector<ui_id> chain;
    ui_id id = deepest;
    for (int guard = 0; id != 0 && guard < 64; ++guard) {
        auto it = children_.find(id);
        if (it == children_.end()) {
            break;
        }
        const child_view_state& child = it->second;
        if (input.mouse_x < child.x || input.mouse_x >= child.x + child.w
            || input.mouse_y < child.y || input.mouse_y >= child.y + child.h) {
            break;
        }
        chain.push_back(id);
        id = child.parent;
    }

    float budget = input.wheel * 48.0f;
    for (ui_id node : chain) {
        child_view_state& child = children_[node];
        const float view_height = child.view_height > 0.0f ? child.view_height : child.h;
        const float max_scroll = std::max(0.0f, child.content_height - view_height);
        const float old_target = child.scroll_target;
        const float next_target = std::clamp(old_target - budget, 0.0f, max_scroll);
        const float consumed = old_target - next_target;
        if (consumed == 0.0f) {
            continue;
        }
        child.scroll_target = next_target;
        if (animations_enabled_ && scroll_animation_seconds_ > 0.0f) {
            child.scroll_anim_from = child.scroll;
            child.scroll_anim_start = static_cast<double>(
                time_ - std::max(delta_time_, 1.0f / 60.0f));
            child.scroll_animating = true;
        } else {
            child.scroll = next_target;
            child.scroll_animating = false;
        }
        budget -= consumed;
        if (std::abs(budget) < 1e-4f) {
            break;
        }
    }
    wheel_remaining_ = budget;
    input.wheel = budget / 48.0f;
}

bool ui_context::begin_child_impl(const char* label, float width, float height,
                                  const float* fixed_content, ui_id id) {
    if (width <= 0.0f || height <= 0.0f) {
        return false;
    }
    route_wheel();
    const ui_id resolved_id = id != 0 ? id : make_id_absolute(label);
    child_view_state& child = child_state(resolved_id);
    child.last_frame = frame_index_;
    child.x = cursor_x_;
    child.y = cursor_y_;
    child.w = width;
    child.h = height;
    child.view_height = height;
    child.depth = static_cast<int>(child_frames_.size());
    child.popup = popup_active_;
    child.parent = (child_frames_.empty()
        || (popup_active_ && child.depth <= popup_child_depth_))
        ? 0 : child_frames_.back().id;
    if (fixed_content != nullptr) {
        child.content_height = std::max(0.0f, *fixed_content);
    }
    if (child.scroll_animating) {
        if (!animations_enabled_ || scroll_animation_seconds_ <= 0.0f) {
            child.scroll = child.scroll_target;
            child.scroll_animating = false;
        } else {
            const float t = std::clamp(
                static_cast<float>((static_cast<double>(time_) - child.scroll_anim_start)
                    / static_cast<double>(scroll_animation_seconds_)),
                0.0f, 1.0f);
            const float inv = 1.0f - t;
            const float eased = 1.0f - inv * inv * inv;
            child.scroll = child.scroll_anim_from
                + (child.scroll_target - child.scroll_anim_from) * eased;
            if (t >= 1.0f) {
                child.scroll = child.scroll_target;
                child.scroll_animating = false;
            }
        }
    }

    const bool hovered = input.mouse_inside
        && input.mouse_x >= child.x && input.mouse_x < child.x + width
        && input.mouse_y >= child.y && input.mouse_y < child.y + height;
    if (child.scrollbar_initialized) {
        const float idle_alpha = std::clamp(style().scrollbar_idle_alpha, 0.0f, 1.0f);
        const float target = (hovered || child.dragging) ? 1.0f : idle_alpha;
        if (!animations_enabled_) {
            child.scrollbar_alpha = target;
        } else {
            const float factor = 1.0f - std::exp(-delta_time_ / 0.100f);
            child.scrollbar_alpha += (target - child.scrollbar_alpha) * factor;
            if (std::abs(child.scrollbar_alpha - target) < 0.01f) {
                child.scrollbar_alpha = target;
            }
        }
    }
    const float max_scroll = std::max(0.0f, child.content_height - height);

    if (max_scroll > 0.0f) {
        const float bar = style().scrollbar_size;
        const float track_x = child.x + width - bar;
        const float thumb_height = std::max(24.0f, height * height / child.content_height);
        const float progress = child.scroll / max_scroll;
        const float thumb_y = child.y + (height - thumb_height) * progress;
        const bool over_thumb = input.mouse_x >= track_x && input.mouse_x < track_x + bar
            && input.mouse_y >= thumb_y && input.mouse_y < thumb_y + thumb_height;
        if (input.mouse_pressed[0] && over_thumb) {
            child.dragging = true;
            child.drag_offset = input.mouse_y - thumb_y;
        }
        if (child.dragging) {
            if (input.mouse_down[0]) {
                const float thumb_top = height - thumb_height;
                const float t = (input.mouse_y - child.drag_offset - child.y)
                    / std::max(1.0f, thumb_top);
                child.scroll = std::clamp(t, 0.0f, 1.0f) * max_scroll;
                child.scroll_target = child.scroll;
                child.scroll_animating = false;
            } else {
                child.dragging = false;
            }
        } else if (input.mouse_pressed[0] && hovered && input.mouse_x >= track_x) {
            child.scroll = std::clamp(
                child.scroll + (input.mouse_y < thumb_y ? -height : height),
                0.0f,
                max_scroll);
            child.scroll_target = child.scroll;
            child.scroll_animating = false;
        }
    }

    const float x = cursor_x_;
    const float y = cursor_y_;
    advance_cursor(x, y, width, height);

    child_frame frame;
    frame.id = resolved_id;
    frame.x = x;
    frame.y = y;
    frame.w = width;
    frame.h = height;
    frame.saved_cursor_x = cursor_x_;
    frame.saved_cursor_y = cursor_y_;
    frame.saved_line_start_x = line_start_x_;
    frame.content_top = y - child.scroll;
    frame.fixed_content = fixed_content != nullptr;
    child_frames_.push_back(frame);

    push_clip(x, y, width, height);
    if (draw_list_ != nullptr) {
        draw_list_->push_clip(x, y, width, height);
    }
    cursor_x_ = x;
    cursor_y_ = frame.content_top;
    line_start_x_ = x;
    push_id(resolved_id);
    return true;
}

bool ui_context::end_child() {
    if (child_frames_.empty()) {
        fail_frame("end_child called without begin_child");
        return false;
    }
    const child_frame frame = child_frames_.back();
    child_view_state& child = child_state(frame.id);

    const float content_height = frame.fixed_content
        ? child.content_height
        : std::max(0.0f, cursor_y_ + child.scroll - frame.y);
    child.content_height = content_height;
    child.scrollbar_initialized = true;
    const float max_scroll = std::max(0.0f, content_height - frame.h);
    child.scroll = std::clamp(child.scroll, 0.0f, max_scroll);
    child.scroll_target = std::clamp(child.scroll_target, 0.0f, max_scroll);
    if (!child.scroll_animating) {
        child.scroll = child.scroll_target;
    }

    pop_clip();
    if (draw_list_ != nullptr) {
        draw_list_->pop_clip();
    }

    if (content_height > frame.h && child.scrollbar_alpha > 0.01f) {
        const style_sheet& current = style();
        const float bar = current.scrollbar_size;
        const float track_x = frame.x + frame.w - bar;
        if (draw_list_ != nullptr) {
            draw_list_->rect_filled(
                track_x, frame.y, bar, frame.h,
                scale_alpha_color(current.track, child.scrollbar_alpha), bar * 0.5f);
        }
        const float thumb_height = std::max(24.0f, frame.h * frame.h / content_height);
        const float progress = max_scroll > 0.0f ? child.scroll / max_scroll : 0.0f;
        const float thumb_y = frame.y + (frame.h - thumb_height) * progress;
        const uint32_t color = child.dragging ? current.text : current.text_dim;
        if (draw_list_ != nullptr) {
            draw_list_->rect_filled(
                track_x + 2.0f, thumb_y, std::max(1.0f, bar - 4.0f), thumb_height,
                scale_alpha_color(color, child.scrollbar_alpha),
                std::max(1.0f, bar - 4.0f) * 0.5f);
        }
    }

    cursor_x_ = frame.saved_cursor_x;
    cursor_y_ = frame.saved_cursor_y;
    line_start_x_ = frame.saved_line_start_x;
    child_frames_.pop_back();
    pop_id();
    return true;
}

float ui_context::child_scroll(const char* label) const {
    return child_scroll(make_id_absolute(label));
}

float ui_context::child_content_height(const char* label) const {
    return child_content_height(make_id_absolute(label));
}

void ui_context::set_child_scroll(const char* label, float value) {
    set_child_scroll(make_id_absolute(label), value);
}

float ui_context::child_scroll(ui_id id) const {
    auto it = children_.find(id);
    return it == children_.end() ? 0.0f : it->second.scroll;
}

float ui_context::child_content_height(ui_id id) const {
    auto it = children_.find(id);
    return it == children_.end() ? 0.0f : it->second.content_height;
}

void ui_context::set_child_scroll(ui_id id, float value) {
    child_view_state& child = child_state(id);
    child.scroll = std::max(0.0f, value);
    child.scroll_target = child.scroll;
    child.scroll_animating = false;
}

}
