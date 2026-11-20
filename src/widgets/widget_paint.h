#pragma once

#include <cstdint>
#include <string>

#include "ui/ui_context.h"
#include "widgets/widget_context.h"

namespace flux::widgets {

void draw_focus_ring(float x, float y, float width, float height, float radius,
                     uint32_t color, float alpha);
void draw_focus_ring_inset(float x, float y, float width, float height, float radius,
                           uint32_t color, float alpha, float inset);
void draw_bottom_elevation(float x, float y, float width, float height, float radius,
                           uint32_t color, float strength);

}
