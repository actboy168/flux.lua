#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

#include "gfx/backdrop.h"
#include "ui/draw_list.h"
#include "runtime/services.h"
#include "ui/ui_context.h"
#include "gfx/composition_renderer.h"

namespace flux::app {

struct app_state {
    app_state();
    app_state(const app_state&) = delete;
    app_state& operator=(const app_state&) = delete;

    HWND hwnd = nullptr;
    int titlebar_height = 34;
    uint32_t background = 0x00000000;
    bool quit = false;
    flux::gfx::backdrop_mode backdrop = flux::gfx::backdrop_mode::none;
    bool timer_period_active = false;
    uint64_t frame_start = 0;
    uint64_t last_frame_time = 0;
    bool has_frame_time = false;
    double last_render_ms = 0.0;
    double last_wait_ms = 0.0;
    bool frame_requested = true;
    std::string last_error;
    flux::runtime::core_services& services;
    flux::ui::ui_context& ui;
    flux::ui::draw_list& draw;
    flux::gfx::composition_renderer renderer;
};

app_state& current();
void request_frame();
}
