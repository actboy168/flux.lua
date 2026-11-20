#pragma once

#include <cstdint>

#include "ui/types.h"
#include "widgets/widget_state_store.h"
#include "widgets/selection_model.h"

namespace flux::widgets {

enum class list_scroll_align {
    top,
    center,
    bottom,
};

struct list_state {
    static constexpr widget_state_kind kind = widget_state_kind::list;
    static constexpr bool persistent = true;

    selection_model selection;
    bool changed = false;
    int pending_scroll_index = -1;
    list_scroll_align pending_scroll_align = list_scroll_align::top;
    flux::ui::ui_id focus_row_id = 0;
};

struct list_scope {
    flux::ui::ui_id id = 0;
    list_state* state = nullptr;
    float x = 0.0f;
    float content_top = 0.0f;
    float row_height = 0.0f;
    float row_width = 0.0f;
    int count = 0;
    int first = 0;
    int last = -1;
};

}
