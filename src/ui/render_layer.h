#pragma once

namespace flux::ui {

enum class render_layer : int {
    content = 0,
    overlay = 1,
    popup = 2,
    tooltip = 3,
    count = 4,
};

constexpr int k_render_layer_count = static_cast<int>(render_layer::count);

const char* render_layer_name(render_layer layer);
bool parse_render_layer(const char* name, render_layer& layer);

}
