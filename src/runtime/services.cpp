#include "runtime/services.h"

#include <windows.h>

#include <imm.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>

#include "gfx/text_engine.h"
#include "platform/utf8_win32.h"

namespace flux::runtime {

core_services& services() {
    static core_services instance;
    return instance;
}

core_services::core_services() {
    ui.attach_draw_list(&draw);
}

void core_services::reset_ui() {
    ui = flux::ui::ui_context();
    ui.attach_draw_list(&draw);
}

void core_services::set_window(native_window_handle hwnd) {
    window = hwnd;
}

void core_services::set_viewport(int logical_width, int logical_height, float dpi_scale) {
    width = std::max(0, logical_width);
    height = std::max(0, logical_height);
    dpi = dpi_scale > 0.0f ? dpi_scale : 1.0f;
}

void core_services::begin_frame(float delta_time, bool animations) {
    draw.clear();
    ui.attach_draw_list(&draw);
    ui.begin_frame(delta_time, animations);
}

void core_services::end_frame() {
    ui.end_frame();
}

bool core_services::set_font_family(std::string_view utf8) {
    return flux::gfx::text_engine_instance().set_font_family_utf8(utf8);
}

std::string core_services::font_family() const {
    return flux::platform::wide_to_utf8(flux::gfx::text_engine_instance().font_family());
}

bool core_services::measure_text(const char* utf8, float size, float& width, float& height) {
    return flux::gfx::text_engine_instance().measure(utf8, size, width, height);
}

bool core_services::measure_text(const char* utf8, float size, const flux::text::text_options& options,
                              float& width, float& height) {
    return flux::gfx::text_engine_instance().measure(utf8, size, options, width, height);
}

bool core_services::measure_text(const char* utf8, float size, const flux::text::text_options& options,
                              float& width, float& height, int& line_count, int& overflow) {
    return flux::gfx::text_engine_instance().measure(utf8, size, options, width, height,
                                          line_count, overflow);
}

bool core_services::caret_metrics_text(const char* utf8, float size, const flux::text::text_options& options,
                                    int byte_offset, float& x, float& y, float& height) {
    return flux::gfx::text_engine_instance().caret_metrics(utf8, size, options, byte_offset, x, y, height);
}

int core_services::hit_test_text(const char* utf8, float size, const flux::text::text_options& options,
                              float x, float y) {
    return flux::gfx::text_engine_instance().hit_test(utf8, size, options, x, y);
}

std::string core_services::clipboard_text() const {
    HWND hwnd = reinterpret_cast<HWND>(window);
    if (hwnd == nullptr || !OpenClipboard(hwnd)) {
        return std::string();
    }
    std::string result;
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle != nullptr) {
        const wchar_t* data = static_cast<const wchar_t*>(GlobalLock(handle));
        if (data != nullptr) {
            result = flux::platform::wide_to_utf8(data, wcslen(data));
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    return result;
}

bool core_services::set_clipboard_text(std::string_view text) {
    HWND hwnd = reinterpret_cast<HWND>(window);
    if (hwnd == nullptr) {
        return false;
    }
    const std::wstring wide = flux::platform::utf8_to_wide(text);
    const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
        return false;
    }
    void* target = GlobalLock(memory);
    if (target == nullptr) {
        GlobalFree(memory);
        return false;
    }
    std::memcpy(target, wide.c_str(), bytes);
    GlobalUnlock(memory);
    if (!OpenClipboard(hwnd)) {
        GlobalFree(memory);
        return false;
    }
    EmptyClipboard();
    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

void core_services::cancel_ime() {
    HWND hwnd = reinterpret_cast<HWND>(window);
    if (hwnd == nullptr) {
        return;
    }
    HIMC context = ImmGetContext(hwnd);
    if (context != nullptr) {
        ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
        ImmReleaseContext(hwnd, context);
    }
}

void core_services::update_ime_window(float x, float y, float height, float font_size) {
    HWND hwnd = reinterpret_cast<HWND>(window);
    if (hwnd == nullptr) {
        return;
    }
    HIMC context = ImmGetContext(hwnd);
    if (context == nullptr) {
        return;
    }
    const LONG px = static_cast<LONG>(std::lround(x * dpi));
    const LONG py = static_cast<LONG>(std::lround(y * dpi));

    COMPOSITIONFORM composition = {};
    composition.dwStyle = CFS_POINT;
    composition.ptCurrentPos.x = px;
    composition.ptCurrentPos.y = py;
    ImmSetCompositionWindow(context, &composition);

    CANDIDATEFORM candidate = {};
    candidate.dwIndex = 0;
    candidate.dwStyle = CFS_CANDIDATEPOS;
    candidate.ptCurrentPos.x = px;
    candidate.ptCurrentPos.y = py + static_cast<LONG>(std::lround(height * dpi));
    ImmSetCandidateWindow(context, &candidate);

    LOGFONTW font = {};
    font.lfHeight = -static_cast<LONG>(std::lround(font_size * dpi));
    font.lfWeight = FW_NORMAL;
    const std::wstring& family = flux::gfx::text_engine_instance().font_family();
    if (!family.empty()) {
        wcsncpy_s(font.lfFaceName, family.c_str(), _TRUNCATE);
    }
    ImmSetCompositionFontW(context, &font);

    ImmReleaseContext(hwnd, context);
}

}
