#pragma once

#include <lua.hpp>

#include "app/context.h"
#include "ui/ui_context.h"

namespace flux::lua {

inline flux::ui::ui_context& ui() {
    return flux::app::current().ui;
}

inline flux::ui::input_state& input() {
    return ui().input;
}

int mouse_button(lua_State* L, int index);
int key_code(lua_State* L, int index);

}
