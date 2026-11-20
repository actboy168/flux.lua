#pragma once

#include "ui/types.h"
#include "widgets/list_types.h"
#include "widgets/selection_model.h"

namespace flux::widgets {

struct begin_list_result {
    bool active = false;
    int count = 0;
    int first = 0;
    int last = -1;
    float scroll = 0.0f;
    float content_height = 0.0f;
};

struct list_item_result {
    bool visible = false;
    bool clicked = false;
    bool double_clicked = false;
    bool right_clicked = false;
    bool selected = false;
    bool changed = false;
    bool focused = false;
    bool hovered = false;
    bool active = false;
};

struct list_item_options {
    bool focusable = true;
    bool tab_stop = true;
    bool chrome = true;
};

begin_list_result begin_list(const char* label, int count, float row_height,
                             float width, float height,
                             selection_mode mode = selection_mode::single,
                             flux::ui::ui_id id = 0);
list_item_result list_item(int index);
list_item_result list_item_ex(int index, const list_item_options& options);
bool end_list();

bool list_exists(const char* label);
bool list_exists(flux::ui::ui_id id);
bool list_selected(const char* label, int index);
bool list_selected(flux::ui::ui_id id, int index);
bool list_set_selected(const char* label, int index);
bool list_set_selected(flux::ui::ui_id id, int index);
int list_selected_count(const char* label);
int list_selected_count(flux::ui::ui_id id);
bool list_selection_changed(const char* label);
bool list_selection_changed(flux::ui::ui_id id);
bool list_clear_selection(const char* label);
bool list_clear_selection(flux::ui::ui_id id);
bool list_select_all(const char* label);
bool list_select_all(flux::ui::ui_id id);
int list_focused(const char* label);
int list_focused(flux::ui::ui_id id);
bool list_scroll_to(const char* label, int index,
                    list_scroll_align align = list_scroll_align::top);
bool list_scroll_to(flux::ui::ui_id id, int index,
                    list_scroll_align align = list_scroll_align::top);

}
