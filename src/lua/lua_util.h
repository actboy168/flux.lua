#pragma once

#include <lua.hpp>

#include "ui/types.h"

namespace flux::lua {

flux::ui::ui_id read_widget_id(lua_State* L, int index, const char* label);
flux::ui::ui_id read_stable_id(lua_State* L, int index, const char* label);

}
