#include <lua.hpp>

#include <cstring>
#include <string>
#include <string_view>

#include "lua/bindings.h"
#include "lua/lua_context.h"
#include "lua/lua_string.h"
#include "lua/lua_util.h"

namespace flux::lua {

namespace {

bool option_bool(lua_State* L, int index, const char* key, bool fallback) {
    lua_getfield(L, index, key);
    const bool value = lua_isnil(L, -1) ? fallback : lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return value;
}

flux::ui::ui_id child_id(lua_State* L, int index) {
    if (lua_isinteger(L, index)) {
        return static_cast<flux::ui::ui_id>(lua_tointeger(L, index));
    }
    if (lua_istable(L, index)) {
        const flux::ui::ui_id id = read_stable_id(L, index, nullptr);
        if (id == 0) {
            luaL_error(L, "child reference needs a non-zero id");
        }
        return id;
    }
    return ui().make_id_absolute(lua_check_string_view(L, index));
}

int l_push_id(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    ui().push_id(label);
    return 0;
}

int l_pop_id(lua_State* L) {
    (void)L;
    ui().pop_id();
    return 0;
}

int l_set_cursor_pos(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    ui().set_cursor_pos(x, y);
    return 0;
}

int l_cursor_pos(lua_State* L) {
    lua_pushnumber(L, ui().cursor_x());
    lua_pushnumber(L, ui().cursor_y());
    return 2;
}

int l_same_line(lua_State* L) {
    (void)L;
    ui().same_line();
    return 0;
}

int l_dummy(lua_State* L) {
    float width = static_cast<float>(luaL_checknumber(L, 1));
    float height = static_cast<float>(luaL_checknumber(L, 2));
    ui().dummy(width, height);
    return 0;
}

int l_set_item_spacing(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    ui().set_item_spacing(x, y);
    return 0;
}

int l_indent(lua_State* L) {
    float width = static_cast<float>(luaL_checknumber(L, 1));
    ui().indent(width);
    return 0;
}

int l_unindent(lua_State* L) {
    float width = static_cast<float>(luaL_checknumber(L, 1));
    ui().unindent(width);
    return 0;
}

int l_invisible_button(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    float width = static_cast<float>(luaL_checknumber(L, 2));
    float height = static_cast<float>(luaL_checknumber(L, 3));
    bool focusable = true;
    bool interactive = true;
    bool tab_stop = true;
    flux::ui::ui_id id = 0;
    if (lua_istable(L, 4)) {
        focusable = option_bool(L, 4, "focusable", true);
        interactive = option_bool(L, 4, "interactive", focusable);
        tab_stop = option_bool(L, 4, "tab_stop", focusable);
        id = read_stable_id(L, 4, label);
    }
    bool hovered = false;
    bool active = false;
    bool clicked = false;
    bool double_clicked = false;
    bool focused = false;
    flux::ui::ui_context::widget_input_options options;
    options.interactive = interactive;
    options.focusable = focusable;
    options.tab_stop = tab_stop;
    ui().widget_ex(label, width, height, options, hovered, active, clicked, double_clicked, focused, id);
    lua_pushboolean(L, hovered ? 1 : 0);
    lua_pushboolean(L, active ? 1 : 0);
    lua_pushboolean(L, clicked ? 1 : 0);
    lua_pushboolean(L, double_clicked ? 1 : 0);
    lua_pushboolean(L, focused ? 1 : 0);
    return 5;
}

int l_item_rect(lua_State* L) {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    ui().last_item_rect(x, y, width, height);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, width);
    lua_pushnumber(L, height);
    return 4;
}

int l_is_item_hovered(lua_State* L) {
    lua_pushboolean(L, ui().is_item_hovered() ? 1 : 0);
    return 1;
}

int l_is_item_active(lua_State* L) {
    lua_pushboolean(L, ui().is_item_active() ? 1 : 0);
    return 1;
}

int l_is_item_clicked(lua_State* L) {
    lua_pushboolean(L, ui().is_item_clicked() ? 1 : 0);
    return 1;
}

int l_is_item_double_clicked(lua_State* L) {
    lua_pushboolean(L, ui().is_item_double_clicked() ? 1 : 0);
    return 1;
}

int l_is_item_focused(lua_State* L) {
    lua_pushboolean(L, ui().is_item_focused() ? 1 : 0);
    return 1;
}

int l_set_keyboard_focus(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    ui().set_keyboard_focus(label);
    return 0;
}

int style_color_index(lua_State* L, int index) {
    if (lua_isinteger(L, index)) {
        return static_cast<int>(lua_tointeger(L, index));
    }
    const char* name = luaL_checkstring(L, index);
    static const struct {
        const char* name;
        int value;
    } colors[] = {
        { "text", flux::ui::style_color_text },
        { "text_dim", flux::ui::style_color_text_dim },
        { "panel", flux::ui::style_color_panel },
        { "panel_hover", flux::ui::style_color_panel_hover },
        { "panel_active", flux::ui::style_color_panel_active },
        { "border", flux::ui::style_color_border },
        { "accent", flux::ui::style_color_accent },
        { "accent_hover", flux::ui::style_color_accent_hover },
        { "accent_active", flux::ui::style_color_accent_active },
        { "white", flux::ui::style_color_white },
        { "disabled", flux::ui::style_color_disabled },
        { "track", flux::ui::style_color_track },
        { "popup_bg", flux::ui::style_color_popup_bg },
        { "popup_border", flux::ui::style_color_popup_border },
        { "popup_shadow", flux::ui::style_color_popup_shadow },
        { "menu_text", flux::ui::style_color_menu_text },
        { "menu_shortcut", flux::ui::style_color_menu_shortcut },
        { "menu_item_hover", flux::ui::style_color_menu_item_hover },
        { "menu_separator", flux::ui::style_color_menu_separator },
        { "tooltip_bg", flux::ui::style_color_tooltip_bg },
        { "tooltip_border", flux::ui::style_color_tooltip_border },
        { "tooltip_shadow", flux::ui::style_color_tooltip_shadow },
        { "tab_bar_bg", flux::ui::style_color_tab_bar_bg },
        { "tab_bar_border", flux::ui::style_color_tab_bar_border },
        { "tab_bg", flux::ui::style_color_tab_bg },
        { "tab_bg_hover", flux::ui::style_color_tab_bg_hover },
        { "tab_bg_selected", flux::ui::style_color_tab_bg_selected },
        { "tab_text", flux::ui::style_color_tab_text },
        { "tab_text_selected", flux::ui::style_color_tab_text_selected },
        { "tab_baseline", flux::ui::style_color_tab_baseline },
        { "tab_focus", flux::ui::style_color_tab_focus },
    };
    for (const auto& color : colors) {
        if (std::strcmp(name, color.name) == 0) {
            return color.value;
        }
    }
    luaL_error(L, "unknown style color: %s", name);
    return 0;
}

int style_var_index(lua_State* L, int index) {
    if (lua_isinteger(L, index)) {
        return static_cast<int>(lua_tointeger(L, index));
    }
    const char* name = luaL_checkstring(L, index);
    static const struct {
        const char* name;
        int value;
    } vars[] = {
        { "item_spacing_x", flux::ui::style_var_item_spacing_x },
        { "item_spacing_y", flux::ui::style_var_item_spacing_y },
        { "frame_padding_x", flux::ui::style_var_frame_padding_x },
        { "frame_padding_y", flux::ui::style_var_frame_padding_y },
        { "rounding", flux::ui::style_var_rounding },
        { "border_size", flux::ui::style_var_border_size },
        { "scrollbar_size", flux::ui::style_var_scrollbar_size },
        { "font_size", flux::ui::style_var_font_size },
        { "line_height", flux::ui::style_var_line_height },
        { "popup_padding_x", flux::ui::style_var_popup_padding_x },
        { "popup_padding_y", flux::ui::style_var_popup_padding_y },
        { "popup_shadow_spread", flux::ui::style_var_popup_shadow_spread },
        { "popup_shadow_offset_y", flux::ui::style_var_popup_shadow_offset_y },
        { "menu_item_height", flux::ui::style_var_menu_item_height },
        { "menu_item_padding_x", flux::ui::style_var_menu_item_padding_x },
        { "menu_separator_gap", flux::ui::style_var_menu_separator_gap },
        { "menu_separator_height", flux::ui::style_var_menu_separator_height },
        { "menu_header_line_height", flux::ui::style_var_menu_header_line_height },
        { "combo_item_height", flux::ui::style_var_combo_item_height },
        { "combo_popup_padding", flux::ui::style_var_combo_popup_padding },
        { "combo_popup_max_height", flux::ui::style_var_combo_popup_max_height },
        { "combo_popup_gap", flux::ui::style_var_combo_popup_gap },
        { "combo_popup_min_width", flux::ui::style_var_combo_popup_min_width },
        { "tooltip_padding_x", flux::ui::style_var_tooltip_padding_x },
        { "tooltip_padding_y", flux::ui::style_var_tooltip_padding_y },
        { "tooltip_shadow_spread", flux::ui::style_var_tooltip_shadow_spread },
        { "tooltip_shadow_offset_y", flux::ui::style_var_tooltip_shadow_offset_y },
        { "tab_height", flux::ui::style_var_tab_height },
        { "tab_min_width", flux::ui::style_var_tab_min_width },
        { "tab_gap", flux::ui::style_var_tab_gap },
        { "tab_padding_x", flux::ui::style_var_tab_padding_x },
        { "tab_rounding", flux::ui::style_var_tab_rounding },
        { "tab_baseline_size", flux::ui::style_var_tab_baseline_size },
        { "tab_font_size", flux::ui::style_var_tab_font_size },
        { "scrollbar_idle_alpha", flux::ui::style_var_scrollbar_idle_alpha },
    };
    for (const auto& var : vars) {
        if (std::strcmp(name, var.name) == 0) {
            return var.value;
        }
    }
    luaL_error(L, "unknown style var: %s", name);
    return 0;
}

int l_push_style_color(lua_State* L) {
    int index = style_color_index(L, 1);
    uint32_t color = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    ui().push_style_color(index, color);
    return 0;
}

int l_pop_style_color(lua_State* L) {
    int count = lua_isnoneornil(L, 1) ? 1 : static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, ui().pop_style_color(count) ? 1 : 0);
    return 1;
}

int l_push_style_var(lua_State* L) {
    int index = style_var_index(L, 1);
    float value = static_cast<float>(luaL_checknumber(L, 2));
    ui().push_style_var(index, value);
    return 0;
}

int l_pop_style_var(lua_State* L) {
    int count = lua_isnoneornil(L, 1) ? 1 : static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, ui().pop_style_var(count) ? 1 : 0);
    return 1;
}

int l_style_color(lua_State* L) {
    int index = style_color_index(L, 1);
    lua_pushinteger(L, ui().style_color(index));
    return 1;
}

int l_style_var(lua_State* L) {
    int index = style_var_index(L, 1);
    lua_pushnumber(L, ui().style_var(index));
    return 1;
}

int l_set_style_color(lua_State* L) {
    int index = style_color_index(L, 1);
    uint32_t color = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    ui().set_base_style_color(index, color);
    return 0;
}

int l_set_style_var(lua_State* L) {
    int index = style_var_index(L, 1);
    float value = static_cast<float>(luaL_checknumber(L, 2));
    ui().set_base_style_var(index, value);
    return 0;
}

int l_style_depth(lua_State* L) {
    lua_pushinteger(L, ui().style_depth());
    return 1;
}

int l_assert_style_balanced(lua_State* L) {
    if (ui().style_depth() != 0 || ui().style_imbalance()) {
        luaL_error(L, "style stack is not balanced");
    }
    return 0;
}

int l_set_font_family(lua_State* L) {
    const bool ok = flux::runtime::services().set_font_family(
        lua_check_string_view(L, 1));
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok) {
        lua_pushstring(L, "font family not found");
        return 2;
    }
    return 1;
}

int l_font_family(lua_State* L) {
    const std::string family = flux::runtime::services().font_family();
    lua_pushlstring(L, family.data(), family.size());
    return 1;
}

int l_begin_child(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    float width = static_cast<float>(luaL_checknumber(L, 2));
    float height = static_cast<float>(luaL_checknumber(L, 3));
    const bool opened = ui().begin_child(
        label, width, height, read_stable_id(L, 4, label));
    lua_pushboolean(L, opened ? 1 : 0);
    if (!opened) {
        lua_pushstring(L, "child size must be positive");
        return 2;
    }
    return 1;
}

int l_begin_child_fixed(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    float width = static_cast<float>(luaL_checknumber(L, 2));
    float height = static_cast<float>(luaL_checknumber(L, 3));
    float content_height = static_cast<float>(luaL_checknumber(L, 4));
    const bool opened = ui().begin_child_fixed(
        label, width, height, content_height, read_stable_id(L, 5, label));
    lua_pushboolean(L, opened ? 1 : 0);
    if (!opened) {
        lua_pushstring(L, "child size must be positive");
        return 2;
    }
    return 1;
}

int l_end_child(lua_State* L) {
    const bool ok = ui().end_child();
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok) {
        const std::string& error = ui().last_error();
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    return 1;
}

int l_child_scroll(lua_State* L) {
    lua_pushnumber(L, ui().child_scroll(child_id(L, 1)));
    return 1;
}

int l_child_content_height(lua_State* L) {
    lua_pushnumber(L, ui().child_content_height(child_id(L, 1)));
    return 1;
}

int l_set_child_scroll(lua_State* L) {
    const flux::ui::ui_id id = child_id(L, 1);
    float value = static_cast<float>(luaL_checknumber(L, 2));
    ui().set_child_scroll(id, value);
    return 0;
}

int l_begin_row(lua_State* L) {
    (void)L;
    ui().begin_row();
    return 0;
}

int l_end_row(lua_State* L) {
    const bool ok = ui().end_row();
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok) {
        const std::string& error = ui().last_error();
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    return 1;
}

int l_begin_column(lua_State* L) {
    (void)L;
    ui().begin_column();
    return 0;
}

int l_end_column(lua_State* L) {
    const bool ok = ui().end_column();
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok) {
        const std::string& error = ui().last_error();
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    return 1;
}

int l_push_clip(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float width = static_cast<float>(luaL_checknumber(L, 3));
    float height = static_cast<float>(luaL_checknumber(L, 4));
    ui().push_clip(x, y, width, height);
    flux::app::current().draw.push_clip(x, y, width, height);
    return 0;
}

int l_pop_clip(lua_State* L) {
    (void)L;
    flux::app::current().draw.pop_clip();
    ui().pop_clip();
    return 0;
}

int l_is_rect_visible(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float width = static_cast<float>(luaL_checknumber(L, 3));
    float height = static_cast<float>(luaL_checknumber(L, 4));
    lua_pushboolean(L, ui().is_rect_visible(x, y, width, height) ? 1 : 0);
    return 1;
}

int l_item_visible(lua_State* L) {
    lua_pushboolean(L, ui().is_item_visible() ? 1 : 0);
    return 1;
}

int l_time(lua_State* L) {
    lua_pushnumber(L, ui().time());
    return 1;
}

int l_set_modal(lua_State* L) {
    ui().set_modal(lua_toboolean(L, 1) != 0);
    return 0;
}

int l_modal(lua_State* L) {
    lua_pushboolean(L, ui().modal() ? 1 : 0);
    return 1;
}

int l_make_id(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(
        ui().make_id(lua_check_string_view(L, 1))));
    return 1;
}

int l_keyboard_focus_id(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(ui().keyboard_focus_id()));
    return 1;
}

}

void push_ui_funcs(lua_State* L) {
    static const luaL_Reg functions[] = {
        { "push_id", l_push_id },
        { "pop_id", l_pop_id },
        { "set_cursor_pos", l_set_cursor_pos },
        { "cursor_pos", l_cursor_pos },
        { "same_line", l_same_line },
        { "dummy", l_dummy },
        { "set_item_spacing", l_set_item_spacing },
        { "indent", l_indent },
        { "unindent", l_unindent },
        { "invisible_button", l_invisible_button },
        { "item_rect", l_item_rect },
        { "is_item_hovered", l_is_item_hovered },
        { "is_item_active", l_is_item_active },
        { "is_item_clicked", l_is_item_clicked },
        { "is_item_double_clicked", l_is_item_double_clicked },
        { "is_item_focused", l_is_item_focused },
        { "set_keyboard_focus", l_set_keyboard_focus },
        { "push_style_color", l_push_style_color },
        { "pop_style_color", l_pop_style_color },
        { "push_style_var", l_push_style_var },
        { "pop_style_var", l_pop_style_var },
        { "style_color", l_style_color },
        { "style_var", l_style_var },
        { "set_style_color", l_set_style_color },
        { "set_style_var", l_set_style_var },
        { "style_depth", l_style_depth },
        { "assert_style_balanced", l_assert_style_balanced },
        { "set_font_family", l_set_font_family },
        { "font_family", l_font_family },
        { "begin_child", l_begin_child },
        { "begin_child_fixed", l_begin_child_fixed },
        { "end_child", l_end_child },
        { "child_scroll", l_child_scroll },
        { "child_content_height", l_child_content_height },
        { "set_child_scroll", l_set_child_scroll },
        { "begin_row", l_begin_row },
        { "end_row", l_end_row },
        { "begin_column", l_begin_column },
        { "end_column", l_end_column },
        { "push_clip", l_push_clip },
        { "pop_clip", l_pop_clip },
        { "is_rect_visible", l_is_rect_visible },
        { "item_visible", l_item_visible },
        { "time", l_time },
        { "set_modal", l_set_modal },
        { "modal", l_modal },
        { "make_id", l_make_id },
        { "keyboard_focus_id", l_keyboard_focus_id },
        { nullptr, nullptr },
    };
    luaL_setfuncs(L, functions, 0);

}

}
