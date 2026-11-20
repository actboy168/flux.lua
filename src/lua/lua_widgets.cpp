#include <lua.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "app/context.h"
#include "runtime/services.h"
#include "lua/bindings.h"
#include "lua/lua_string.h"
#include "lua/lua_util.h"
#include "widgets/control_anim.h"
#include "widgets/virtual_list.h"
#include "widgets/widget_runtime.h"
#include "widgets/widgets.h"

namespace flux::lua {

namespace {

bool optional_bool(lua_State* L, int index, bool fallback) {
    if (lua_isnoneornil(L, index)) {
        return fallback;
    }
    return lua_toboolean(L, index) != 0;
}

bool bool_field(lua_State* L, int index, const char* key, bool fallback) {
    lua_getfield(L, index, key);
    const bool value = lua_isnil(L, -1) ? fallback : lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return value;
}

float number_field(lua_State* L, int index, const char* key, float fallback) {
    lua_getfield(L, index, key);
    const float value = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : fallback;
    lua_pop(L, 1);
    return value;
}

int integer_field(lua_State* L, int index, const char* key, int fallback) {
    lua_getfield(L, index, key);
    const int value = lua_isnumber(L, -1) ? static_cast<int>(lua_tointeger(L, -1)) : fallback;
    lua_pop(L, 1);
    return value;
}

void read_tooltip_options(lua_State* L, int index, flux::widgets::tooltip_options& options) {
    if (!lua_istable(L, index)) {
        return;
    }
    lua_getfield(L, index, "delay");
    options.delay = static_cast<float>(luaL_optnumber(L, -1, options.delay));
    lua_pop(L, 1);
    lua_getfield(L, index, "max_width");
    options.max_width = static_cast<float>(luaL_optnumber(L, -1, options.max_width));
    lua_pop(L, 1);
    lua_getfield(L, index, "offset_x");
    options.offset_x = static_cast<float>(luaL_optnumber(L, -1, options.offset_x));
    lua_pop(L, 1);
    lua_getfield(L, index, "offset_y");
    options.offset_y = static_cast<float>(luaL_optnumber(L, -1, options.offset_y));
    lua_pop(L, 1);
    lua_getfield(L, index, "follow_mouse");
    if (!lua_isnil(L, -1)) {
        options.follow_mouse = lua_toboolean(L, -1) != 0;
    }
    lua_pop(L, 1);
}

flux::widgets::list_scroll_align scroll_align_from(lua_State* L, int index) {
    const char* value = luaL_optstring(L, index, "top");
    if (std::strcmp(value, "center") == 0) {
        return flux::widgets::list_scroll_align::center;
    }
    if (std::strcmp(value, "bottom") == 0) {
        return flux::widgets::list_scroll_align::bottom;
    }
    return flux::widgets::list_scroll_align::top;
}

flux::ui::ui_id list_id(lua_State* L, int index) {
    if (lua_isinteger(L, index)) {
        return static_cast<flux::ui::ui_id>(lua_tointeger(L, index));
    }
    if (lua_istable(L, index)) {
        const flux::ui::ui_id id = read_stable_id(L, index, nullptr);
        if (id == 0) {
            luaL_error(L, "list reference needs a non-zero id");
        }
        return id;
    }
    return flux::runtime::services().ui.make_id_absolute(luaL_checkstring(L, index));
}

int l_tab_bar(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    flux::widgets::tab_bar_options options;
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "id");
        if (lua_isinteger(L, -1)) {
            options.id = static_cast<flux::ui::ui_id>(lua_tointeger(L, -1));
        } else if (lua_isstring(L, -1)) {
            options.id = flux::runtime::services().ui.make_id(lua_string_view(L, -1));
        }
        lua_pop(L, 1);
        lua_getfield(L, 3, "selected");
        options.selected = static_cast<int>(luaL_optinteger(L, -1, 1));
        lua_pop(L, 1);
        lua_getfield(L, 3, "width");
        options.width = static_cast<float>(luaL_optnumber(L, -1, 0.0));
        lua_pop(L, 1);
        lua_getfield(L, 3, "height");
        options.height = static_cast<float>(luaL_optnumber(L, -1, 0.0));
        lua_pop(L, 1);
        lua_getfield(L, 3, "min_width");
        options.min_width = static_cast<float>(luaL_optnumber(L, -1, -1.0));
        lua_pop(L, 1);
        lua_getfield(L, 3, "gap");
        options.gap = static_cast<float>(luaL_optnumber(L, -1, -1.0));
        lua_pop(L, 1);
        lua_getfield(L, 3, "padding_x");
        options.padding_x = static_cast<float>(luaL_optnumber(L, -1, -1.0));
        lua_pop(L, 1);
        lua_getfield(L, 3, "rounding");
        options.rounding = static_cast<float>(luaL_optnumber(L, -1, -1.0));
        lua_pop(L, 1);
        lua_getfield(L, 3, "equal_width");
        options.equal_width = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        lua_getfield(L, 3, "roving");
        options.roving = lua_isnil(L, -1) ? true : lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        lua_getfield(L, 3, "draw_baseline");
        options.draw_baseline = lua_isnil(L, -1) ? true : lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
    }

    const int count = static_cast<int>(lua_rawlen(L, 2));
    std::vector<std::string_view> labels;
    std::vector<std::string_view> ids;
    std::vector<bool> has_id;
    labels.reserve(static_cast<size_t>(count));
    ids.reserve(static_cast<size_t>(count));
    has_id.reserve(static_cast<size_t>(count));

    for (int i = 1; i <= count; ++i) {
        lua_rawgeti(L, 2, i);
        std::string_view text;
        std::string_view id;
        bool with_id = false;
        if (lua_isstring(L, -1)) {
            text = lua_string_view(L, -1);
        } else if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "label");
            if (!lua_isstring(L, -1)) {
                lua_pop(L, 1);
                lua_rawgeti(L, -1, 1);
            }
            if (lua_isstring(L, -1)) {
                text = lua_string_view(L, -1);
            }
            lua_pop(L, 1);
            lua_getfield(L, -1, "id");
            if (lua_isstring(L, -1)) {
                id = lua_string_view(L, -1);
                with_id = !id.empty();
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        labels.push_back(text);
        ids.push_back(id);
        has_id.push_back(with_id);
    }

    std::vector<flux::widgets::tab_item_desc> items;
    items.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        flux::widgets::tab_item_desc item;
        item.label = labels[static_cast<size_t>(i)].data();
        item.id = has_id[static_cast<size_t>(i)]
            ? ids[static_cast<size_t>(i)].data() : nullptr;
        lua_rawgeti(L, 2, i + 1);
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "width");
            item.width = static_cast<float>(luaL_optnumber(L, -1, 0.0));
            lua_pop(L, 1);
            lua_getfield(L, -1, "disabled");
            item.disabled = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        items.push_back(item);
    }

    const flux::widgets::tab_bar_result result = flux::widgets::tab_bar(
        label, items.data(), count, options);
    lua_pushinteger(L, result.selected);
    lua_pushinteger(L, result.clicked);
    lua_pushboolean(L, result.changed ? 1 : 0);
    lua_pushinteger(L, result.focused);
    return 4;
}

int l_popup_place(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    flux::widgets::popup_anchor anchor;
    lua_getfield(L, 1, "x");
    anchor.x = static_cast<float>(luaL_optnumber(L, -1, 0.0));
    lua_pop(L, 1);
    lua_getfield(L, 1, "y");
    anchor.y = static_cast<float>(luaL_optnumber(L, -1, 0.0));
    lua_pop(L, 1);
    lua_getfield(L, 1, "w");
    anchor.w = static_cast<float>(luaL_optnumber(L, -1, 0.0));
    lua_pop(L, 1);
    lua_getfield(L, 1, "h");
    anchor.h = static_cast<float>(luaL_optnumber(L, -1, 0.0));
    lua_pop(L, 1);

    const float width = static_cast<float>(luaL_checknumber(L, 2));
    const float height = static_cast<float>(luaL_checknumber(L, 3));
    flux::widgets::popup_place_options options;
    float viewport_width = static_cast<float>(flux::runtime::services().width);
    float viewport_height = static_cast<float>(flux::runtime::services().height);
    if (lua_istable(L, 4)) {
        lua_getfield(L, 4, "gap");
        options.gap = static_cast<float>(luaL_optnumber(L, -1, options.gap));
        lua_pop(L, 1);
        lua_getfield(L, 4, "margin");
        options.margin = static_cast<float>(luaL_optnumber(L, -1, options.margin));
        lua_pop(L, 1);
        lua_getfield(L, 4, "viewport_w");
        if (lua_isnumber(L, -1)) {
            viewport_width = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);
        lua_getfield(L, 4, "viewport_h");
        if (lua_isnumber(L, -1)) {
            viewport_height = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);
    }
    const flux::widgets::popup_rect placed = flux::widgets::place_popup(
        anchor, width, height, options, viewport_width, viewport_height);
    lua_pushnumber(L, placed.x);
    lua_pushnumber(L, placed.y);
    lua_pushnumber(L, placed.w);
    lua_pushnumber(L, placed.h);
    return 4;
}

int l_begin_popup(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float width = static_cast<float>(luaL_checknumber(L, 4));
    float height = static_cast<float>(luaL_checknumber(L, 5));
    float alpha = -1.0f;
    bool modal = false;
    flux::ui::ui_id id = 0;
    if (lua_istable(L, 6)) {
        alpha = number_field(L, 6, "alpha", -1.0f);
        modal = bool_field(L, 6, "modal", false);
        id = read_widget_id(L, 6, label);
    }
    const bool opened = flux::widgets::begin_popup(
        label, x, y, width, height, alpha, id, modal);
    lua_pushboolean(L, opened ? 1 : 0);
    if (!opened) {
        lua_pushstring(L, "popup is not open");
        return 2;
    }
    return 1;
}

int l_end_popup(lua_State* L) {
    (void)L;
    const bool ok = flux::widgets::end_popup();
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok) {
        const std::string& error = flux::widgets::widget_runtime().frame_error;
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    return 1;
}

int l_popup_open(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    bool modal = false;
    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "modal");
        modal = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
    }
    flux::app::current().ui.open_popup(read_widget_id(L, 2, label), modal);
    return 0;
}

int l_popup_close(lua_State* L) {
    flux::ui::ui_id id = 0;
    if (lua_isstring(L, 1)) {
        id = flux::app::current().ui.make_id(lua_string_view(L, 1));
    } else if (lua_istable(L, 1)) {
        id = read_widget_id(L, 1, nullptr);
    }
    flux::app::current().ui.close_popup(id);
    return 0;
}

int l_popup_is_open(lua_State* L) {
    flux::ui::ui_id id = 0;
    if (lua_isstring(L, 1)) {
        id = flux::app::current().ui.make_id(lua_string_view(L, 1));
    } else if (lua_istable(L, 1)) {
        id = read_widget_id(L, 1, nullptr);
    }
    lua_pushboolean(L, flux::app::current().ui.popup_is_open(id) ? 1 : 0);
    return 1;
}

int l_menu_item(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    const char* shortcut = "";
    bool enabled = true;
    bool checked = false;
    bool checkable = false;
    flux::ui::ui_id id = 0;
    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "shortcut");
        shortcut = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
        lua_pop(L, 1);
        enabled = bool_field(L, 2, "enabled", true);
        checked = bool_field(L, 2, "checked", false);
        checkable = bool_field(L, 2, "checkable", false);
        id = read_widget_id(L, 2, label);
    }
    lua_pushboolean(L,
        flux::widgets::menu_item(label, shortcut, enabled, checked, checkable, id) ? 1 : 0);
    return 1;
}

int l_begin_menu(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    flux::widgets::menu_begin_options options;
    if (lua_istable(L, 2)) {
        options.width = number_field(L, 2, "w", options.width);
        options.height = number_field(L, 2, "h", options.height);
        options.enabled = bool_field(L, 2, "enabled", true);
        options.id = read_widget_id(L, 2, label);
    }
    lua_pushboolean(L, flux::widgets::begin_menu(label, options) ? 1 : 0);
    return 1;
}

int l_end_menu(lua_State* L) {
    (void)L;
    const bool ok = flux::widgets::end_menu();
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok) {
        const std::string& error = flux::widgets::widget_runtime().frame_error;
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    return 1;
}

int l_begin_form_row(lua_State* L) {
    float label_width = static_cast<float>(luaL_checknumber(L, 1));
    float gap = static_cast<float>(luaL_optnumber(L, 2, -1.0));
    float row_height = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    flux::runtime::services().ui.begin_form_row(label_width, gap, row_height);
    return 0;
}

int l_form_label(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    uint32_t color = static_cast<uint32_t>(luaL_optinteger(L, 2, 0));
    float size = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    if (size <= 0.0f) {
        size = flux::runtime::services().ui.style().font_size;
    }
    float width = 0.0f;
    float height = 0.0f;
    flux::runtime::services().measure_text(text, size, width, height);
    flux::ui::ui_context& ui = flux::runtime::services().ui;
    float y = ui.form_row_y();
    const float row_height = ui.form_row_height();
    if (row_height > 0.0f) {
        y += std::max(0.0f, (row_height - height) * 0.5f);
    }
    ui.set_cursor_pos(ui.form_label_x(), y);
    flux::widgets::label(text, color, size, 0.0f, false, 1);
    ui.form_align();
    return 0;
}

int l_form_align(lua_State* L) {
    float x = static_cast<float>(luaL_optnumber(L, 1, -1.0));
    flux::runtime::services().ui.form_align(x);
    return 0;
}

int l_end_form_row(lua_State* L) {
    flux::ui::ui_context& context = flux::runtime::services().ui;
    const bool ok = context.end_form_row();
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok) {
        const std::string& error = context.last_error();
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    return 1;
}

int l_menu_measure(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    flux::widgets::menu_measure_options options;
    lua_getfield(L, 1, "items"); options.items = static_cast<int>(luaL_optinteger(L, -1, 0)); lua_pop(L, 1);
    lua_getfield(L, 1, "separators"); options.separators = static_cast<int>(luaL_optinteger(L, -1, 0)); lua_pop(L, 1);
    lua_getfield(L, 1, "text_rows"); options.text_rows = static_cast<int>(luaL_optinteger(L, -1, 0)); lua_pop(L, 1);
    lua_getfield(L, 1, "text_row_height"); options.text_row_height = static_cast<float>(luaL_optnumber(L, -1, 18.0)); lua_pop(L, 1);
    lua_getfield(L, 1, "extra_height"); options.extra_height = static_cast<float>(luaL_optnumber(L, -1, 0.0)); lua_pop(L, 1);
    lua_pushnumber(L, flux::widgets::menu_measure(options));
    return 1;
}

int l_menu_separator(lua_State* L) {
    (void)L;
    flux::widgets::menu_separator();
    return 0;
}

int l_popup_hovered(lua_State* L) {
    (void)L;
    lua_pushboolean(L, flux::widgets::popup_hovered() ? 1 : 0);
    return 1;
}

int l_tooltip(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float y = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float delay = static_cast<float>(luaL_optnumber(L, 4, 0.35));
    float max_width = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    flux::widgets::tooltip(text, x, y, delay, max_width);
    return 0;
}

int l_rect_tooltip(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float width = static_cast<float>(luaL_checknumber(L, 4));
    float height = static_cast<float>(luaL_checknumber(L, 5));
    flux::widgets::tooltip_options options;
    read_tooltip_options(L, 6, options);
    flux::widgets::rect_tooltip(text, x, y, width, height, options);
    return 0;
}

int l_begin_table(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const int columns = static_cast<int>(lua_rawlen(L, 2));
    const float width = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    const float height = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    float header_height = 30.0f;
    flux::ui::ui_id id = 0;
    if (lua_istable(L, 6)) {
        header_height = number_field(L, 6, "header_height", 30.0f);
        id = read_widget_id(L, 6, label);
    }

    std::vector<const char*> pointers;
    pointers.reserve(static_cast<size_t>(columns));
    for (int i = 1; i <= columns; ++i) {
        lua_rawgeti(L, 2, i);
        pointers.push_back(luaL_checkstring(L, -1));
        lua_pop(L, 1);
    }

    std::vector<float> widths;
    if (lua_istable(L, 3)) {
        const int count = static_cast<int>(lua_rawlen(L, 3));
        if (count == columns) {
            widths.reserve(static_cast<size_t>(count));
            for (int i = 1; i <= count; ++i) {
                lua_rawgeti(L, 3, i);
                widths.push_back(static_cast<float>(luaL_checknumber(L, -1)));
                lua_pop(L, 1);
            }
        }
    }

    if (columns <= 0) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "table needs at least one column");
        return 2;
    }
    bool has_widths = !widths.empty();
    for (const float column_width : widths) {
        if (column_width <= 0.0f) {
            has_widths = false;
            break;
        }
    }
    if (width <= 0.0f && !has_widths) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "table needs a positive width or column widths");
        return 2;
    }

    const bool opened = flux::widgets::begin_table(
        label, pointers.data(), widths.empty() ? nullptr : widths.data(),
        columns, width, height, header_height, id);
    lua_pushboolean(L, opened ? 1 : 0);
    if (!opened) {
        lua_pushstring(L, "begin_table failed");
        return 2;
    }
    return 1;
}

int l_table_next_row(lua_State* L) {
    const int index = static_cast<int>(luaL_checkinteger(L, 1));
    const bool selected = lua_toboolean(L, 2) != 0;
    bool clicked = false;
    bool double_clicked = false;
    bool right_clicked = false;
    flux::widgets::table_next_row(index, selected, clicked, double_clicked, right_clicked);
    lua_pushboolean(L, clicked ? 1 : 0);
    lua_pushboolean(L, double_clicked ? 1 : 0);
    lua_pushboolean(L, right_clicked ? 1 : 0);
    return 3;
}

int l_table_cell_text(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    const int align = static_cast<int>(luaL_optinteger(L, 2, 0));
    flux::widgets::table_cell_text(text, align);
    return 0;
}

int l_end_table(lua_State* L) {
    (void)L;
    const bool ok = flux::widgets::end_table();
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok) {
        const std::string& error = flux::widgets::widget_runtime().frame_error;
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    return 1;
}

int l_input_text(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    const char* value = luaL_optstring(L, 2, "");
    float width = 0.0f;
    float height = 0.0f;
    bool password = false;
    bool enabled = true;
    flux::ui::ui_id id = 0;
    if (lua_istable(L, 3)) {
        width = number_field(L, 3, "w", number_field(L, 3, "width", 0.0f));
        height = number_field(L, 3, "h", number_field(L, 3, "height", 0.0f));
        password = bool_field(L, 3, "password", false);
        enabled = bool_field(L, 3, "enabled", true);
        id = read_widget_id(L, 3, label);
    }

    flux::widgets::text_input_result result =
        flux::widgets::input_text(label, value, width, height, password, enabled, id);
    lua_pushboolean(L, result.changed ? 1 : 0);
    lua_pushlstring(L, result.value.data(), result.value.size());
    lua_pushboolean(L, result.submitted ? 1 : 0);
    lua_pushboolean(L, result.deactivated ? 1 : 0);
    lua_pushboolean(L, result.focused ? 1 : 0);
    return 5;
}

int l_input_int(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    const int value = static_cast<int>(luaL_checkinteger(L, 2));
    float width = 0.0f;
    float height = 0.0f;
    bool enabled = true;
    int step = 1;
    flux::ui::ui_id id = 0;
    if (lua_istable(L, 3)) {
        width = number_field(L, 3, "w", number_field(L, 3, "width", 0.0f));
        height = number_field(L, 3, "h", number_field(L, 3, "height", 0.0f));
        enabled = bool_field(L, 3, "enabled", true);
        step = integer_field(L, 3, "step", 1);
        id = read_widget_id(L, 3, label);
    }

    flux::widgets::int_input_result result =
        flux::widgets::input_int(label, value, width, height, enabled, step, id);
    lua_pushboolean(L, result.changed ? 1 : 0);
    lua_pushinteger(L, result.value);
    lua_pushboolean(L, result.submitted ? 1 : 0);
    lua_pushboolean(L, result.deactivated ? 1 : 0);
    lua_pushboolean(L, result.focused ? 1 : 0);
    return 5;
}

int l_input_float(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    const double value = luaL_checknumber(L, 2);
    float width = 0.0f;
    float height = 0.0f;
    bool enabled = true;
    double step = 1.0;
    flux::ui::ui_id id = 0;
    if (lua_istable(L, 3)) {
        width = number_field(L, 3, "w", number_field(L, 3, "width", 0.0f));
        height = number_field(L, 3, "h", number_field(L, 3, "height", 0.0f));
        enabled = bool_field(L, 3, "enabled", true);
        step = number_field(L, 3, "step", 1.0f);
        id = read_widget_id(L, 3, label);
    }

    flux::widgets::float_input_result result =
        flux::widgets::input_float(label, value, width, height, enabled, step, id);
    lua_pushboolean(L, result.changed ? 1 : 0);
    lua_pushnumber(L, result.value);
    lua_pushboolean(L, result.submitted ? 1 : 0);
    lua_pushboolean(L, result.deactivated ? 1 : 0);
    lua_pushboolean(L, result.focused ? 1 : 0);
    return 5;
}

int l_combo(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    int selected = static_cast<int>(luaL_checkinteger(L, 2));
    luaL_checktype(L, 3, LUA_TTABLE);
    float width = 0.0f;
    float height = 0.0f;
    bool enabled = true;
    flux::ui::ui_id id = 0;
    if (lua_istable(L, 4)) {
        width = number_field(L, 4, "w", number_field(L, 4, "width", 0.0f));
        height = number_field(L, 4, "h", number_field(L, 4, "height", 0.0f));
        enabled = bool_field(L, 4, "enabled", true);
        id = read_widget_id(L, 4, label);
    }

    const int count = static_cast<int>(lua_rawlen(L, 3));
    std::vector<std::string_view> labels;
    std::vector<const char*> pointers;
    std::vector<uint8_t> disabled;
    labels.reserve(static_cast<size_t>(count));
    pointers.reserve(static_cast<size_t>(count));
    disabled.reserve(static_cast<size_t>(count));
    for (int i = 1; i <= count; ++i) {
        lua_rawgeti(L, 3, i);
        uint8_t item_disabled = 0;
        if (lua_isstring(L, -1)) {
            labels.push_back(lua_string_view(L, -1));
        } else if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "label");
            if (!lua_isstring(L, -1)) {
                lua_pop(L, 1);
                lua_rawgeti(L, -1, 1);
            }
            if (!lua_isstring(L, -1)) {
                luaL_error(L, "combo item %d must be a string or { label = ... }", i);
            }
            labels.push_back(lua_string_view(L, -1));
            lua_pop(L, 1);
            lua_getfield(L, -1, "disabled");
            item_disabled = lua_toboolean(L, -1) != 0 ? 1 : 0;
            lua_pop(L, 1);
        } else {
            luaL_error(L, "combo item %d must be a string or table", i);
        }
        lua_pop(L, 1);
        disabled.push_back(item_disabled);
    }
    for (std::string_view item : labels) {
        pointers.push_back(item.data());
    }

    flux::widgets::combo_result result = flux::widgets::combo(
        label, selected - 1, pointers.data(),
        disabled.empty() ? nullptr : disabled.data(), count, width, height, enabled, id);
    lua_pushboolean(L, result.changed ? 1 : 0);
    lua_pushinteger(L, result.selected + 1);
    lua_pushboolean(L, result.open ? 1 : 0);
    return 3;
}

int l_combo_is_open(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    const flux::ui::ui_id id = read_widget_id(L, 2, label);
    lua_pushboolean(L, flux::widgets::combo_is_open(id) ? 1 : 0);
    return 1;
}

int l_combo_set_open(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    const bool open = lua_toboolean(L, 2) != 0;
    const flux::ui::ui_id id = read_widget_id(L, 3, label);
    flux::widgets::combo_set_open(id, open);
    return 0;
}

int l_button(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    const float width = static_cast<float>(luaL_checknumber(L, 2));
    const float height = static_cast<float>(luaL_checknumber(L, 3));
    bool enabled = true;
    bool primary = false;
    flux::ui::ui_id id = 0;
    if (lua_istable(L, 4)) {
        enabled = bool_field(L, 4, "enabled", true);
        primary = bool_field(L, 4, "primary", false);
        id = read_widget_id(L, 4, label);
    }
    lua_pushboolean(L, flux::widgets::button(label, width, height, enabled, primary, id) ? 1 : 0);
    return 1;
}

int l_checkbox(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    bool checked = lua_toboolean(L, 2) != 0;
    float width = 0.0f;
    float height = 0.0f;
    bool enabled = true;
    flux::ui::ui_id id = 0;
    if (lua_istable(L, 3)) {
        width = number_field(L, 3, "w", number_field(L, 3, "width", 0.0f));
        height = number_field(L, 3, "h", number_field(L, 3, "height", 0.0f));
        enabled = bool_field(L, 3, "enabled", true);
        id = read_widget_id(L, 3, label);
    }
    bool changed = flux::widgets::checkbox(label, checked, width, height, enabled, id);
    lua_pushboolean(L, changed ? 1 : 0);
    lua_pushboolean(L, checked ? 1 : 0);
    return 2;
}

int l_selectable(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    bool selected = lua_toboolean(L, 2) != 0;
    float width = 0.0f;
    float height = 0.0f;
    bool enabled = true;
    flux::ui::ui_id id = 0;
    if (lua_istable(L, 3)) {
        width = number_field(L, 3, "w", number_field(L, 3, "width", 0.0f));
        height = number_field(L, 3, "h", number_field(L, 3, "height", 0.0f));
        enabled = bool_field(L, 3, "enabled", true);
        id = read_widget_id(L, 3, label);
    }
    lua_pushboolean(L, flux::widgets::selectable(label, selected, width, height, enabled, id) ? 1 : 0);
    return 1;
}

int l_label(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    uint32_t color = static_cast<uint32_t>(luaL_optinteger(L, 2, 0));
    float size = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float width = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    bool wrap = optional_bool(L, 5, false);
    int max_lines = static_cast<int>(luaL_optinteger(L, 6, 0));
    flux::widgets::label(text, color, size, width, wrap, max_lines);
    return 0;
}

int l_progress_bar(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    const float fraction = static_cast<float>(luaL_checknumber(L, 2));
    const float width = static_cast<float>(luaL_checknumber(L, 3));
    float height = 0.0f;
    bool indeterminate = false;
    flux::widgets::progress_bar_options options;
    if (lua_istable(L, 4)) {
        height = number_field(L, 4, "h", number_field(L, 4, "height", height));
        indeterminate = bool_field(L, 4, "indeterminate", indeterminate);
        options.id = read_widget_id(L, 4, label);
    } else {
        options.id = flux::runtime::services().ui.make_id(label);
    }
    flux::widgets::progress_bar(label, fraction, width, height, indeterminate, options);
    return 0;
}

int l_separator(lua_State* L) {
    float width = static_cast<float>(luaL_checknumber(L, 1));
    flux::widgets::separator(width);
    return 0;
}

int l_spacing(lua_State* L) {
    float height = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    flux::widgets::spacing(height);
    return 0;
}

int l_begin_list(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    const int count = static_cast<int>(luaL_checkinteger(L, 2));
    const float row_height = static_cast<float>(luaL_checknumber(L, 3));
    const float width = static_cast<float>(luaL_checknumber(L, 4));
    const float height = static_cast<float>(luaL_checknumber(L, 5));
    flux::widgets::selection_mode mode = flux::widgets::selection_mode::single;
    flux::ui::ui_id id = flux::runtime::services().ui.make_id(label);
    if (lua_istable(L, 6)) {
        lua_getfield(L, 6, "mode");
        if (lua_isstring(L, -1)) {
            const char* value = lua_tostring(L, -1);
            if (std::strcmp(value, "multiple") == 0) {
                mode = flux::widgets::selection_mode::multiple;
            } else if (std::strcmp(value, "extended") == 0) {
                mode = flux::widgets::selection_mode::extended;
            }
        }
        lua_pop(L, 1);
        id = read_stable_id(L, 6, label);
    }
    const flux::widgets::begin_list_result result =
        flux::widgets::begin_list(label, count, row_height, width, height, mode, id);
    if (!result.active || result.count <= 0) {
        lua_pushboolean(L, 0);
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
        return 3;
    }
    lua_pushboolean(L, 1);
    lua_pushinteger(L, result.first + 1);
    lua_pushinteger(L, result.last + 1);
    return 3;
}

int l_list_item(lua_State* L) {
    const int index = static_cast<int>(luaL_checkinteger(L, 1)) - 1;
    const flux::widgets::list_item_result result =
        flux::widgets::list_item(index);
    lua_pushboolean(L, result.clicked ? 1 : 0);
    lua_pushboolean(L, result.double_clicked ? 1 : 0);
    lua_pushboolean(L, result.right_clicked ? 1 : 0);
    lua_pushboolean(L, result.selected ? 1 : 0);
    lua_pushboolean(L, result.changed ? 1 : 0);
    return 5;
}

int l_list_item_ex(lua_State* L) {
    const int index = static_cast<int>(luaL_checkinteger(L, 1)) - 1;
    luaL_checktype(L, 2, LUA_TTABLE);
    flux::widgets::list_item_options options;
    lua_getfield(L, 2, "focusable");
    options.focusable = lua_isnil(L, -1) ? true : lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    lua_getfield(L, 2, "tab_stop");
    options.tab_stop = lua_isnil(L, -1) ? options.focusable : lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    lua_getfield(L, 2, "chrome");
    options.chrome = lua_isnil(L, -1) ? true : lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    const flux::widgets::list_item_result result =
        flux::widgets::list_item_ex(index, options);
    lua_pushboolean(L, result.clicked ? 1 : 0);
    lua_pushboolean(L, result.double_clicked ? 1 : 0);
    lua_pushboolean(L, result.right_clicked ? 1 : 0);
    lua_pushboolean(L, result.selected ? 1 : 0);
    lua_pushboolean(L, result.changed ? 1 : 0);
    lua_pushboolean(L, result.hovered ? 1 : 0);
    lua_pushboolean(L, result.active ? 1 : 0);
    lua_pushboolean(L, result.focused ? 1 : 0);
    return 8;
}

int l_end_list(lua_State* L) {
    (void)L;
    const bool ok = flux::widgets::end_list();
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok) {
        const std::string& error = flux::widgets::widget_runtime().frame_error;
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    return 1;
}

int l_list_selected(lua_State* L) {
    const flux::ui::ui_id id = list_id(L, 1);
    const int index = static_cast<int>(luaL_checkinteger(L, 2)) - 1;
    lua_pushboolean(L, flux::widgets::list_selected(id, index) ? 1 : 0);
    return 1;
}

int l_list_set_selected(lua_State* L) {
    const flux::ui::ui_id id = list_id(L, 1);
    const int index = static_cast<int>(luaL_optinteger(L, 2, 0)) - 1;
    if (!flux::widgets::list_exists(id)) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "list not found");
        return 2;
    }
    lua_pushboolean(L, 1);
    lua_pushboolean(L, flux::widgets::list_set_selected(id, index) ? 1 : 0);
    return 2;
}

int l_list_selected_count(lua_State* L) {
    lua_pushinteger(L, flux::widgets::list_selected_count(list_id(L, 1)));
    return 1;
}

int l_list_selection_changed(lua_State* L) {
    lua_pushboolean(L, flux::widgets::list_selection_changed(list_id(L, 1)) ? 1 : 0);
    return 1;
}

int l_list_clear_selection(lua_State* L) {
    const flux::ui::ui_id id = list_id(L, 1);
    if (!flux::widgets::list_exists(id)) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "list not found");
        return 2;
    }
    lua_pushboolean(L, 1);
    lua_pushboolean(L, flux::widgets::list_clear_selection(id) ? 1 : 0);
    return 2;
}

int l_list_select_all(lua_State* L) {
    const flux::ui::ui_id id = list_id(L, 1);
    if (!flux::widgets::list_exists(id)) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "list not found");
        return 2;
    }
    lua_pushboolean(L, 1);
    lua_pushboolean(L, flux::widgets::list_select_all(id) ? 1 : 0);
    return 2;
}

int l_list_focused(lua_State* L) {
    const int index = flux::widgets::list_focused(list_id(L, 1));
    lua_pushinteger(L, index >= 0 ? index + 1 : 0);
    return 1;
}

int l_list_scroll_to(lua_State* L) {
    const flux::ui::ui_id id = list_id(L, 1);
    const int index = static_cast<int>(luaL_checkinteger(L, 2)) - 1;
    const flux::widgets::list_scroll_align align = scroll_align_from(L, 3);
    if (!flux::widgets::list_scroll_to(id, index, align)) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "list is not active or empty");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int l_control_anim(lua_State* L) {
    flux::ui::ui_id id = 0;
    if (lua_isinteger(L, 1)) {
        id = static_cast<flux::ui::ui_id>(lua_tointeger(L, 1));
    } else {
        id = flux::runtime::services().ui.make_id(lua_check_string_view(L, 1));
    }
    luaL_checktype(L, 2, LUA_TTABLE);
    flux::widgets::control_anim_targets targets;
    lua_getfield(L, 2, "hovered");
    targets.hovered = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    lua_getfield(L, 2, "active");
    targets.active = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    lua_getfield(L, 2, "focused");
    targets.focused = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    lua_getfield(L, 2, "selected");
    targets.has_value = true;
    targets.value = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    lua_getfield(L, 2, "snap");
    targets.snap = lua_isnil(L, -1) ? true : lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    const flux::widgets::control_anim_state& anim = flux::widgets::control_anim(
        id, targets, flux::runtime::services().ui.frame_index());
    lua_pushnumber(L, anim.hover);
    lua_pushnumber(L, anim.press);
    lua_pushnumber(L, anim.focus);
    lua_pushnumber(L, anim.value);
    return 4;
}

int l_mix_color(lua_State* L) {
    const uint32_t from = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const uint32_t to = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    const float t = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushinteger(L, static_cast<lua_Integer>(flux::widgets::mix_color(from, to, t)));
    return 1;
}

}

void push_widget_funcs(lua_State* L) {
    static const luaL_Reg functions[] = {
        { "tab_bar", l_tab_bar },
        { "popup_place", l_popup_place },
        { "begin_popup", l_begin_popup },
        { "end_popup", l_end_popup },
        { "popup_open", l_popup_open },
        { "popup_close", l_popup_close },
        { "popup_is_open", l_popup_is_open },
        { "menu_item", l_menu_item },
        { "begin_menu", l_begin_menu },
        { "end_menu", l_end_menu },
        { "menu_separator", l_menu_separator },
        { "menu_measure", l_menu_measure },
        { "begin_form_row", l_begin_form_row },
        { "form_label", l_form_label },
        { "form_align", l_form_align },
        { "end_form_row", l_end_form_row },
        { "popup_hovered", l_popup_hovered },
        { "tooltip", l_tooltip },
        { "rect_tooltip", l_rect_tooltip },
        { "begin_table", l_begin_table },
        { "table_next_row", l_table_next_row },
        { "table_cell_text", l_table_cell_text },
        { "end_table", l_end_table },
        { "begin_list", l_begin_list },
        { "list_item", l_list_item },
        { "list_item_ex", l_list_item_ex },
        { "end_list", l_end_list },
        { "list_selected", l_list_selected },
        { "list_set_selected", l_list_set_selected },
        { "list_selected_count", l_list_selected_count },
        { "list_selection_changed", l_list_selection_changed },
        { "list_clear_selection", l_list_clear_selection },
        { "list_select_all", l_list_select_all },
        { "list_focused", l_list_focused },
        { "list_scroll_to", l_list_scroll_to },
        { "control_anim", l_control_anim },
        { "mix_color", l_mix_color },
        { "input_text", l_input_text },
        { "input_int", l_input_int },
        { "input_float", l_input_float },
        { "combo", l_combo },
        { "combo_is_open", l_combo_is_open },
        { "combo_set_open", l_combo_set_open },
        { "button", l_button },
        { "checkbox", l_checkbox },
        { "selectable", l_selectable },
        { "label", l_label },
        { "progress_bar", l_progress_bar },
        { "separator", l_separator },
        { "spacing", l_spacing },
        { nullptr, nullptr },
    };
    luaL_setfuncs(L, functions, 0);
}

}
