#include <lua.hpp>

#ifndef FLUX_ENABLE_TESTING
#include "lua/bindings.h"
#include "lua/lua_context.h"

namespace flux::lua {

void push_testing_table(lua_State*) {}

}

#else

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>

#include "app/win32_window.h"
#include "lua/bindings.h"
#include "lua/lua_context.h"
#include "lua/registration.h"
#include "lua/lua_string.h"
#include "widgets/control_anim.h"
#include "widgets/widget_runtime.h"
#include "widgets/widgets.h"

namespace flux::lua {

namespace {

int l_testing_mouse_move(lua_State* L) {
    flux::ui::input_state& state = input();
    state.window_active = true;
    state.mouse_inside = true;
    state.lost_focus = false;
    state.mouse_x = static_cast<float>(luaL_checknumber(L, 1));
    state.mouse_y = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}

int l_testing_mouse_down(lua_State* L) {
    int button = mouse_button(L, 1);
    flux::ui::input_state& state = input();
    state.window_active = true;
    state.mouse_inside = true;
    if (!state.mouse_down[button]) {
        state.mouse_pressed[button] = true;
    }
    state.mouse_down[button] = true;
    return 0;
}

int l_testing_mouse_up(lua_State* L) {
    int button = mouse_button(L, 1);
    flux::ui::input_state& state = input();
    state.mouse_down[button] = false;
    state.mouse_released[button] = true;
    return 0;
}

int l_testing_click(lua_State* L) {
    flux::ui::input_state& state = input();
    state.window_active = true;
    state.mouse_inside = true;
    state.lost_focus = false;
    state.mouse_x = static_cast<float>(luaL_checknumber(L, 1));
    state.mouse_y = static_cast<float>(luaL_checknumber(L, 2));
    int button = lua_isnoneornil(L, 3) ? 0 : mouse_button(L, 3);
    state.mouse_down[button] = false;
    state.mouse_pressed[button] = true;
    state.mouse_released[button] = true;
    return 0;
}

int l_testing_double_click(lua_State* L) {
    flux::ui::input_state& state = input();
    state.window_active = true;
    state.mouse_inside = true;
    state.lost_focus = false;
    state.mouse_x = static_cast<float>(luaL_checknumber(L, 1));
    state.mouse_y = static_cast<float>(luaL_checknumber(L, 2));
    int button = lua_isnoneornil(L, 3) ? 0 : mouse_button(L, 3);
    state.mouse_down[button] = false;
    state.mouse_pressed[button] = true;
    state.mouse_released[button] = true;
    state.mouse_double_clicked[button] = true;
    return 0;
}

int l_testing_wheel(lua_State* L) {
    input().wheel += static_cast<float>(luaL_checknumber(L, 1));
    return 0;
}

void apply_modifier(flux::ui::input_state& state, int code, bool down) {
    if (code == VK_CONTROL) {
        state.ctrl = down;
    } else if (code == VK_SHIFT) {
        state.shift = down;
    } else if (code == VK_MENU) {
        state.alt = down;
    }
}

void snapshot_modifiers(flux::ui::input_state& state, int code) {
    if (code == VK_CONTROL || code == VK_SHIFT || code == VK_MENU
        || code == VK_LCONTROL || code == VK_RCONTROL
        || code == VK_LSHIFT || code == VK_RSHIFT
        || code == VK_LMENU || code == VK_RMENU) {
        return;
    }
    state.key_ctrl[code] = state.ctrl;
    state.key_shift[code] = state.shift;
    state.key_alt[code] = state.alt;
}

int l_testing_key_down(lua_State* L) {
    int code = key_code(L, 1);
    flux::ui::input_state& state = input();
    state.window_active = true;
    snapshot_modifiers(state, code);
    if (!state.key_down[code]) {
        state.key_pressed[code] = true;
    }
    state.key_down[code] = true;
    apply_modifier(state, code, true);
    return 0;
}

int l_testing_key_up(lua_State* L) {
    int code = key_code(L, 1);
    flux::ui::input_state& state = input();
    state.key_down[code] = false;
    state.key_released[code] = true;
    apply_modifier(state, code, false);
    return 0;
}

int l_testing_key_press(lua_State* L) {
    int code = key_code(L, 1);
    flux::ui::input_state& state = input();
    state.window_active = true;
    snapshot_modifiers(state, code);
    state.key_down[code] = false;
    state.key_pressed[code] = true;
    state.key_released[code] = true;
    apply_modifier(state, code, false);
    return 0;
}

int l_testing_text(lua_State* L) {
    size_t length = 0;
    const char* value = luaL_checklstring(L, 1, &length);
    input().text.append(value, length);
    return 0;
}

int l_testing_ime(lua_State* L) {
    size_t length = 0;
    const char* value = luaL_checklstring(L, 1, &length);
    flux::ui::input_state& state = input();
    state.ime_composition.assign(value, length);
    state.ime_cursor = static_cast<int>(luaL_optinteger(L, 2, length));
    state.ime_composing = !state.ime_composition.empty();
    return 0;
}

int l_testing_ime_commit(lua_State* L) {
    size_t length = 0;
    const char* value = luaL_checklstring(L, 1, &length);
    flux::ui::input_state& state = input();
    state.text.append(value, length);
    state.ime_composition.clear();
    state.ime_cursor = 0;
    state.ime_composing = false;
    return 0;
}

int l_testing_anim(lua_State* L) {
    const flux::ui::ui_id id = flux::app::current().ui.make_id(lua_check_string_view(L, 1));
    flux::widgets::control_anim_state anim;
    if (!flux::widgets::control_anim_snapshot(id, anim)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushnumber(L, anim.hover);
    lua_pushnumber(L, anim.press);
    lua_pushnumber(L, anim.focus);
    lua_pushnumber(L, anim.value);
    return 4;
}

int l_testing_anim_reset(lua_State* L) {
    (void)L;
    flux::widgets::control_anim_reset();
    return 0;
}

int l_testing_debug_lose_device(lua_State* L) {
    std::string error;
    if (!flux::app::debug_lose_device(error)) {
        lua_pushboolean(L, 0);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_testing_debug_set_dpi_scale(lua_State* L) {
    const float scale = static_cast<float>(luaL_checknumber(L, 1));
    flux::app::debug_set_dpi_scale(scale);
    return 0;
}

int l_testing_focus_visible(lua_State* L) {
    lua_pushboolean(L, flux::runtime::services().ui.focus_visible() ? 1 : 0);
    return 1;
}

int l_testing_combo_reveal(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    flux::ui::ui_id id = 0;
    if (lua_isinteger(L, 2)) {
        id = static_cast<flux::ui::ui_id>(lua_tointeger(L, 2));
    } else if (lua_isstring(L, 2)) {
        id = flux::runtime::services().ui.make_id(lua_string_view(L, 2));
    }
    lua_pushnumber(L, flux::widgets::combo_reveal(label, id));
    return 1;
}

int l_testing_reset(lua_State* L) {
    (void)L;
    flux::widgets::widget_runtime_reset();
    flux::app::current().renderer.debug_set_animations_enabled(-1);
    flux::app::current().ui.clear_interaction();
    flux::ui::input_state& state = input();
    state.window_active = true;
    state.lost_focus = false;
    return 0;
}

const char* draw_kind_name(flux::ui::draw_kind kind) {
    switch (kind) {
    case flux::ui::draw_kind::rect_filled: return "rect_filled";
    case flux::ui::draw_kind::rect: return "rect";
    case flux::ui::draw_kind::rect_gradient: return "rect_gradient";
    case flux::ui::draw_kind::text: return "text";
    case flux::ui::draw_kind::push_clip: return "push_clip";
    case flux::ui::draw_kind::pop_clip: return "pop_clip";
    }
    return "unknown";
}

void set_field_number(lua_State* L, const char* key, lua_Number value) {
    lua_pushnumber(L, value);
    lua_setfield(L, -2, key);
}

void set_field_integer(lua_State* L, const char* key, lua_Integer value) {
    lua_pushinteger(L, value);
    lua_setfield(L, -2, key);
}

void set_field_bool(lua_State* L, const char* key, bool value) {
    lua_pushboolean(L, value ? 1 : 0);
    lua_setfield(L, -2, key);
}

void set_field_string(lua_State* L, const char* key, const char* value) {
    lua_pushstring(L, value);
    lua_setfield(L, -2, key);
}

void set_field_string(lua_State* L, const char* key, const char* value, size_t length) {
    lua_pushlstring(L, value, length);
    lua_setfield(L, -2, key);
}

void set_field_string(lua_State* L, const char* key, std::string_view value) {
    if (value.empty()) {
        set_field_string(L, key, "", 0);
    } else {
        set_field_string(L, key, value.data(), value.size());
    }
}

void push_draw_commands(lua_State* L, const flux::ui::draw_list& list,
                        flux::ui::render_layer layer) {
    const auto& commands = list.commands(layer);
    lua_createtable(L, static_cast<int>(commands.size()), 0);
    for (size_t i = 0; i < commands.size(); ++i) {
        const flux::ui::draw_cmd& cmd = commands[i];
        lua_createtable(L, 0, 21);
        set_field_string(L, "kind", draw_kind_name(cmd.kind));
        set_field_number(L, "x", cmd.x);
        set_field_number(L, "y", cmd.y);
        set_field_number(L, "w", cmd.width());
        set_field_number(L, "h", cmd.height());
        set_field_number(L, "radius", cmd.corner_radius());
        set_field_number(L, "thickness", cmd.stroke_thickness());
        set_field_number(L, "size", cmd.font_size());
        set_field_integer(L, "color", static_cast<lua_Integer>(cmd.color));
        set_field_integer(L, "color2", static_cast<lua_Integer>(cmd.color2()));
        set_field_integer(L, "gradient_type", cmd.gradient_kind());
        set_field_string(L, "text", list.text_of(cmd));
        set_field_number(L, "text_width", cmd.text_width());
        set_field_number(L, "text_height", cmd.text_height());
        set_field_number(L, "line_height", cmd.line_height());
        set_field_integer(L, "max_lines", cmd.max_lines());
        set_field_integer(L, "align", cmd.text_align());
        set_field_integer(L, "valign", cmd.text_valign());
        set_field_integer(L, "weight", cmd.font_weight());
        set_field_bool(L, "wrap", cmd.wrap());
        set_field_bool(L, "ellipsis", cmd.ellipsis());
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
}

int l_testing_draw_commands(lua_State* L) {
    if (lua_isnoneornil(L, 1)) {
        lua_createtable(L, 0, flux::ui::k_render_layer_count);
        for (int i = 0; i < flux::ui::k_render_layer_count; ++i) {
            const flux::ui::render_layer layer = static_cast<flux::ui::render_layer>(i);
            push_draw_commands(L, flux::app::current().draw, layer);
            lua_setfield(L, -2, flux::ui::render_layer_name(layer));
        }
        return 1;
    }
    const char* name = luaL_checkstring(L, 1);
    flux::ui::render_layer layer = flux::ui::render_layer::content;
    if (!flux::ui::parse_render_layer(name, layer)) {
        return luaL_error(L, "unknown render layer: %s", name);
    }
    push_draw_commands(L, flux::app::current().draw, layer);
    return 1;
}

int l_testing_id_depth(lua_State* L) {
    lua_pushinteger(L, ui().id_depth());
    return 1;
}

int l_testing_id_imbalance(lua_State* L) {
    lua_pushboolean(L, ui().id_imbalance() ? 1 : 0);
    return 1;
}

int l_testing_assert_id_balanced(lua_State* L) {
    if (ui().id_depth() != 0 || ui().id_imbalance()) {
        luaL_error(L, "ID stack is not balanced");
    }
    return 0;
}

int l_testing_style_imbalance(lua_State* L) {
    lua_pushboolean(L, ui().style_imbalance() ? 1 : 0);
    return 1;
}

int l_testing_debug_set_reduced_motion(lua_State* L) {
    if (lua_isnoneornil(L, 1)) {
        flux::app::current().renderer.debug_set_animations_enabled(-1);
        return 0;
    }
    const bool reduced = lua_toboolean(L, 1) != 0;
    flux::app::current().renderer.debug_set_animations_enabled(reduced ? 0 : 1);
    return 0;
}

}

void push_testing_table(lua_State* L) {
    static const luaL_Reg functions[] = {
        { "mouse_move", l_testing_mouse_move },
        { "mouse_down", l_testing_mouse_down },
        { "mouse_up", l_testing_mouse_up },
        { "click", l_testing_click },
        { "double_click", l_testing_double_click },
        { "wheel", l_testing_wheel },
        { "key_down", l_testing_key_down },
        { "key_up", l_testing_key_up },
        { "key_press", l_testing_key_press },
        { "text", l_testing_text },
        { "ime", l_testing_ime },
        { "ime_commit", l_testing_ime_commit },
        { "anim", l_testing_anim },
        { "anim_reset", l_testing_anim_reset },
        { "focus_visible", l_testing_focus_visible },
        { "combo_reveal", l_testing_combo_reveal },
        { "debug_lose_device", l_testing_debug_lose_device },
        { "debug_set_dpi_scale", l_testing_debug_set_dpi_scale },
        { "draw_commands", l_testing_draw_commands },
        { "id_depth", l_testing_id_depth },
        { "id_imbalance", l_testing_id_imbalance },
        { "assert_id_balanced", l_testing_assert_id_balanced },
        { "style_imbalance", l_testing_style_imbalance },
        { "debug_set_reduced_motion", l_testing_debug_set_reduced_motion },
        { "reset", l_testing_reset },
        { nullptr, nullptr },
    };
    register_table(L, "testing", functions);
}

}

#endif
