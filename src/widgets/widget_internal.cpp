#include "widgets/widget_internal.h"

#include "runtime/services.h"

namespace flux::widgets {

widget_runtime_state& wr() {
    return widget_runtime();
}

void draw_text_block(float x, float y, float width, float height, const char* text,
                     uint32_t color, float size, int align, int valign,
                     bool wrap, bool ellipsis, int max_lines, float line_height) {
    flux::text::text_options options;
    options.width = width;
    options.height = height;
    options.line_height = line_height;
    options.max_lines = max_lines;
    options.align = align;
    options.valign = valign;
    options.wrap = wrap;
    options.ellipsis = ellipsis;
    flux::runtime::services().draw.text_options(
        x, y, text != nullptr ? text : "", color, size,
        options.width, options.height, options.line_height, options.max_lines,
        options.align, options.valign, options.weight, options.wrap, options.ellipsis);
}

}
