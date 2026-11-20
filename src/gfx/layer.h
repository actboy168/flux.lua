#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui/render_layer.h"

namespace flux::gfx {

enum class layer_property : int {
    opacity = 0,
    offset = 1,
    scale = 2,
    rotation = 3,
};

const char* layer_property_name(layer_property property);
bool parse_layer_property(const char* name, layer_property& property);

struct layer_transform {
    float opacity = 1.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float rotation = 0.0f;
};

struct layer_shadow {
    float blur = 16.0f;
    float offset_x = 0.0f;
    float offset_y = 4.0f;
    float opacity = 0.6f;
    uint32_t color = 0xFF000000u;
};

struct layer_clip {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float radius = 0.0f;
};

struct layer_value {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    int dims = 1;
};

struct layer_keyframe {
    float t = 0.0f;
    layer_value value;
};

struct layer_animation_track {
    layer_property property = layer_property::opacity;
    std::vector<layer_keyframe> keyframes;
    float duration = 0.2f;
    float delay = 0.0f;
    int iterations = 1;
    int direction = 0;
    bool spring = false;
    float damping_ratio = 0.7f;
    float period = 0.35f;
    float initial_velocity = 0.0f;
    bool linear = false;
    float easing_x1 = 0.25f;
    float easing_y1 = 0.1f;
    float easing_x2 = 0.25f;
    float easing_y2 = 1.0f;
};

struct layer_animation_spec {
    std::vector<layer_animation_track> tracks;
};

}
