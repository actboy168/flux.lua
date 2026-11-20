#include <lua.hpp>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include "lua/bindings.h"
#include "lua/lua_context.h"
#include "lua/registration.h"

namespace flux::lua {

int mouse_button(lua_State* L, int index) {
    if (lua_isinteger(L, index)) {
        lua_Integer value = lua_tointeger(L, index);
        if (value >= 0 && value <= 2) {
            return static_cast<int>(value);
        }
        luaL_error(L, "mouse button index out of range: %d", static_cast<int>(value));
    }
    const char* name = luaL_checkstring(L, index);
    if (std::strcmp(name, "left") == 0 || std::strcmp(name, "l") == 0) {
        return 0;
    }
    if (std::strcmp(name, "right") == 0 || std::strcmp(name, "r") == 0) {
        return 1;
    }
    if (std::strcmp(name, "middle") == 0 || std::strcmp(name, "m") == 0) {
        return 2;
    }
    luaL_error(L, "unknown mouse button: %s", name);
    return 0;
}

int key_code(lua_State* L, int index) {
    if (lua_isinteger(L, index)) {
        lua_Integer value = lua_tointeger(L, index);
        if (value < 0 || value > 255) {
            luaL_error(L, "key code out of range: %d", static_cast<int>(value));
        }
        return static_cast<int>(value);
    }

    const char* raw = luaL_checkstring(L, index);
    std::string name(raw);
    for (char& c : name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (name.size() == 1) {
        char c = name[0];
        if (c >= 'a' && c <= 'z') {
            return std::toupper(static_cast<unsigned char>(c));
        }
        if (c >= '0' && c <= '9') {
            return c;
        }
        if (c == ' ') {
            return VK_SPACE;
        }
    }

    static const struct {
        const char* name;
        int code;
    } keys[] = {
        { "tab", VK_TAB },
        { "enter", VK_RETURN },
        { "return", VK_RETURN },
        { "escape", VK_ESCAPE },
        { "esc", VK_ESCAPE },
        { "space", VK_SPACE },
        { "backspace", VK_BACK },
        { "delete", VK_DELETE },
        { "del", VK_DELETE },
        { "insert", VK_INSERT },
        { "home", VK_HOME },
        { "end", VK_END },
        { "pageup", VK_PRIOR },
        { "pgup", VK_PRIOR },
        { "pagedown", VK_NEXT },
        { "pgdn", VK_NEXT },
        { "left", VK_LEFT },
        { "right", VK_RIGHT },
        { "up", VK_UP },
        { "down", VK_DOWN },
        { "shift", VK_SHIFT },
        { "ctrl", VK_CONTROL },
        { "control", VK_CONTROL },
        { "alt", VK_MENU },
        { "menu", VK_MENU },
    };
    for (const auto& key : keys) {
        if (name == key.name) {
            return key.code;
        }
    }
    if (name.size() >= 2 && name[0] == 'f') {
        int number = std::atoi(name.c_str() + 1);
        if (number >= 1 && number <= 24) {
            return VK_F1 + number - 1;
        }
    }
    luaL_error(L, "unknown key name: %s", raw);
    return 0;
}

namespace {

int l_mouse_pos(lua_State* L) {
    lua_pushnumber(L, ui().input.mouse_x);
    lua_pushnumber(L, ui().input.mouse_y);
    return 2;
}

int l_mouse_down(lua_State* L) {
    int button = mouse_button(L, 1);
    lua_pushboolean(L, ui().input.mouse_down[button] ? 1 : 0);
    return 1;
}

int l_mouse_pressed(lua_State* L) {
    int button = mouse_button(L, 1);
    lua_pushboolean(L, ui().input.mouse_pressed[button] ? 1 : 0);
    return 1;
}

int l_mouse_released(lua_State* L) {
    int button = mouse_button(L, 1);
    lua_pushboolean(L, ui().input.mouse_released[button] ? 1 : 0);
    return 1;
}

int l_mouse_double_clicked(lua_State* L) {
    int button = mouse_button(L, 1);
    lua_pushboolean(L, ui().input.mouse_double_clicked[button] ? 1 : 0);
    return 1;
}

int l_mouse_wheel(lua_State* L) {
    lua_pushnumber(L, ui().input.wheel);
    return 1;
}

int l_mouse_inside(lua_State* L) {
    lua_pushboolean(L, ui().input.mouse_inside ? 1 : 0);
    return 1;
}

int l_mouse_in_rect(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float width = static_cast<float>(luaL_checknumber(L, 3));
    float height = static_cast<float>(luaL_checknumber(L, 4));
    flux::ui::input_state& input = ui().input;
    bool inside = input.window_active && input.mouse_inside
        && input.mouse_x >= x && input.mouse_x < x + width
        && input.mouse_y >= y && input.mouse_y < y + height;
    lua_pushboolean(L, inside ? 1 : 0);
    return 1;
}

int l_key_down(lua_State* L) {
    int code = key_code(L, 1);
    lua_pushboolean(L, ui().input.key_down[code] ? 1 : 0);
    return 1;
}

int l_key_pressed(lua_State* L) {
    int code = key_code(L, 1);
    lua_pushboolean(L, ui().input.key_pressed[code] ? 1 : 0);
    return 1;
}

int l_key_released(lua_State* L) {
    int code = key_code(L, 1);
    lua_pushboolean(L, ui().input.key_released[code] ? 1 : 0);
    return 1;
}

int l_text(lua_State* L) {
    const std::string_view text = ui().input.text;
    lua_pushlstring(L, text.data(), text.size());
    return 1;
}

int l_delta_time(lua_State* L) {
    lua_pushnumber(L, ui().delta_time());
    return 1;
}

int l_shift(lua_State* L) {
    lua_pushboolean(L, ui().input.shift ? 1 : 0);
    return 1;
}

int l_ctrl(lua_State* L) {
    lua_pushboolean(L, ui().input.ctrl ? 1 : 0);
    return 1;
}

int l_alt(lua_State* L) {
    lua_pushboolean(L, ui().input.alt ? 1 : 0);
    return 1;
}

}

void push_input_table(lua_State* L) {
    static const luaL_Reg functions[] = {
        { "mouse_pos", l_mouse_pos },
        { "mouse_down", l_mouse_down },
        { "mouse_pressed", l_mouse_pressed },
        { "mouse_released", l_mouse_released },
        { "mouse_double_clicked", l_mouse_double_clicked },
        { "mouse_wheel", l_mouse_wheel },
        { "mouse_inside", l_mouse_inside },
        { "mouse_in_rect", l_mouse_in_rect },
        { "key_down", l_key_down },
        { "key_pressed", l_key_pressed },
        { "key_released", l_key_released },
        { "text", l_text },
        { "delta_time", l_delta_time },
        { "shift", l_shift },
        { "ctrl", l_ctrl },
        { "alt", l_alt },
        { nullptr, nullptr },
    };
    register_table(L, "input", functions);
}

}
