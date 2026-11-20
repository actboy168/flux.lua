#pragma once

#include <windows.h>

#include <string>

#include "gfx/backdrop.h"

namespace flux::app {

struct options {
    std::wstring title = L"flux";
    int width = 560;
    int height = 540;
    int titlebar_height = 34;
    flux::gfx::backdrop_mode backdrop = flux::gfx::backdrop_mode::mica;
};

bool create(const options& options, std::string& error);
void show();
bool pump();
void request_frame();
void sleep(int milliseconds = 16);
bool vsync();
void set_vsync(bool enabled);
bool begin_frame(bool force_draw = false);
bool end_frame();
void shutdown();

float dpi_scale();
int width();
int height();
int titlebar_height();
flux::gfx::backdrop_mode backdrop();
bool backdrop_native();

// Logical-coordinate rectangles in the native titlebar that should behave as
// client area (e.g. clickable task chip), so WM_NCHITTEST returns HTCLIENT.
void set_titlebar_client_rects(const float* rects, int count);

bool debug_lose_device(std::string& error);
void debug_set_dpi_scale(float scale);
void frame_timings(double& render_ms, double& wait_ms);

}
