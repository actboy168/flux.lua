#include <lua.hpp>

#ifndef FLUX_ENABLE_TESTING
#include "lua/bindings.h"

namespace flux::lua {

void push_offscreen_table(lua_State*) {}

}

#else

#include <string>

#include "app/context.h"
#include "gfx/offscreen_renderer.h"
#include "text/charconv.h"
#include "widgets/widget_runtime.h"
#include "widgets/widgets.h"
#include "lua/bindings.h"
#include "lua/registration.h"

namespace flux::lua {

namespace {

flux::gfx::offscreen_renderer g_offscreen;

int l_offscreen_create(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    int width = 640;
    int height = 480;
    float dpi = 1.0f;
    uint32_t background = 0xFF000000u;

    lua_getfield(L, 1, "width");
    if (lua_isinteger(L, -1)) width = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "height");
    if (lua_isinteger(L, -1)) height = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "dpi");
    if (lua_isnumber(L, -1)) dpi = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, 1, "background");
    if (lua_isinteger(L, -1)) background = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    std::string error;
    if (!g_offscreen.init(width, height, dpi, error)) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    flux::runtime::services().reset_ui();
    flux::runtime::services().set_viewport(width, height, dpi);
    flux::app::current().background = background;
    lua_pushboolean(L, 1);
    return 1;
}

int l_offscreen_destroy(lua_State* L) {
    (void)L;
    g_offscreen.shutdown();
    return 0;
}

int l_offscreen_begin_frame(lua_State* L) {
    if (!g_offscreen.ready()) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "offscreen renderer is not initialized");
        return 2;
    }
    float delta = static_cast<float>(luaL_optnumber(L, 1, 1.0 / 60.0));
    flux::app::app_state& state = flux::app::current();
    state.services.begin_frame(delta, state.renderer.animations_enabled());
    flux::widgets::widget_frame_begin();
    lua_pushboolean(L, 1);
    return 1;
}

int l_offscreen_end_frame(lua_State* L) {
    if (!g_offscreen.ready()) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "offscreen renderer is not initialized");
        return 2;
    }
    flux::app::app_state& state = flux::app::current();
    flux::widgets::flush_tooltips();
    std::string widget_error;
    const bool widgets_ok = flux::widgets::widget_frame_end(widget_error);
    bool ok = g_offscreen.frame(state.draw, state.background, g_offscreen.dpi_scale());
    state.services.end_frame();
    if (!ok) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "offscreen frame failed");
        return 2;
    }
    if (!widgets_ok) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, widget_error.data(), widget_error.size());
        return 2;
    }
    if (!state.services.ui.frame_error().empty()) {
        lua_pushboolean(L, 0);
        const std::string& error = state.services.ui.frame_error();
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_offscreen_save(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::string error;
    if (!g_offscreen.save_png(path, error)) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_offscreen_hash(lua_State* L) {
    uint64_t value = 0;
    std::string error;
    if (!g_offscreen.hash(value, error)) {
        lua_pushnil(L);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    char hex[16];
    const char* end = flux::text::uint64_to_hex(value, hex);
    lua_pushlstring(L, hex, static_cast<size_t>(end - hex));
    return 1;
}

int l_offscreen_pixel(lua_State* L) {
    int x = static_cast<int>(luaL_checkinteger(L, 1));
    int y = static_cast<int>(luaL_checkinteger(L, 2));
    lua_pushinteger(L, g_offscreen.pixel(x, y));
    return 1;
}

int l_offscreen_compare(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint64_t differing = 0;
    int max_diff = 0;
    std::string error;
    if (!g_offscreen.compare_png(path, differing, max_diff, error)) {
        lua_pushnil(L);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(differing));
    lua_pushinteger(L, max_diff);
    lua_pushinteger(L, static_cast<lua_Integer>(g_offscreen.pixel_width())
        * static_cast<lua_Integer>(g_offscreen.pixel_height()));
    return 3;
}

int l_offscreen_dpi_scale(lua_State* L) {
    lua_pushnumber(L, g_offscreen.dpi_scale());
    return 1;
}

int l_offscreen_pixel_size(lua_State* L) {
    lua_pushinteger(L, g_offscreen.pixel_width());
    lua_pushinteger(L, g_offscreen.pixel_height());
    return 2;
}

int l_offscreen_size(lua_State* L) {
    lua_pushinteger(L, g_offscreen.logical_width());
    lua_pushinteger(L, g_offscreen.logical_height());
    return 2;
}

}

void push_offscreen_table(lua_State* L) {
    static const luaL_Reg functions[] = {
        { "create", l_offscreen_create },
        { "destroy", l_offscreen_destroy },
        { "begin_frame", l_offscreen_begin_frame },
        { "end_frame", l_offscreen_end_frame },
        { "save", l_offscreen_save },
        { "hash", l_offscreen_hash },
        { "pixel", l_offscreen_pixel },
        { "compare", l_offscreen_compare },
        { "dpi_scale", l_offscreen_dpi_scale },
        { "pixel_size", l_offscreen_pixel_size },
        { "size", l_offscreen_size },
        { nullptr, nullptr },
    };
    register_table(L, "offscreen", functions);
}

}

#endif
