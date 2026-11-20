#pragma once

#include <cstdint>

namespace flux::ui {

enum style_color_index {
    style_color_text = 0,
    style_color_text_dim,
    style_color_panel,
    style_color_panel_hover,
    style_color_panel_active,
    style_color_border,
    style_color_accent,
    style_color_accent_hover,
    style_color_accent_active,
    style_color_white,
    style_color_disabled,
    style_color_track,
    style_color_popup_bg,
    style_color_popup_border,
    style_color_popup_shadow,
    style_color_menu_text,
    style_color_menu_shortcut,
    style_color_menu_item_hover,
    style_color_menu_separator,
    style_color_tooltip_bg,
    style_color_tooltip_border,
    style_color_tooltip_shadow,
    style_color_tab_bar_bg,
    style_color_tab_bar_border,
    style_color_tab_bg,
    style_color_tab_bg_hover,
    style_color_tab_bg_selected,
    style_color_tab_text,
    style_color_tab_text_selected,
    style_color_tab_baseline,
    style_color_tab_focus,
};

enum style_var_index {
    style_var_item_spacing_x = 100,
    style_var_item_spacing_y,
    style_var_frame_padding_x,
    style_var_frame_padding_y,
    style_var_rounding,
    style_var_border_size,
    style_var_scrollbar_size,
    style_var_font_size,
    style_var_line_height,
    style_var_popup_padding_x,
    style_var_popup_padding_y,
    style_var_popup_shadow_spread,
    style_var_popup_shadow_offset_y,
    style_var_menu_item_height,
    style_var_menu_item_padding_x,
    style_var_menu_separator_gap,
    style_var_menu_separator_height,
    style_var_menu_header_line_height,
    style_var_combo_item_height,
    style_var_combo_popup_padding,
    style_var_combo_popup_max_height,
    style_var_combo_popup_gap,
    style_var_combo_popup_min_width,
    style_var_tooltip_padding_x,
    style_var_tooltip_padding_y,
    style_var_tooltip_shadow_spread,
    style_var_tooltip_shadow_offset_y,
    style_var_tab_height,
    style_var_tab_min_width,
    style_var_tab_gap,
    style_var_tab_padding_x,
    style_var_tab_rounding,
    style_var_tab_baseline_size,
    style_var_tab_font_size,
    style_var_scrollbar_idle_alpha,
};

struct style_sheet {
    uint32_t text = 0xFFF2F2F2;
    uint32_t text_dim = 0xFFB4B4B4;
    uint32_t panel = 0xA62A2A2A;
    uint32_t panel_hover = 0xC2333333;
    uint32_t panel_active = 0xD23B3B3B;
    uint32_t border = 0x26FFFFFF;
    uint32_t accent = 0xFF3C8AE0;
    uint32_t accent_hover = 0xFF5A9BEA;
    uint32_t accent_active = 0xFF2F74C0;
    uint32_t white = 0xFFFFFFFF;
    uint32_t disabled = 0xFF6E6E6E;
    uint32_t track = 0x33000000;
    uint32_t popup_bg = 0xA62A2A2A;
    uint32_t popup_border = 0x26FFFFFF;
    uint32_t popup_shadow = 0x40000000;
    uint32_t menu_text = 0xFFF2F2F2;
    uint32_t menu_shortcut = 0xFFB4B4B4;
    uint32_t menu_item_hover = 0xC2333333;
    uint32_t menu_separator = 0x26FFFFFF;
    uint32_t tooltip_bg = 0xF0202020;
    uint32_t tooltip_border = 0x26FFFFFF;
    uint32_t tooltip_shadow = 0x66000000;
    uint32_t tab_bar_bg = 0x00000000;
    uint32_t tab_bar_border = 0x00000000;
    uint32_t tab_bg = 0xA62A2A2A;
    uint32_t tab_bg_hover = 0xC2333333;
    uint32_t tab_bg_selected = 0xD23B3B3B;
    uint32_t tab_text = 0xFFB4B4B4;
    uint32_t tab_text_selected = 0xFFF2F2F2;
    uint32_t tab_baseline = 0x26FFFFFF;
    uint32_t tab_focus = 0xFF3C8AE0;
    float item_spacing_x = 8.0f;
    float item_spacing_y = 6.0f;
    float frame_padding_x = 10.0f;
    float frame_padding_y = 4.0f;
    float rounding = 6.0f;
    float border_size = 1.0f;
    float scrollbar_size = 10.0f;
    float font_size = 14.0f;
    float line_height = 0.0f;
    float popup_padding_x = 10.0f;
    float popup_padding_y = 4.0f;
    float popup_shadow_spread = 4.0f;
    float popup_shadow_offset_y = 2.0f;
    float menu_item_height = 28.0f;
    float menu_item_padding_x = 10.0f;
    float menu_separator_gap = 4.0f;
    float menu_separator_height = 9.0f;
    float menu_header_line_height = 18.0f;
    float combo_item_height = 24.0f;
    float combo_popup_padding = 6.0f;
    float combo_popup_max_height = 260.0f;
    float combo_popup_gap = 4.0f;
    float combo_popup_min_width = 72.0f;
    float tooltip_padding_x = 8.0f;
    float tooltip_padding_y = 5.0f;
    float tooltip_shadow_spread = 7.0f;
    float tooltip_shadow_offset_y = 3.0f;
    float tab_height = 28.0f;
    float tab_min_width = 36.0f;
    float tab_gap = 4.0f;
    float tab_padding_x = 6.0f;
    float tab_rounding = 4.0f;
    float tab_baseline_size = 1.0f;
    float tab_font_size = 0.0f;
    float scrollbar_idle_alpha = 0.0f;
};

}
