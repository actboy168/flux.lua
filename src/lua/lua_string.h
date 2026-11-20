#pragma once

#include <lua.hpp>

#include <string_view>

namespace flux::lua {

// Non-checking view: returns an empty view when the value is not a string.
inline std::string_view lua_string_view(lua_State* L, int index) {
    size_t length = 0;
    const char* value = lua_tolstring(L, index, &length);
    return value != nullptr ? std::string_view(value, length) : std::string_view();
}

// Checking view: raises a Lua error when the value is not a string.
inline std::string_view lua_check_string_view(lua_State* L, int index) {
    size_t length = 0;
    const char* value = luaL_checklstring(L, index, &length);
    return std::string_view(value, length);
}

}
