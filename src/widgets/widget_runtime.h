#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ui/types.h"
#include "widgets/control_anim.h"
#include "widgets/list_types.h"
#include "widgets/popup_types.h"
#include "widgets/table_types.h"
#include "widgets/widget_state_store.h"

namespace flux::widgets {

struct edit_snapshot {
    std::string text;
    int caret = 0;
    int anchor = 0;
};

struct edit_state {
    static constexpr widget_state_kind kind = widget_state_kind::edit;

    bool known_set = false;
    std::string known;
    int caret = 0;
    int anchor = 0;
    float scroll = 0.0f;
    bool dragging = false;
    bool focused_last = false;
    bool session_edited = false;
    double last_edit_time = -100.0;
    bool coalescing = false;
    std::string focus_text;
    std::vector<edit_snapshot> undo;
    std::vector<edit_snapshot> redo;
    double caret_blink_start = 0.0;
    int caret_blink_caret = -1;
    bool number_init = false;
    std::string number_text;
    double number_committed = 0.0;
};

struct combo_state {
    static constexpr widget_state_kind kind = widget_state_kind::combo;

    bool open = false;
    int highlighted = -1;
    float reveal = 0.0f;
    float popup_x = 0.0f;
    float popup_y = 0.0f;
    float popup_w = 0.0f;
    float popup_h = 0.0f;
    std::string search;
    double search_time = -100.0;
};

struct progress_anim {
    static constexpr widget_state_kind kind = widget_state_kind::progress;

    bool initialized = false;
    float value = 0.0f;
};

struct control_anim_entry {
    control_anim_state anim;
    bool hover_armed = false;
    bool press_armed = false;
    bool focus_armed = false;
    bool value_armed = false;
    uint64_t last_frame = 0;
};

struct widget_runtime_state {
    widget_state_store states;
    flux::ui::ui_id combo_open_id = 0;
    std::unordered_map<flux::ui::ui_id, control_anim_entry> anims;
    uint64_t anim_enabled_frame = UINT64_MAX;
    bool anim_enabled = true;
    uint64_t prune_frame = UINT64_MAX;
    uint64_t animation_frame = 0;
    float tooltip_reveal = 0.0f;
    uint64_t tooltip_frame = 0;
    double tooltip_hover_start = -100.0;
    std::string tooltip_text;
    tooltip_request tooltip;
    std::vector<popup_scope> popup_scopes;
    std::vector<table_scope> table_scopes;
    std::vector<list_scope> list_scopes;
    std::vector<bool> menu_scopes;
    bool frame_ok = true;
    std::string frame_error;

    void reset();
    void fail(std::string_view message);
    bool animations_active(uint64_t frame) const;
};

widget_runtime_state& widget_runtime();
void widget_runtime_reset();
void widget_frame_begin();
bool widget_frame_end(std::string& error);

}
