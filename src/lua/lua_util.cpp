#include "lua/lua_util.h"

#include "runtime/services.h"
#include "lua/lua_string.h"

namespace flux::lua {

flux::ui::ui_id read_widget_id(lua_State* L, int index, const char* label) {
    if (lua_istable(L, index)) {
        lua_getfield(L, index, "id");
        if (lua_isinteger(L, -1)) {
            const flux::ui::ui_id id = static_cast<flux::ui::ui_id>(lua_tointeger(L, -1));
            lua_pop(L, 1);
            return id;
        }
        if (lua_isstring(L, -1)) {
            const flux::ui::ui_id id = flux::runtime::services().ui.make_id(lua_string_view(L, -1));
            lua_pop(L, 1);
            return id;
        }
        lua_pop(L, 1);
    }
    return label != nullptr ? flux::runtime::services().ui.make_id(label) : 0;
}

flux::ui::ui_id read_stable_id(lua_State* L, int index, const char* label) {
    if (lua_istable(L, index)) {
        lua_getfield(L, index, "id");
        if (lua_isinteger(L, -1)) {
            const flux::ui::ui_id id = static_cast<flux::ui::ui_id>(lua_tointeger(L, -1));
            lua_pop(L, 1);
            return id;
        }
        if (lua_isstring(L, -1)) {
            const flux::ui::ui_id id =
                flux::runtime::services().ui.make_id_absolute(lua_string_view(L, -1));
            lua_pop(L, 1);
            return id;
        }
        lua_pop(L, 1);
    }
    return label != nullptr
        ? flux::runtime::services().ui.make_id_absolute(label) : 0;
}

}
