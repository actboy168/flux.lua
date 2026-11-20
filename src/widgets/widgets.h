#pragma once

#include <cstdint>
#include <string>

#include "ui/types.h"

namespace flux::widgets {

struct text_input_result {
    bool changed = false;
    bool submitted = false;
    bool deactivated = false;
    bool focused = false;
    std::string value;
};

struct int_input_result {
    bool changed = false;
    bool submitted = false;
    bool deactivated = false;
    bool focused = false;
    int value = 0;
};

struct float_input_result {
    bool changed = false;
    bool submitted = false;
    bool deactivated = false;
    bool focused = false;
    double value = 0.0;
};

text_input_result input_text(const char* label, const char* value, float width, float height,
                             bool password, bool enabled, flux::ui::ui_id id = 0);
int_input_result input_int(const char* label, int value, float width, float height,
                           bool enabled, int step, flux::ui::ui_id id = 0);
float_input_result input_float(const char* label, double value, float width, float height,
                               bool enabled, double step, flux::ui::ui_id id = 0);

struct combo_result {
    bool changed = false;
    bool open = false;
    int selected = 0;
};

combo_result combo(const char* label, int selected, const char* const* items,
                   const uint8_t* disabled, int count, float width, float height, bool enabled,
                   flux::ui::ui_id id = 0);
float combo_reveal(const char* label, flux::ui::ui_id id = 0);
bool combo_is_open(flux::ui::ui_id id);
void combo_set_open(flux::ui::ui_id id, bool open);

struct tab_item_desc {
    const char* label = nullptr;
    const char* id = nullptr;
    float width = 0.0f;
    bool disabled = false;
};

struct tab_bar_options {
    flux::ui::ui_id id = 0;
    int selected = 1;
    float width = 0.0f;
    float height = 0.0f;
    float min_width = -1.0f;
    float gap = -1.0f;
    float padding_x = -1.0f;
    float rounding = -1.0f;
    bool equal_width = false;
    bool roving = true;
    bool draw_baseline = true;
};

struct tab_bar_result {
    int selected = 0;
    int clicked = 0;
    int focused = 0;
    bool changed = false;
};

tab_bar_result tab_bar(const char* label, const tab_item_desc* items, int count,
                       const tab_bar_options& options);
struct menu_measure_options {
    int items = 0;
    int separators = 0;
    int text_rows = 0;
    float text_row_height = 18.0f;
    float extra_height = 0.0f;
};

float menu_measure(const menu_measure_options& options);

struct popup_anchor {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct popup_place_options {
    float gap = 0.0f;
    float margin = 4.0f;
};

struct popup_rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

popup_rect place_popup(const popup_anchor& anchor, float width, float height,
                       const popup_place_options& options,
                       float viewport_width, float viewport_height);

bool begin_popup(const char* label, float x, float y, float width, float height,
                 float alpha = -1.0f, flux::ui::ui_id id = 0, bool modal = false);
bool end_popup();
bool menu_item(const char* label, const char* shortcut, bool enabled, bool checked,
               bool checkable = false, flux::ui::ui_id id = 0);
struct menu_begin_options {
    float width = 160.0f;
    float height = 120.0f;
    bool enabled = true;
    flux::ui::ui_id id = 0;
};
bool begin_menu(const char* label, const menu_begin_options& options = {});
bool end_menu();
void menu_separator();
bool popup_hovered();
struct tooltip_options {
    float delay = 0.35f;
    float max_width = 0.0f;
    float offset_x = 16.0f;
    float offset_y = 10.0f;
    bool follow_mouse = true;
};
void tooltip(const char* text, float x, float y, float delay = 0.35f,
             float max_width = 0.0f);
void rect_tooltip(const char* text, float x, float y, float width, float height,
                  const tooltip_options& options = {});
void flush_tooltips();
bool begin_table(const char* label, const char* const* headers, const float* widths,
                 int columns, float width, float height, float header_height,
                 flux::ui::ui_id id = 0);
void table_next_row(int index, bool selected, bool& clicked, bool& double_clicked,
                    bool& right_clicked);
void table_cell_text(const char* text, int align);
bool end_table();
bool button(const char* label, float width, float height, bool enabled, bool primary,
            flux::ui::ui_id id = 0);
bool checkbox(const char* label, bool& checked, float width, float height, bool enabled,
              flux::ui::ui_id id = 0);
bool selectable(const char* label, bool selected, float width, float height, bool enabled,
                flux::ui::ui_id id = 0);
void label(const char* text, uint32_t color, float size, float width, bool wrap,
           int max_lines);
struct progress_bar_options {
    flux::ui::ui_id id = 0;
};
void progress_bar(const char* label, float fraction, float width, float height,
                  bool indeterminate, const progress_bar_options& options = {});
void separator(float width);
void spacing(float height);

}
