#pragma once

#include <cstdint>

#include "ui/types.h"

namespace flux::widgets {

struct control_anim_state {
    float hover = 0.0f;
    float press = 0.0f;
    float focus = 0.0f;
    float value = 0.0f;
};

struct control_anim_targets {
    bool hovered = false;
    bool active = false;
    bool focused = false;
    bool has_value = false;
    bool value = false;
    bool snap = true;
};

control_anim_state& control_anim(flux::ui::ui_id id, const control_anim_targets& targets, uint64_t frame);
bool control_anim_snapshot(flux::ui::ui_id id, control_anim_state& out);
void control_anim_reset();
bool control_anim_animations_enabled(uint64_t frame);
float control_anim_step(float current, float target, float dt, float duration, uint64_t frame);

uint32_t mix_color(uint32_t from, uint32_t to, float t);
uint32_t with_alpha(uint32_t color, float alpha);
uint32_t scale_alpha(uint32_t color, float factor);
float ease_out_cubic(float t);

}
