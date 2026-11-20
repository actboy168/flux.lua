#include "widgets/control_anim.h"

#include <algorithm>
#include <cmath>

#include "runtime/services.h"
#include "ui/motion.h"
#include "widgets/widget_runtime.h"

namespace {

float smooth(float current, float target, float dt, float tau) {
    if (dt <= 0.0f || tau <= 0.0f) {
        return target;
    }
    const float factor = 1.0f - std::exp(-dt / tau);
    float next = current + (target - current) * factor;
    if (std::abs(next - target) < 0.001f) {
        next = target;
    }
    return next;
}

}

namespace flux::widgets {

namespace {

bool animations_enabled(uint64_t frame) {
    widget_runtime_state& state = widget_runtime();
    if (state.anim_enabled_frame != frame) {
        state.anim_enabled_frame = frame;
        state.anim_enabled = flux::runtime::services().ui.animations_enabled();
    }
    return state.anim_enabled;
}

}

control_anim_state& control_anim(flux::ui::ui_id id, const control_anim_targets& targets, uint64_t frame) {
    static control_anim_state empty;
    if (id == 0) {
        return empty;
    }

    widget_runtime_state& state = widget_runtime();

    control_anim_entry& entry = state.anims[id];
    entry.last_frame = frame;
    const float dt = std::clamp(flux::runtime::services().ui.delta_time(), 0.0f, 0.1f);
    const bool enabled = animations_enabled(frame);

    auto channel = [&](float& value, bool& armed, bool target, float tau) {
        if (!armed) {
            if (target) {
                value = 1.0f;
                armed = true;
            } else {
                value = 0.0f;
            }
            return;
        }
        if (!enabled) {
            value = target ? 1.0f : 0.0f;
            return;
        }
        value = smooth(value, target ? 1.0f : 0.0f, dt, tau);
    };

    channel(entry.anim.hover, entry.hover_armed, targets.hovered, flux::ui::motion::hover_tau);
    channel(entry.anim.press, entry.press_armed, targets.active, flux::ui::motion::press_tau);
    channel(entry.anim.focus, entry.focus_armed, targets.focused, flux::ui::motion::focus_tau);

    if (!entry.value_armed) {
        entry.anim.value = (targets.has_value && targets.value && targets.snap) ? 1.0f : 0.0f;
        entry.value_armed = true;
    } else {
        const float target = targets.has_value && targets.value ? 1.0f : 0.0f;
        entry.anim.value = enabled ? smooth(entry.anim.value, target, dt, flux::ui::motion::value_tau) : target;
    }

    return entry.anim;
}

bool control_anim_snapshot(flux::ui::ui_id id, control_anim_state& out) {
    widget_runtime_state& state = widget_runtime();
    auto it = state.anims.find(id);
    if (it == state.anims.end()) {
        return false;
    }
    out = it->second.anim;
    return true;
}

void control_anim_reset() {
    widget_runtime_state& state = widget_runtime();
    state.anims.clear();
    state.anim_enabled_frame = UINT64_MAX;
    state.anim_enabled = true;
}

bool control_anim_animations_enabled(uint64_t frame) {
    return animations_enabled(frame);
}

float control_anim_step(float current, float target, float dt, float duration, uint64_t frame) {
    if (!animations_enabled(frame)) {
        return target;
    }
    if (dt <= 0.0f || duration <= 0.0f) {
        return target;
    }
    const float step = std::clamp(dt, 0.0f, 0.1f) / duration;
    const float next = current < target
        ? std::min(target, current + step)
        : std::max(target, current - step);
    if (std::abs(next - target) > 0.001f) {
        widget_runtime().animation_frame = std::max(widget_runtime().animation_frame, frame + 1);
    }
    return next;
}

uint32_t mix_color(uint32_t from, uint32_t to, float t) {
    if (t <= 0.0f) {
        return from;
    }
    if (t >= 1.0f) {
        return to;
    }
    // Interpolate in premultiplied space so fading from transparent does not
    // pass through black (visible as a dark halo on light backgrounds).
    const float from_alpha = static_cast<float>((from >> 24) & 0xFF) / 255.0f;
    const float to_alpha = static_cast<float>((to >> 24) & 0xFF) / 255.0f;
    const float alpha = from_alpha + (to_alpha - from_alpha) * t;
    uint32_t result = static_cast<uint32_t>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)) << 24;
    if (alpha > 0.0f) {
        for (int shift = 0; shift < 24; shift += 8) {
            const float a = static_cast<float>((from >> shift) & 0xFF) / 255.0f * from_alpha;
            const float b = static_cast<float>((to >> shift) & 0xFF) / 255.0f * to_alpha;
            const float value = (a + (b - a) * t) / alpha;
            result |= static_cast<uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)) << shift;
        }
    }
    return result;
}

uint32_t with_alpha(uint32_t color, float alpha) {
    if (alpha >= 1.0f) {
        return color;
    }
    const uint32_t value = static_cast<uint32_t>(
        std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
    return (color & 0x00FFFFFFu) | (value << 24);
}

uint32_t scale_alpha(uint32_t color, float factor) {
    if (factor >= 1.0f) {
        return color;
    }
    const uint32_t alpha = (color >> 24) & 0xFFu;
    const uint32_t value = static_cast<uint32_t>(
        std::lround(static_cast<float>(alpha) * std::clamp(factor, 0.0f, 1.0f)));
    return (color & 0x00FFFFFFu) | (value << 24);
}

float ease_out_cubic(float t) {
    const float value = std::clamp(t, 0.0f, 1.0f);
    const float inverse = 1.0f - value;
    return 1.0f - inverse * inverse * inverse;
}

}
