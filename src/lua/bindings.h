#pragma once

struct lua_State;

namespace flux::lua {

void push_draw_funcs(lua_State* L);
void push_ui_funcs(lua_State* L);
void push_input_table(lua_State* L);
void push_widget_funcs(lua_State* L);
void push_layer_table(lua_State* L);
void push_anim_table(lua_State* L);
void push_window_funcs(lua_State* L);
void push_offscreen_table(lua_State* L);
void push_testing_table(lua_State* L);

}
