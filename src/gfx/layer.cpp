#include "gfx/layer.h"

#include <cstring>

namespace flux::gfx {

const char* layer_property_name(layer_property property) {
    switch (property) {
    case layer_property::opacity: return "opacity";
    case layer_property::offset: return "offset";
    case layer_property::scale: return "scale";
    case layer_property::rotation: return "rotation";
    default: return "unknown";
    }
}

bool parse_layer_property(const char* name, layer_property& property) {
    if (name == nullptr) {
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        const layer_property candidate = static_cast<layer_property>(i);
        if (std::strcmp(name, layer_property_name(candidate)) == 0) {
            property = candidate;
            return true;
        }
    }
    return false;
}

}
