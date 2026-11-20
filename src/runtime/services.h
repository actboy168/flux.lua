#pragma once

#include <string>
#include <string_view>

#include "ui/draw_list.h"
#include "text/text_types.h"
#include "ui/ui_context.h"

namespace flux::runtime {

struct native_window;
using native_window_handle = native_window*;

class core_services {
public:
    core_services();

    flux::ui::ui_context ui;
    flux::ui::draw_list draw;

    int width = 0;
    int height = 0;
    native_window_handle window = nullptr;
    float dpi = 1.0f;

    void reset_ui();
    void set_window(native_window_handle hwnd);
    void set_viewport(int logical_width, int logical_height, float dpi_scale);
    void begin_frame(float delta_time, bool animations);
    void end_frame();

    bool set_font_family(std::string_view utf8);
    std::string font_family() const;

    bool measure_text(const char* utf8, float size, float& width, float& height);
    bool measure_text(const char* utf8, float size, const flux::text::text_options& options,
                      float& width, float& height);
    bool measure_text(const char* utf8, float size, const flux::text::text_options& options,
                      float& width, float& height, int& line_count, int& overflow);
    bool caret_metrics_text(const char* utf8, float size, const flux::text::text_options& options,
                            int byte_offset, float& x, float& y, float& height);
    int hit_test_text(const char* utf8, float size, const flux::text::text_options& options,
                      float x, float y);

    std::string clipboard_text() const;
    bool set_clipboard_text(std::string_view text);
    void cancel_ime();
    void update_ime_window(float x, float y, float height, float font_size);
};

core_services& services();

}
