#include <lua.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "app/context.h"
#include "lua/registration.h"

namespace flux::lua {

namespace {

int l_anim_poll(lua_State* L) {
    const std::vector<uint64_t> ids = flux::app::current().renderer.poll_animations();
    lua_createtable(L, static_cast<int>(ids.size()), 0);
    for (size_t i = 0; i < ids.size(); ++i) {
        lua_pushinteger(L, static_cast<lua_Integer>(ids[i]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

int l_anim_running(lua_State* L) {
    const uint64_t id = static_cast<uint64_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, flux::app::current().renderer.animation_running(id) ? 1 : 0);
    return 1;
}

int l_anim_stop(lua_State* L) {
    const uint64_t id = static_cast<uint64_t>(luaL_checkinteger(L, 1));
    const bool apply_final = lua_isnoneornil(L, 2) || lua_toboolean(L, 2) != 0;
    std::string error;
    if (!flux::app::current().renderer.stop_animation(id, apply_final, error)) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_anim_reduced_motion(lua_State* L) {
    lua_pushboolean(L, flux::app::current().renderer.animations_enabled() ? 0 : 1);
    return 1;
}

}

void push_anim_table(lua_State* L) {
    static const luaL_Reg functions[] = {
        { "poll", l_anim_poll },
        { "running", l_anim_running },
        { "stop", l_anim_stop },
        { "reduced_motion", l_anim_reduced_motion },
        { nullptr, nullptr },
    };
    register_table(L, "anim", functions);
}

}
