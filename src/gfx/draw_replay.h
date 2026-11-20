#pragma once

#include <d2d1.h>

#include <string_view>
#include <vector>

#include "ui/draw_list.h"

namespace flux::gfx {

void replay_commands(ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush,
                     const std::vector<flux::ui::draw_cmd>& commands,
                     std::string_view text_arena, float scale);

void replay_draw_list(ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush,
                      const flux::ui::draw_list& draw_list, uint32_t background, float scale);

}
