#include "gfx/composition_renderer.h"
#include "gfx/composition_renderer_internal.h"

#include <d2d1_1helper.h>
#include <d2d1helper.h>
#include <DispatcherQueue.h>
#include <roapi.h>
#include <windows.graphics.directx.h>
#include <windows.ui.composition.desktop.h>
#include <winstring.h>

#include <cmath>
#include <cwchar>
#include <format>

#include "gfx/color.h"
#include "gfx/draw_replay.h"
#include "gfx/text_engine.h"
#include "gfx/effect_brush.h"
#include "platform/os_win32.h"

namespace flux::gfx {

void composition_renderer::apply_track_value(flux::ui::render_layer layer, const flux::gfx::layer_animation_track& track,
                                            const flux::gfx::layer_value& value) {
    const int index = static_cast<int>(layer);
    flux::gfx::layer_transform& transform = base_transforms_[index];
    switch (track.property) {
    case flux::gfx::layer_property::opacity:
        transform.opacity = value.x;
        break;
    case flux::gfx::layer_property::offset:
        transform.offset_x = value.x;
        transform.offset_y = value.y;
        break;
    case flux::gfx::layer_property::scale:
        transform.scale_x = value.x;
        transform.scale_y = value.y;
        break;
    case flux::gfx::layer_property::rotation:
        transform.rotation = value.x;
        break;
    }
    apply_layer_property(layer, track.property);
}
uint64_t composition_renderer::start_animation(flux::ui::render_layer layer,
                                              const flux::gfx::layer_animation_spec& animation,
                                              std::string& error) {
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        error = "invalid render layer";
        return 0;
    }
    if (layer != flux::ui::render_layer::content) {
        error = "only the content layer supports animation";
        return 0;
    }
    if (animation.tracks.empty()) {
        error = "animation has no tracks";
        return 0;
    }
    if (!ensure_layer(layer, error)) {
        return 0;
    }

    for (size_t i = 0; i < animation.tracks.size(); ++i) {
        const flux::gfx::layer_animation_track& track = animation.tracks[i];
        if (track.keyframes.empty()) {
            error = "animation track has no keyframes";
            return 0;
        }
        if (!track.spring && track.keyframes.size() < 2) {
            error = "keyframe animation needs at least two keyframes";
            return 0;
        }
        for (size_t j = 0; j < i; ++j) {
            if (animation.tracks[j].property == track.property) {
                error = "animation has duplicate property tracks";
                return 0;
            }
        }
    }

    std::vector<uint64_t> conflicts;
    for (const auto& entry : pending_animations_) {
        if (entry.second.layer != layer) {
            continue;
        }
        bool overlaps = false;
        for (const flux::gfx::layer_animation_track& old_track : entry.second.spec.tracks) {
            for (const flux::gfx::layer_animation_track& new_track : animation.tracks) {
                if (old_track.property == new_track.property) {
                    overlaps = true;
                    break;
                }
            }
            if (overlaps) {
                break;
            }
        }
        if (overlaps) {
            conflicts.push_back(entry.first);
        }
    }
    for (uint64_t id : conflicts) {
        cancel_animation(id, false);
    }

    for (const flux::gfx::layer_animation_track& track : animation.tracks) {
        apply_track_value(layer, track, track.keyframes.front().value);
    }

    const uint64_t id = next_animation_id_++;

    if (!animations_enabled()) {
        for (const flux::gfx::layer_animation_track& track : animation.tracks) {
            apply_track_value(layer, track, track.keyframes.back().value);
        }
        if (completed_animations_.size() < 256) {
            completed_animations_.push_back(id);
        }
        return id;
    }

    const uint64_t started = animator_.start(
        surfaces_[index].visual.Get(), animation, dpi_scale_, id, error);
    if (started == 0) {
        return 0;
    }

    pending_animations_[started] = pending_animation{layer, animation};
    return started;
}
bool composition_renderer::stop_animation(uint64_t id, bool apply_final, std::string& error) {
    auto found = pending_animations_.find(id);
    if (found == pending_animations_.end()) {
        error = "unknown or finished animation";
        return false;
    }
    animator_.stop(id);
    if (apply_final) {
        for (const flux::gfx::layer_animation_track& track : found->second.spec.tracks) {
            apply_track_value(found->second.layer, track, track.keyframes.back().value);
        }
    }
    if (completed_animations_.size() < 256) {
        completed_animations_.push_back(id);
    }
    pending_animations_.erase(found);
    return true;
}
bool composition_renderer::animation_running(uint64_t id) const {
    return pending_animations_.find(id) != pending_animations_.end();
}
std::vector<uint64_t> composition_renderer::poll_animations() {
    std::vector<uint64_t> result = std::move(completed_animations_);
    completed_animations_.clear();
    return result;
}
void composition_renderer::on_animation_completed(uint64_t id) {
    auto found = pending_animations_.find(id);
    if (found == pending_animations_.end()) {
        return;
    }
    animator_.finish(id);
    for (const flux::gfx::layer_animation_track& track : found->second.spec.tracks) {
        apply_track_value(found->second.layer, track, track.keyframes.back().value);
    }
    if (completed_animations_.size() < 256) {
        completed_animations_.push_back(id);
    }
    pending_animations_.erase(found);
}
bool composition_renderer::animations_enabled() const {
    if (animations_override_ == 0) {
        return false;
    }
    if (animations_override_ == 1) {
        return true;
    }
    return wuc_animator::system_animations_enabled();
}
void composition_renderer::debug_set_animations_enabled(int value) {
    animations_override_ = value < 0 ? -1 : (value == 0 ? 0 : 1);
}
void composition_renderer::cancel_animation(uint64_t id, bool apply_final) {
    auto found = pending_animations_.find(id);
    if (found == pending_animations_.end()) {
        return;
    }
    animator_.stop(id);
    if (apply_final) {
        for (const flux::gfx::layer_animation_track& track : found->second.spec.tracks) {
            apply_track_value(found->second.layer, track, track.keyframes.back().value);
        }
    }
    if (completed_animations_.size() < 256) {
        completed_animations_.push_back(id);
    }
    pending_animations_.erase(found);
}
void composition_renderer::cancel_layer_animations(flux::ui::render_layer layer, bool apply_final) {
    std::vector<uint64_t> ids;
    for (const auto& entry : pending_animations_) {
        if (entry.second.layer == layer) {
            ids.push_back(entry.first);
        }
    }
    for (uint64_t id : ids) {
        cancel_animation(id, apply_final);
    }
}
void composition_renderer::cancel_all_animations(bool apply_final) {
    std::vector<uint64_t> ids;
    ids.reserve(pending_animations_.size());
    for (const auto& entry : pending_animations_) {
        ids.push_back(entry.first);
    }
    for (uint64_t id : ids) {
        cancel_animation(id, apply_final);
    }
}
}
