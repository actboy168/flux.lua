#include <lua.hpp>

#include <cstdint>
#include <string_view>

#include "app/context.h"
#include "ui/draw_list.h"
#include "lua/bindings.h"
#include "lua/lua_string.h"

namespace flux::lua {

namespace {

int align_from_value(lua_State* L, int index) {
    if (lua_isinteger(L, index)) {
        lua_Integer value = lua_tointeger(L, index);
        return static_cast<int>(value < 0 ? 0 : (value > 2 ? 2 : value));
    }
    if (lua_isstring(L, index)) {
        const char* name = lua_tostring(L, index);
        if (std::strcmp(name, "center") == 0 || std::strcmp(name, "middle") == 0) {
            return 1;
        }
        if (std::strcmp(name, "right") == 0 || std::strcmp(name, "end") == 0
            || std::strcmp(name, "bottom") == 0) {
            return 2;
        }
    }
    return 0;
}

int weight_from_value(lua_State* L, int index, int fallback) {
    if (lua_isinteger(L, index)) {
        return static_cast<int>(lua_tointeger(L, index));
    }
    if (lua_isstring(L, index)) {
        const char* name = lua_tostring(L, index);
        if (std::strcmp(name, "normal") == 0) return 400;
        if (std::strcmp(name, "medium") == 0) return 500;
        if (std::strcmp(name, "semibold") == 0) return 600;
        if (std::strcmp(name, "bold") == 0) return 700;
    }
    return fallback;
}

void read_text_options(lua_State* L, int index, flux::text::text_options& options) {
    if (!lua_istable(L, index)) {
        return;
    }

    lua_getfield(L, index, "width");
    if (lua_isnumber(L, -1)) options.width = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, index, "height");
    if (lua_isnumber(L, -1)) options.height = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, index, "line_height");
    if (lua_isnumber(L, -1)) options.line_height = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, index, "max_lines");
    if (lua_isinteger(L, -1)) options.max_lines = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, index, "wrap");
    if (!lua_isnil(L, -1)) options.wrap = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    lua_getfield(L, index, "ellipsis");
    if (!lua_isnil(L, -1)) options.ellipsis = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    lua_getfield(L, index, "align");
    if (!lua_isnil(L, -1)) options.align = align_from_value(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "valign");
    if (!lua_isnil(L, -1)) options.valign = align_from_value(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "weight");
    if (!lua_isnil(L, -1)) options.weight = weight_from_value(L, -1, options.weight);
    lua_pop(L, 1);
}

int l_clear(lua_State* L) {
    flux::app::current().background = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    return 0;
}

int l_rect_filled(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    float h = static_cast<float>(luaL_checknumber(L, 4));
    uint32_t color = static_cast<uint32_t>(luaL_checkinteger(L, 5));
    float radius = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    flux::app::current().draw.rect_filled(x, y, w, h, color, radius);
    return 0;
}

int l_rect(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    float h = static_cast<float>(luaL_checknumber(L, 4));
    uint32_t color = static_cast<uint32_t>(luaL_checkinteger(L, 5));
    float radius = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float thickness = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    flux::app::current().draw.rect(x, y, w, h, color, radius, thickness);
    return 0;
}

int l_rect_gradient(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    float h = static_cast<float>(luaL_checknumber(L, 4));
    uint32_t from = static_cast<uint32_t>(luaL_checkinteger(L, 5));
    uint32_t to = static_cast<uint32_t>(luaL_checkinteger(L, 6));
    float radius = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    bool horizontal = lua_toboolean(L, 8) != 0;
    flux::app::current().draw.rect_gradient(x, y, w, h, from, to, radius, horizontal);
    return 0;
}

int l_rect_radial(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    float h = static_cast<float>(luaL_checknumber(L, 4));
    uint32_t inner = static_cast<uint32_t>(luaL_checkinteger(L, 5));
    uint32_t outer = static_cast<uint32_t>(luaL_checkinteger(L, 6));
    float radius = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    flux::app::current().draw.rect_radial(x, y, w, h, inner, outer, radius);
    return 0;
}

int l_shadow(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    float h = static_cast<float>(luaL_checknumber(L, 4));
    float radius = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    uint32_t color = static_cast<uint32_t>(luaL_optinteger(L, 6, 0x66000000));
    float spread = static_cast<float>(luaL_optnumber(L, 7, 8.0));
    float offset_y = static_cast<float>(luaL_optnumber(L, 8, 3.0));
    flux::app::current().draw.shadow_rect(x, y, w, h, radius, color, spread, offset_y);
    return 0;
}

int l_push_alpha(lua_State* L) {
    float alpha = static_cast<float>(luaL_checknumber(L, 1));
    flux::app::current().draw.push_alpha(alpha);
    return 0;
}

int l_pop_alpha(lua_State* L) {
    (void)L;
    flux::app::current().draw.pop_alpha();
    return 0;
}

int l_push_offset(lua_State* L) {
    float x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    float y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    flux::app::current().draw.push_offset(x, y);
    return 0;
}

int l_pop_offset(lua_State* L) {
    (void)L;
    flux::app::current().draw.pop_offset();
    return 0;
}

int l_text(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    const std::string_view value = lua_check_string_view(L, 3);
    uint32_t color = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    float size = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    if (size <= 0.0f) {
        size = flux::runtime::services().ui.style().font_size;
    }
    flux::text::text_options options;
    read_text_options(L, 6, options);
    float width = 0.0f;
    float height = 0.0f;
    flux::runtime::services().measure_text(value.data(), size, options, width, height);
    flux::app::current().draw.text_options(
        x, y, value, color, size,
        options.width, options.height, options.line_height, options.max_lines,
        options.align, options.valign, options.weight, options.wrap, options.ellipsis);
    lua_pushnumber(L, width);
    lua_pushnumber(L, height);
    return 2;
}

int l_measure_text(lua_State* L) {
    const char* value = luaL_checkstring(L, 1);
    float size = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    if (size <= 0.0f) {
        size = flux::runtime::services().ui.style().font_size;
    }
    flux::text::text_options options;
    read_text_options(L, 3, options);
    float width = 0.0f;
    float height = 0.0f;
    int line_count = 0;
    int overflow = 0;
    flux::runtime::services().measure_text(value, size, options, width, height,
                              line_count, overflow);
    lua_pushnumber(L, width);
    lua_pushnumber(L, height);
    lua_pushinteger(L, line_count);
    lua_pushinteger(L, overflow);
    return 4;
}

}

void push_draw_funcs(lua_State* L) {
    static const luaL_Reg functions[] = {
        { "clear", l_clear },
        { "rect_filled", l_rect_filled },
        { "rect", l_rect },
        { "rect_gradient", l_rect_gradient },
        { "rect_radial", l_rect_radial },
        { "shadow", l_shadow },
        { "push_alpha", l_push_alpha },
        { "pop_alpha", l_pop_alpha },
        { "push_offset", l_push_offset },
        { "pop_offset", l_pop_offset },
        { "text", l_text },
        { "measure_text", l_measure_text },
        { nullptr, nullptr },
    };
    lua_newtable(L);
    luaL_setfuncs(L, functions, 0);
}

}
