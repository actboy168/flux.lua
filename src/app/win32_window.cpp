#include "app/win32_window.h"

#include <dcomp.h>
#include <dwmapi.h>
#include <imm.h>
#include <shellscalingapi.h>
#include <timeapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "app/context.h"
#include "text/utf8.h"
#include "platform/os_win32.h"
#include "platform/utf8_win32.h"
#include "widgets/widget_runtime.h"
#include "widgets/widgets.h"

namespace flux::app {

namespace {

constexpr wchar_t k_class_name[] = L"FluxWindow";
constexpr DWORD k_dark_mode_attribute = 20;
constexpr DWORD k_corner_preference_attribute = 33;
constexpr DWORD k_host_backdrop_brush_attribute = 17;
constexpr DWORD k_system_backdrop_type_attribute = 38;
constexpr int k_corner_round = 2;
constexpr int k_system_backdrop_main_window = 2;

bool g_console_handler_installed = false;

BOOL WINAPI console_ctrl_handler(DWORD type) {
    if (type != CTRL_C_EVENT && type != CTRL_BREAK_EVENT) {
        return FALSE;
    }
    HWND hwnd = flux::app::current().hwnd;
    if (hwnd == nullptr) {
        return FALSE;
    }
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

void ensure_dpi_awareness() {
    if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        return;
    }
    if (SUCCEEDED(SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE))) {
        return;
    }
    SetProcessDPIAware();
}

bool system_backdrop_for(flux::gfx::backdrop_mode mode) {
    return mode == flux::gfx::backdrop_mode::mica && flux::platform::supports_system_backdrop();
}

void setup_dwm(HWND hwnd, flux::gfx::backdrop_mode mode) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, k_dark_mode_attribute, &dark, sizeof(dark));

    int corner = k_corner_round;
    DwmSetWindowAttribute(hwnd, k_corner_preference_attribute, &corner, sizeof(corner));

    const bool native_backdrop = system_backdrop_for(mode);
    if (mode != flux::gfx::backdrop_mode::none) {
        BOOL use_host_brush = (mode == flux::gfx::backdrop_mode::acrylic || !native_backdrop) ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, k_host_backdrop_brush_attribute, &use_host_brush, sizeof(use_host_brush));
    }

    if (native_backdrop) {
        int type = k_system_backdrop_main_window;
        DwmSetWindowAttribute(hwnd, k_system_backdrop_type_attribute, &type, sizeof(type));
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
}

int titlebar_pixels() {
    float scale = flux::app::current().services.dpi;
    return static_cast<int>(std::lround(flux::app::current().titlebar_height * scale));
}

struct titlebar_client_rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

std::vector<titlebar_client_rect> g_titlebar_client_rects;

bool point_in_titlebar_client_rect(int x, int y) {
    const float scale = flux::app::current().services.dpi > 0.0f ? flux::app::current().services.dpi : 1.0f;
    for (const titlebar_client_rect& rect : g_titlebar_client_rects) {
        const int rx = static_cast<int>(std::lround(rect.x * scale));
        const int ry = static_cast<int>(std::lround(rect.y * scale));
        const int rw = static_cast<int>(std::lround(rect.w * scale));
        const int rh = static_cast<int>(std::lround(rect.h * scale));
        if (x >= rx && x < rx + rw && y >= ry && y < ry + rh) {
            return true;
        }
    }
    return false;
}

bool point_in_close_button(HWND hwnd, int x, int y) {
    RECT client;
    GetClientRect(hwnd, &client);
    int bar = titlebar_pixels();
    return x >= client.right - bar && x < client.right && y >= 0 && y < bar;
}

void update_modifiers(flux::ui::input_state& input) {
    input.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    input.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    input.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
}

void set_mouse_from_client(app_state& state, LPARAM lparam) {
    float scale = state.services.dpi > 0.0f ? state.services.dpi : 1.0f;
    state.ui.input.mouse_x = static_cast<float>(GET_X_LPARAM(lparam)) / scale;
    state.ui.input.mouse_y = static_cast<float>(GET_Y_LPARAM(lparam)) / scale;
    state.ui.input.mouse_inside = true;
}

void set_mouse_from_screen(app_state& state, HWND hwnd, LPARAM lparam) {
    POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
    ScreenToClient(hwnd, &pt);
    float scale = state.services.dpi > 0.0f ? state.services.dpi : 1.0f;
    state.ui.input.mouse_x = static_cast<float>(pt.x) / scale;
    state.ui.input.mouse_y = static_cast<float>(pt.y) / scale;
    state.ui.input.mouse_inside = true;
}

void record_key(flux::ui::input_state& input, int vk, bool down) {
    if (vk < 0 || vk >= 256) {
        return;
    }
    const bool modifier = vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT
        || vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL
        || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU;
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
        input.shift = down;
    } else if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) {
        input.ctrl = down;
    } else if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) {
        input.alt = down;
    }
    if (down) {
        if (!input.key_down[vk]) {
            input.key_pressed[vk] = true;
            if (!modifier) {
                input.key_ctrl[vk] = input.ctrl;
                input.key_shift[vk] = input.shift;
                input.key_alt[vk] = input.alt;
            }
        }
        input.key_down[vk] = true;
    } else {
        if (input.key_down[vk]) {
            input.key_released[vk] = true;
        }
        input.key_down[vk] = false;
    }
}

void append_utf8(flux::ui::input_state& input, const wchar_t* data, int length) {
    if (data == nullptr || length <= 0) {
        return;
    }
    const int count = WideCharToMultiByte(
        CP_UTF8, 0, data, length, nullptr, 0, nullptr, nullptr);
    if (count <= 0) {
        return;
    }
    std::string buffer(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, data, length, buffer.data(), count, nullptr, nullptr);
    input.text += buffer;
}

std::wstring ime_string(HWND hwnd, DWORD flag) {
    HIMC context = ImmGetContext(hwnd);
    if (context == nullptr) {
        return std::wstring();
    }
    const LONG bytes = ImmGetCompositionStringW(context, flag, nullptr, 0);
    std::wstring result;
    if (bytes > 0) {
        result.resize(static_cast<size_t>(bytes) / sizeof(wchar_t));
        const LONG copied = ImmGetCompositionStringW(
            context, flag, result.data(), static_cast<DWORD>(bytes));
        if (copied > 0) {
            result.resize(static_cast<size_t>(copied) / sizeof(wchar_t));
        } else {
            result.clear();
        }
    }
    ImmReleaseContext(hwnd, context);
    return result;
}

void update_ime_composition(flux::ui::input_state& input, HWND hwnd, LPARAM flags) {
    if ((flags & GCS_RESULTSTR) != 0) {
        const std::wstring result = ime_string(hwnd, GCS_RESULTSTR);
        if (!result.empty()) {
            append_utf8(input, result.data(), static_cast<int>(result.size()));
        }
        input.pending_high_surrogate = 0;
        input.ime_composition.clear();
        input.ime_cursor = 0;
        input.ime_composing = false;
    }
    if ((flags & GCS_COMPSTR) != 0) {
        const std::wstring composition = ime_string(hwnd, GCS_COMPSTR);
        input.ime_composition = flux::platform::wide_to_utf8(composition);
        if (input.ime_composition.empty()) {
            input.ime_cursor = 0;
            input.ime_composing = false;
        } else {
            HIMC context = ImmGetContext(hwnd);
            const LONG cursor = context != nullptr
                ? static_cast<LONG>(ImmGetCompositionStringW(context, GCS_CURSORPOS, nullptr, 0))
                : 0;
            if (context != nullptr) {
                ImmReleaseContext(hwnd, context);
            }
            input.ime_cursor = static_cast<int>(
                flux::text::utf16_to_utf8_offset(input.ime_composition, static_cast<size_t>(std::max<LONG>(cursor, 0))));
            input.ime_composing = true;
        }
    }
}

void track_mouse_leave(HWND hwnd) {
    TRACKMOUSEEVENT track = {};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = hwnd;
    TrackMouseEvent(&track);
}

uint64_t performance_frequency() {
    static const uint64_t frequency = [] {
        LARGE_INTEGER value = {};
        QueryPerformanceFrequency(&value);
        return static_cast<uint64_t>(value.QuadPart);
    }();
    return frequency;
}

uint64_t performance_counter() {
    LARGE_INTEGER value = {};
    QueryPerformanceCounter(&value);
    return static_cast<uint64_t>(value.QuadPart);
}

double ticks_to_seconds(uint64_t ticks) {
    const uint64_t frequency = performance_frequency();
    return frequency != 0
        ? static_cast<double>(ticks) / static_cast<double>(frequency)
        : 0.0;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    app_state& state = flux::app::current();
    flux::ui::input_state& input = state.ui.input;

    if (message == flux::gfx::k_animation_completed_message) {
        const uint64_t id = static_cast<uint64_t>(static_cast<uint32_t>(wparam))
            | (static_cast<uint64_t>(static_cast<uint32_t>(lparam)) << 32);
        if (state.hwnd == hwnd) {
            state.renderer.on_animation_completed(id);
        }
        return 0;
    }

    switch (message) {
    case WM_NCCALCSIZE:
        if (wparam) {
            return 0;
        }
        break;
    case WM_NCACTIVATE:
        return TRUE;
    case WM_NCHITTEST: {
        POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
        ScreenToClient(hwnd, &pt);
        int bar = titlebar_pixels();
        if (pt.y >= 0 && pt.y < bar) {
            RECT client;
            GetClientRect(hwnd, &client);
            if (pt.x >= client.right - bar && pt.x < client.right) {
                return HTCLIENT;
            }
            if (point_in_titlebar_client_rect(pt.x, pt.y)) {
                return HTCLIENT;
            }
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_NCLBUTTONDBLCLK:
        if (wparam == HTCAPTION) {
            return 0;
        }
        break;
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
        if (wparam == HTCAPTION) {
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        set_mouse_from_client(state, lparam);
        track_mouse_leave(hwnd);
        return 0;
    case WM_MOUSELEAVE:
        input.mouse_inside = false;
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        set_mouse_from_client(state, lparam);
        if (!input.mouse_down[0]) {
            input.mouse_pressed[0] = true;
        }
        input.mouse_down[0] = true;
        if (message == WM_LBUTTONDBLCLK) {
            input.mouse_double_clicked[0] = true;
        }
        SetCapture(hwnd);
        SetFocus(hwnd);
        return 0;
    case WM_LBUTTONUP:
        set_mouse_from_client(state, lparam);
        input.mouse_down[0] = false;
        input.mouse_released[0] = true;
        ReleaseCapture();
        if (point_in_close_button(hwnd, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))) {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        return 0;
    case WM_RBUTTONDOWN:
        set_mouse_from_client(state, lparam);
        if (!input.mouse_down[1]) {
            input.mouse_pressed[1] = true;
        }
        input.mouse_down[1] = true;
        SetCapture(hwnd);
        return 0;
    case WM_RBUTTONUP:
        set_mouse_from_client(state, lparam);
        input.mouse_down[1] = false;
        input.mouse_released[1] = true;
        ReleaseCapture();
        return 0;
    case WM_MBUTTONDOWN:
        set_mouse_from_client(state, lparam);
        if (!input.mouse_down[2]) {
            input.mouse_pressed[2] = true;
        }
        input.mouse_down[2] = true;
        SetCapture(hwnd);
        return 0;
    case WM_MBUTTONUP:
        set_mouse_from_client(state, lparam);
        input.mouse_down[2] = false;
        input.mouse_released[2] = true;
        ReleaseCapture();
        return 0;
    case WM_MOUSEWHEEL:
        set_mouse_from_screen(state, hwnd, lparam);
        input.wheel += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
        return 0;
    case WM_KEYDOWN:
        record_key(input, static_cast<int>(wparam), true);
        if (wparam == VK_ESCAPE && !state.ui.modal() && !input.ime_composing
            && !state.ui.edit_focused()) {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
        return 0;
    case WM_KEYUP:
        record_key(input, static_cast<int>(wparam), false);
        return 0;
    case WM_SYSKEYDOWN:
        record_key(input, static_cast<int>(wparam), true);
        if (wparam == VK_F4) {
            break;
        }
        return 0;
    case WM_SYSKEYUP:
        record_key(input, static_cast<int>(wparam), false);
        break;
    case WM_CHAR: {
        const wchar_t ch = static_cast<wchar_t>(wparam);
        if (ch >= 0xD800 && ch <= 0xDBFF) {
            input.pending_high_surrogate = ch;
            return 0;
        }
        if (ch >= 0xDC00 && ch <= 0xDFFF) {
            if (input.pending_high_surrogate != 0) {
                const wchar_t pair[2] = {
                    static_cast<wchar_t>(input.pending_high_surrogate),
                    ch,
                };
                append_utf8(input, pair, 2);
                input.pending_high_surrogate = 0;
            }
            return 0;
        }
        input.pending_high_surrogate = 0;
        if (ch >= 0x20) {
            append_utf8(input, &ch, 1);
        }
        return 0;
    }
    case WM_IME_STARTCOMPOSITION:
        input.ime_composition.clear();
        input.ime_cursor = 0;
        input.ime_composing = true;
        return 0;
    case WM_IME_COMPOSITION:
        update_ime_composition(input, hwnd, static_cast<LPARAM>(lparam));
        return 0;
    case WM_IME_ENDCOMPOSITION:
        input.ime_composition.clear();
        input.ime_cursor = 0;
        input.ime_composing = false;
        return 0;
    case WM_ACTIVATEAPP:
        input.window_active = wparam != 0;
        if (wparam == 0) {
            state.ui.clear_interaction();
            input.window_active = false;
        }
        return 0;
    case WM_KILLFOCUS:
        state.ui.clear_interaction();
        input.window_active = false;
        input.ime_composition.clear();
        input.ime_cursor = 0;
        input.ime_composing = false;
        return 0;
    case WM_SETFOCUS:
        input.window_active = true;
        update_modifiers(input);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_SYSCOMMAND:
        if ((wparam & 0xFFF0) == SC_CLOSE) {
            DestroyWindow(hwnd);
            return 0;
        }
        if ((wparam & 0xFFF0) == SC_MAXIMIZE) {
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        state.hwnd = nullptr;
        state.quit = true;
        PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED: {
        RECT* suggested = reinterpret_cast<RECT*>(lparam);
        UINT dpi = HIWORD(wparam);
        float scale = dpi / 96.0f;
        state.services.dpi = scale;
        state.services.set_viewport(state.services.width, state.services.height, scale);
        int width = static_cast<int>(std::lround(state.services.width * scale));
        int height = static_cast<int>(std::lround(state.services.height * scale));
        if (suggested != nullptr) {
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, width, height,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
        return 0;
    }
    case WM_SIZE:
        if (state.hwnd == hwnd && state.renderer.ready()) {
            int width = LOWORD(lparam);
            int height = HIWORD(lparam);
            if (width > 0 && height > 0) {
                state.renderer.resize(width, height, state.services.dpi);
            }
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool register_window_class() {
    static bool registered = false;
    if (registered) {
        return true;
    }
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = k_class_name;
    if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }
    registered = true;
    return true;
}

}

void set_titlebar_client_rects(const float* rects, int count) {
    g_titlebar_client_rects.clear();
    if (rects == nullptr || count <= 0) {
        return;
    }
    g_titlebar_client_rects.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        titlebar_client_rect rect;
        rect.x = rects[i * 4 + 0];
        rect.y = rects[i * 4 + 1];
        rect.w = rects[i * 4 + 2];
        rect.h = rects[i * 4 + 3];
        g_titlebar_client_rects.push_back(rect);
    }
}

bool create(const options& options, std::string& error) {
    app_state& state = flux::app::current();
    if (state.hwnd != nullptr) {
        return true;
    }
    g_titlebar_client_rects.clear();
    if (options.width <= 0 || options.height <= 0 || options.titlebar_height < 0) {
        error = "invalid window options";
        return false;
    }

    ensure_dpi_awareness();
    if (!register_window_class()) {
        error = "RegisterClassExW failed";
        return false;
    }

    DWORD style = WS_POPUP | WS_SYSMENU;
    DWORD ex_style = WS_EX_NOREDIRECTIONBITMAP | WS_EX_APPWINDOW;

    float scale = GetDpiForSystem() / 96.0f;
    int physical_width = static_cast<int>(std::lround(options.width * scale));
    int physical_height = static_cast<int>(std::lround(options.height * scale));
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    int origin_x = (screen_width - physical_width) / 2;
    int origin_y = (screen_height - physical_height) / 2;

    HWND hwnd = CreateWindowExW(
        ex_style,
        k_class_name,
        options.title.c_str(),
        style,
        origin_x,
        origin_y,
        physical_width,
        physical_height,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (hwnd == nullptr) {
        error = "CreateWindowExW failed";
        return false;
    }

    UINT window_dpi = GetDpiForWindow(hwnd);
    float window_scale = window_dpi / 96.0f;
    int window_width = static_cast<int>(std::lround(options.width * window_scale));
    int window_height = static_cast<int>(std::lround(options.height * window_scale));
    if (window_width != physical_width || window_height != physical_height) {
        SetWindowPos(
            hwnd,
            nullptr,
            (screen_width - window_width) / 2,
            (screen_height - window_height) / 2,
            window_width,
            window_height,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    setup_dwm(hwnd, options.backdrop);

    RECT client = {};
    GetClientRect(hwnd, &client);
    int client_width = client.right - client.left;
    int client_height = client.bottom - client.top;
    if (client_width < 1) {
        client_width = window_width;
    }
    if (client_height < 1) {
        client_height = window_height;
    }

    state.services.reset_ui();
    state.services.set_window(reinterpret_cast<flux::runtime::native_window_handle>(hwnd));
    state.services.set_viewport(options.width, options.height, window_scale);
    state.hwnd = hwnd;
    state.titlebar_height = options.titlebar_height;
    state.quit = false;
    state.backdrop = options.backdrop;
    state.background = options.backdrop == flux::gfx::backdrop_mode::none ? 0xFF1E1E1E : 0x00000000;
    state.draw.clear();
    state.has_frame_time = true;
    state.last_frame_time = performance_counter();
    state.frame_start = state.last_frame_time;
    state.ui.input.window_active = true;
    timeBeginPeriod(1);
    state.timer_period_active = true;

    if (!state.renderer.init(hwnd, client_width, client_height, window_scale,
                             options.backdrop, error)) {
        state.hwnd = nullptr;
        DestroyWindow(hwnd);
        timeEndPeriod(1);
        state.timer_period_active = false;
        return false;
    }
    if (!g_console_handler_installed) {
        g_console_handler_installed = SetConsoleCtrlHandler(console_ctrl_handler, TRUE) != FALSE;
    }
    return true;
}

void show() {
    app_state& state = flux::app::current();
    if (state.hwnd == nullptr) {
        return;
    }
    state.renderer.frame(state.draw, state.background, state.services.dpi);
    state.frame_requested = false;
    ShowWindow(state.hwnd, SW_SHOW);
    SetForegroundWindow(state.hwnd);
    state.last_frame_time = performance_counter();
    state.frame_start = state.last_frame_time;
    state.has_frame_time = true;
}

bool pump() {
    app_state& state = flux::app::current();
    MSG message;
    bool received = false;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        received = true;
        if (message.message == WM_QUIT) {
            state.quit = true;
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (received) {
        state.frame_requested = true;
    }
    return !state.quit;
}

static bool should_draw() {
    app_state& state = flux::app::current();
    if (state.hwnd == nullptr || state.quit) {
        return false;
    }
    if (state.frame_requested) {
        return true;
    }
    if (state.services.ui.has_active_animations()) {
        return true;
    }
    if (widgets::widget_runtime().animations_active(state.services.ui.frame_index())) {
        return true;
    }
    return state.renderer.animations_running();
}

void sleep(int milliseconds) {
    Sleep(static_cast<DWORD>(std::max(0, milliseconds)));
}

bool vsync() {
    return flux::app::current().renderer.vsync();
}

void set_vsync(bool enabled) {
    flux::app::current().renderer.set_vsync(enabled);
    flux::app::request_frame();
}

bool begin_frame(bool force_draw) {
    app_state& state = flux::app::current();
    if (state.hwnd == nullptr || state.quit) {
        return false;
    }
    if (!force_draw && !should_draw()) {
        return false;
    }
    const uint64_t now = performance_counter();
    float delta = 0.0f;
    if (state.has_frame_time) {
        delta = static_cast<float>(ticks_to_seconds(now - state.last_frame_time));
    }
    state.last_frame_time = now;
    state.frame_start = now;
    state.has_frame_time = true;
    state.services.begin_frame(std::min(delta, 0.1f), state.renderer.animations_enabled());
    widgets::widget_frame_begin();
    return true;
}

bool end_frame() {
    app_state& state = flux::app::current();
    if (state.hwnd == nullptr || state.quit) {
        return false;
    }
    widgets::flush_tooltips();
    std::string widget_error;
    const bool widgets_ok = widgets::widget_frame_end(widget_error);
    const uint64_t render_start = performance_counter();
    bool ok = state.renderer.frame(state.draw, state.background, state.services.dpi);
    const uint64_t render_end = performance_counter();
    if (!ok) {
        std::string error;
        ok = state.renderer.recover(error);
        if (!ok) {
            state.services.end_frame();
            state.last_error = !widget_error.empty()
                ? widget_error : state.services.ui.frame_error();
            if (state.last_error.empty()) {
                state.last_error = state.renderer.last_error();
            }
            return false;
        }
    }
    state.services.end_frame();
    state.frame_requested = false;
    state.last_error.clear();
    if (!widgets_ok) {
        state.last_error = widget_error;
    } else if (!state.services.ui.frame_error().empty()) {
        state.last_error = state.services.ui.frame_error();
    }
    const uint64_t wait_end = performance_counter();
    state.last_render_ms = ticks_to_seconds(render_end - render_start) * 1000.0;
    state.last_wait_ms = ticks_to_seconds(wait_end - render_end) * 1000.0;
    return state.last_error.empty();
}

void shutdown() {
    app_state& state = flux::app::current();
    if (g_console_handler_installed) {
        SetConsoleCtrlHandler(console_ctrl_handler, FALSE);
        g_console_handler_installed = false;
    }
    state.renderer.shutdown();
    if (state.hwnd != nullptr) {
        HWND hwnd = state.hwnd;
        state.hwnd = nullptr;
        DestroyWindow(hwnd);
    }
    if (state.timer_period_active) {
        timeEndPeriod(1);
        state.timer_period_active = false;
    }
    state.quit = true;
    state.draw.clear();
}

float dpi_scale() {
    return flux::app::current().services.dpi;
}

int width() {
    return flux::app::current().services.width;
}

int height() {
    return flux::app::current().services.height;
}

int titlebar_height() {
    return flux::app::current().titlebar_height;
}

flux::gfx::backdrop_mode backdrop() {
    return flux::app::current().backdrop;
}

bool backdrop_native() {
    return flux::app::current().renderer.backdrop_native();
}

bool debug_lose_device(std::string& error) {
    return flux::app::current().renderer.recover(error);
}

void debug_set_dpi_scale(float scale) {
    app_state& state = flux::app::current();
    if (state.hwnd == nullptr || scale <= 0.0f) {
        return;
    }
    state.services.dpi = scale;
    state.services.set_viewport(state.services.width, state.services.height, scale);
    state.frame_requested = true;
    int width = static_cast<int>(std::lround(state.services.width * scale));
    int height = static_cast<int>(std::lround(state.services.height * scale));
    SetWindowPos(state.hwnd, nullptr, 0, 0, width, height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void frame_timings(double& render_ms, double& wait_ms) {
    render_ms = flux::app::current().last_render_ms;
    wait_ms = flux::app::current().last_wait_ms;
}

}
