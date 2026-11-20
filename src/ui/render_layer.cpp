#include "ui/render_layer.h"

#include <cstring>

namespace flux::ui {

const char* render_layer_name(render_layer layer) {
    switch (layer) {
    case render_layer::content: return "content";
    case render_layer::overlay: return "overlay";
    case render_layer::popup: return "popup";
    case render_layer::tooltip: return "tooltip";
    default: return "unknown";
    }
}

bool parse_render_layer(const char* name, render_layer& layer) {
    if (name == nullptr) {
        return false;
    }
    for (int i = 0; i < k_render_layer_count; ++i) {
        const render_layer candidate = static_cast<render_layer>(i);
        if (std::strcmp(name, render_layer_name(candidate)) == 0) {
            layer = candidate;
            return true;
        }
    }
    return false;
}

}
