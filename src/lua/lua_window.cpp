#include <lua.hpp>

#include <cstring>
#include <string>
#include <vector>

#include "app/context.h"
#include "gfx/backdrop.h"
#include "lua/bindings.h"
#include "platform/utf8_win32.h"
#include "app/win32_window.h"

namespace flux::lua {

namespace {

int opt_integer(lua_State* L, int index, const char* key, int fallback) {
    lua_getfield(L, index, key);
    int result = fallback;
    if (lua_isinteger(L, -1)) {
        result = static_cast<int>(lua_tointeger(L, -1));
    } else if (lua_isnumber(L, -1)) {
        result = static_cast<int>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);
    return result;
}

int l_create(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    flux::app::options options;
    lua_getfield(L, 1, "title");
    if (lua_isstring(L, -1)) {
        size_t length = 0;
        const char* title = lua_tolstring(L, -1, &length);
        options.title = flux::platform::utf8_to_wide(title, length);
    }
    lua_pop(L, 1);

    options.width = opt_integer(L, 1, "width", options.width);
    options.height = opt_integer(L, 1, "height", options.height);
    options.titlebar_height = opt_integer(L, 1, "titlebar_height", options.titlebar_height);

    lua_getfield(L, 1, "backdrop");
    if (lua_isstring(L, -1)) {
        const char* value = lua_tostring(L, -1);
        if (std::strcmp(value, "none") == 0) {
            options.backdrop = flux::gfx::backdrop_mode::none;
        } else if (std::strcmp(value, "mica") == 0) {
            options.backdrop = flux::gfx::backdrop_mode::mica;
        } else if (std::strcmp(value, "acrylic") == 0) {
            options.backdrop = flux::gfx::backdrop_mode::acrylic;
        } else {
            lua_pop(L, 1);
            lua_pushboolean(L, 0);
            lua_pushstring(L, "unknown backdrop (expected none, mica or acrylic)");
            return 2;
        }
    } else if (!lua_isnil(L, -1) && !lua_isnone(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 0);
        lua_pushstring(L, "backdrop must be a string");
        return 2;
    }
    lua_pop(L, 1);

    std::string error;
    if (!flux::app::create(options, error)) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_show(lua_State* L) {
    flux::app::show();
    return 0;
}

int l_pump(lua_State* L) {
    lua_pushboolean(L, flux::app::pump() ? 1 : 0);
    return 1;
}

int l_begin_frame(lua_State* L) {
    const bool force_draw = lua_toboolean(L, 1) != 0;
    lua_pushboolean(L, flux::app::begin_frame(force_draw) ? 1 : 0);
    return 1;
}

int l_end_frame(lua_State* L) {
    const bool ok = flux::app::end_frame();
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok) {
        const flux::app::app_state& state = flux::app::current();
        const std::string& error = state.last_error.empty()
            ? state.renderer.last_error() : state.last_error;
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    return 1;
}

int l_size(lua_State* L) {
    lua_pushinteger(L, flux::app::width());
    lua_pushinteger(L, flux::app::height());
    return 2;
}

int l_pixel_size(lua_State* L) {
    HWND hwnd = flux::app::current().hwnd;
    if (hwnd != nullptr) {
        RECT client;
        GetClientRect(hwnd, &client);
        lua_pushinteger(L, client.right - client.left);
        lua_pushinteger(L, client.bottom - client.top);
        return 2;
    }
    lua_pushinteger(L, flux::app::current().renderer.pixel_width());
    lua_pushinteger(L, flux::app::current().renderer.pixel_height());
    return 2;
}

int l_dpi_scale(lua_State* L) {
    lua_pushnumber(L, flux::app::dpi_scale());
    return 1;
}

int l_backdrop(lua_State* L) {
    flux::gfx::backdrop_mode mode = flux::app::backdrop();
    const char* name = "none";
    if (mode == flux::gfx::backdrop_mode::mica) {
        name = flux::app::backdrop_native() ? "mica" : "mica_host";
    } else if (mode == flux::gfx::backdrop_mode::acrylic) {
        name = "acrylic";
    }
    lua_pushstring(L, name);
    return 1;
}

int l_backdrop_native(lua_State* L) {
    lua_pushboolean(L, flux::app::backdrop_native() ? 1 : 0);
    return 1;
}

int l_last_error(lua_State* L) {
    const flux::app::app_state& state = flux::app::current();
    const std::string& error = state.last_error.empty()
        ? state.renderer.last_error() : state.last_error;
    lua_pushlstring(L, error.data(), error.size());
    return 1;
}

int l_set_titlebar_client_rects(lua_State* L) {
    std::vector<float> values;
    if (lua_istable(L, 1)) {
        const int count = static_cast<int>(lua_rawlen(L, 1));
        values.reserve(static_cast<size_t>(count) * 4);
        for (int i = 1; i <= count; ++i) {
            lua_rawgeti(L, 1, i);
            if (lua_istable(L, -1)) {
                const char* keys[] = { "x", "y", "w", "h" };
                for (const char* key : keys) {
                    lua_getfield(L, -1, key);
                    values.push_back(static_cast<float>(lua_tonumber(L, -1)));
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }
    }
    flux::app::set_titlebar_client_rects(
        values.empty() ? nullptr : values.data(),
        static_cast<int>(values.size() / 4));
    return 0;
}

int l_software(lua_State* L) {
    lua_pushboolean(L, flux::app::current().renderer.software() ? 1 : 0);
    return 1;
}

int l_remote_session(lua_State* L) {
    lua_pushboolean(L, GetSystemMetrics(SM_REMOTESESSION) != 0 ? 1 : 0);
    return 1;
}

int l_gfx_limited(lua_State* L) {
    const bool limited = flux::app::current().renderer.software()
        || GetSystemMetrics(SM_REMOTESESSION) != 0;
    lua_pushboolean(L, limited ? 1 : 0);
    return 1;
}

int l_request_frame(lua_State* L) {
    (void)L;
    flux::app::request_frame();
    return 0;
}

int l_vsync(lua_State* L) {
    lua_pushboolean(L, flux::app::vsync() ? 1 : 0);
    return 1;
}

int l_set_vsync(lua_State* L) {
    flux::app::set_vsync(lua_toboolean(L, 1) != 0);
    return 0;
}

int l_sleep(lua_State* L) {
    const int milliseconds = static_cast<int>(luaL_optinteger(L, 1, 16));
    flux::app::sleep(milliseconds);
    return 0;
}

int l_frame_timings(lua_State* L) {
    double render_ms = 0.0;
    double wait_ms = 0.0;
    flux::app::frame_timings(render_ms, wait_ms);
    lua_pushnumber(L, render_ms);
    lua_pushnumber(L, wait_ms);
    return 2;
}

int l_close_rect(lua_State* L) {
    int bar = flux::app::titlebar_height();
    lua_pushinteger(L, flux::app::width() - bar);
    lua_pushinteger(L, 0);
    lua_pushinteger(L, bar);
    lua_pushinteger(L, bar);
    return 4;
}

int l_shutdown(lua_State* L) {
    flux::app::shutdown();
    return 0;
}

}

void push_window_funcs(lua_State* L) {
    static const luaL_Reg functions[] = {
        { "create", l_create },
        { "show", l_show },
        { "pump", l_pump },
        { "request_frame", l_request_frame },
        { "sleep", l_sleep },
        { "vsync", l_vsync },
        { "set_vsync", l_set_vsync },
        { "begin_frame", l_begin_frame },
        { "end_frame", l_end_frame },
        { "size", l_size },
        { "pixel_size", l_pixel_size },
        { "dpi_scale", l_dpi_scale },
        { "backdrop", l_backdrop },
        { "backdrop_native", l_backdrop_native },
        { "software", l_software },
        { "remote_session", l_remote_session },
        { "gfx_limited", l_gfx_limited },
        { "last_error", l_last_error },
        { "set_titlebar_client_rects", l_set_titlebar_client_rects },
        { "frame_timings", l_frame_timings },
        { "close_rect", l_close_rect },
        { "shutdown", l_shutdown },
        { nullptr, nullptr },
    };
    luaL_setfuncs(L, functions, 0);
}

}
