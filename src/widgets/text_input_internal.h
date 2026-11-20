#pragma once

#include <string>

#include "ui/types.h"
#include "widgets/widget_runtime.h"
#include "widgets/widgets.h"

namespace flux::widgets {

text_input_result text_input_core(const char* label, std::string& text,
                                  float width, float height, bool password,
                                  bool enabled, edit_state& state,
                                  bool sync_external, flux::ui::ui_id id);

void text_input_push_undo(edit_state& state, const std::string& text);

}
