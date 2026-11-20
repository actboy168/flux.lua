#include "widgets/widget_paint.h"

#include "runtime/services.h"
#include "ui/pixel.h"
#include "widgets/control_anim.h"

namespace flux::widgets {

void draw_focus_ring(float x, float y, float width, float height, float radius,
                     uint32_t color, float alpha) {
    if (!widget_ui().focus_visible() || alpha <= 0.002f) {
        return;
    }
    const float dpi = flux::runtime::services().dpi;
    float ox = x - 2.0f;
    float oy = y - 2.0f;
    float ow = width + 4.0f;
    float oh = height + 4.0f;
    flux::ui::pixel::snap_rect(ox, oy, ow, oh, dpi);
    flux::runtime::services().draw.rect(ox, oy, ow, oh, with_alpha(color, alpha * 0.55f),
                     radius + 2.0f, 2.0f);

    float ix = x - 1.0f;
    float iy = y - 1.0f;
    float iw = width + 2.0f;
    float ih = height + 2.0f;
    flux::ui::pixel::snap_rect(ix, iy, iw, ih, dpi);
    flux::runtime::services().draw.rect(ix, iy, iw, ih, with_alpha(color, alpha),
                     radius + 1.0f, 1.0f);
}

void draw_focus_ring_inset(float x, float y, float width, float height, float radius,
                           uint32_t color, float alpha, float inset) {
    if (!widget_ui().focus_visible() || alpha <= 0.002f) {
        return;
    }
    const float dpi = flux::runtime::services().dpi;
    x += inset;
    y += inset;
    width = std::max(0.0f, width - inset * 2.0f);
    height = std::max(0.0f, height - inset * 2.0f);

    float ox = x - 1.0f;
    float oy = y - 1.0f;
    float ow = width + 2.0f;
    float oh = height + 2.0f;
    flux::ui::pixel::snap_rect(ox, oy, ow, oh, dpi);
    flux::runtime::services().draw.rect(ox, oy, ow, oh, with_alpha(color, alpha * 0.55f),
                     std::max(0.0f, radius), 1.0f);

    float ix = x;
    float iy = y;
    float iw = width;
    float ih = height;
    flux::ui::pixel::snap_rect(ix, iy, iw, ih, dpi);
    flux::runtime::services().draw.rect(ix, iy, iw, ih, with_alpha(color, alpha),
                     std::max(0.0f, radius), 1.0f);
}

void draw_bottom_elevation(float x, float y, float width, float height, float radius,
                           uint32_t color, float strength) {
    if (strength <= 0.004f) {
        return;
    }
    const float dpi = flux::runtime::services().dpi;
    flux::ui::pixel::snap_rect(x, y, width, height, dpi);
    const float inset = std::max(1.0f, radius * 0.5f);
    flux::runtime::services().draw.rect_filled(
        x + inset, y + height - 3.0f, std::max(0.0f, width - inset * 2.0f), 1.0f,
        with_alpha(color, 0.20f * strength), 0.0f);
    flux::runtime::services().draw.rect_filled(
        x + inset * 0.5f, y + height - 2.0f, std::max(0.0f, width - inset), 1.0f,
        with_alpha(color, 0.12f * strength), 0.0f);
    flux::runtime::services().draw.rect_filled(
        x + inset * 0.25f, y + height - 1.0f, std::max(0.0f, width - inset * 0.5f), 1.0f,
        with_alpha(color, 0.07f * strength), 0.0f);
}

}
