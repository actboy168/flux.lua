#pragma once

#include <lua.hpp>

namespace flux::lua {

inline void register_table(lua_State* L, const char* name, const luaL_Reg* functions) {
    lua_newtable(L);
    luaL_setfuncs(L, functions, 0);
    lua_setfield(L, -2, name);
}

}
