#include <lua.hpp>

#include "lua/bindings.h"

extern "C" int luaopen_flux(lua_State* L) {
    luaL_checkversion(L);
    flux::lua::push_draw_funcs(L);
    flux::lua::push_ui_funcs(L);
    flux::lua::push_input_table(L);
    flux::lua::push_widget_funcs(L);
    flux::lua::push_layer_table(L);
    flux::lua::push_anim_table(L);
    flux::lua::push_window_funcs(L);
    flux::lua::push_offscreen_table(L);
    flux::lua::push_testing_table(L);
    return 1;
}
