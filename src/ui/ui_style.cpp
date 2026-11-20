#include "ui/ui_context.h"

#include "ui/draw_list.h"
#include "ui/ui_internal.h"

namespace flux::ui {

const style_sheet& ui_context::style() const {
    return current_style_;
}

void ui_context::rebuild_style() {
    current_style_ = base_style_;
    for (const style_color_frame& frame : style_color_frames_) {
        apply_style_color(current_style_, frame);
    }
    for (const style_var_frame& frame : style_var_frames_) {
        apply_style_var(current_style_, frame);
    }
}

void ui_context::apply_style_color(style_sheet& style, const style_color_frame& frame) const {
    switch (frame.index) {
    case style_color_text: style.text = frame.color; break;
    case style_color_text_dim: style.text_dim = frame.color; break;
    case style_color_panel: style.panel = frame.color; break;
    case style_color_panel_hover: style.panel_hover = frame.color; break;
    case style_color_panel_active: style.panel_active = frame.color; break;
    case style_color_border: style.border = frame.color; break;
    case style_color_accent: style.accent = frame.color; break;
    case style_color_accent_hover: style.accent_hover = frame.color; break;
    case style_color_accent_active: style.accent_active = frame.color; break;
    case style_color_white: style.white = frame.color; break;
    case style_color_disabled: style.disabled = frame.color; break;
    case style_color_track: style.track = frame.color; break;
    case style_color_popup_bg: style.popup_bg = frame.color; break;
    case style_color_popup_border: style.popup_border = frame.color; break;
    case style_color_popup_shadow: style.popup_shadow = frame.color; break;
    case style_color_menu_text: style.menu_text = frame.color; break;
    case style_color_menu_shortcut: style.menu_shortcut = frame.color; break;
    case style_color_menu_item_hover: style.menu_item_hover = frame.color; break;
    case style_color_menu_separator: style.menu_separator = frame.color; break;
    case style_color_tooltip_bg: style.tooltip_bg = frame.color; break;
    case style_color_tooltip_border: style.tooltip_border = frame.color; break;
    case style_color_tooltip_shadow: style.tooltip_shadow = frame.color; break;
    case style_color_tab_bar_bg: style.tab_bar_bg = frame.color; break;
    case style_color_tab_bar_border: style.tab_bar_border = frame.color; break;
    case style_color_tab_bg: style.tab_bg = frame.color; break;
    case style_color_tab_bg_hover: style.tab_bg_hover = frame.color; break;
    case style_color_tab_bg_selected: style.tab_bg_selected = frame.color; break;
    case style_color_tab_text: style.tab_text = frame.color; break;
    case style_color_tab_text_selected: style.tab_text_selected = frame.color; break;
    case style_color_tab_baseline: style.tab_baseline = frame.color; break;
    case style_color_tab_focus: style.tab_focus = frame.color; break;
    default: break;
    }
}

void ui_context::apply_style_var(style_sheet& style, const style_var_frame& frame) const {
    switch (frame.index) {
    case style_var_item_spacing_x: style.item_spacing_x = frame.value; break;
    case style_var_item_spacing_y: style.item_spacing_y = frame.value; break;
    case style_var_frame_padding_x: style.frame_padding_x = frame.value; break;
    case style_var_frame_padding_y: style.frame_padding_y = frame.value; break;
    case style_var_rounding: style.rounding = frame.value; break;
    case style_var_border_size: style.border_size = frame.value; break;
    case style_var_scrollbar_size: style.scrollbar_size = frame.value; break;
    case style_var_font_size: style.font_size = frame.value; break;
    case style_var_line_height: style.line_height = frame.value; break;
    case style_var_popup_padding_x: style.popup_padding_x = frame.value; break;
    case style_var_popup_padding_y: style.popup_padding_y = frame.value; break;
    case style_var_popup_shadow_spread: style.popup_shadow_spread = frame.value; break;
    case style_var_popup_shadow_offset_y: style.popup_shadow_offset_y = frame.value; break;
    case style_var_menu_item_height: style.menu_item_height = frame.value; break;
    case style_var_menu_item_padding_x: style.menu_item_padding_x = frame.value; break;
    case style_var_menu_separator_gap: style.menu_separator_gap = frame.value; break;
    case style_var_menu_separator_height: style.menu_separator_height = frame.value; break;
    case style_var_menu_header_line_height: style.menu_header_line_height = frame.value; break;
    case style_var_combo_item_height: style.combo_item_height = frame.value; break;
    case style_var_combo_popup_padding: style.combo_popup_padding = frame.value; break;
    case style_var_combo_popup_max_height: style.combo_popup_max_height = frame.value; break;
    case style_var_combo_popup_gap: style.combo_popup_gap = frame.value; break;
    case style_var_combo_popup_min_width: style.combo_popup_min_width = frame.value; break;
    case style_var_tooltip_padding_x: style.tooltip_padding_x = frame.value; break;
    case style_var_tooltip_padding_y: style.tooltip_padding_y = frame.value; break;
    case style_var_tooltip_shadow_spread: style.tooltip_shadow_spread = frame.value; break;
    case style_var_tooltip_shadow_offset_y: style.tooltip_shadow_offset_y = frame.value; break;
    case style_var_tab_height: style.tab_height = frame.value; break;
    case style_var_tab_min_width: style.tab_min_width = frame.value; break;
    case style_var_tab_gap: style.tab_gap = frame.value; break;
    case style_var_tab_padding_x: style.tab_padding_x = frame.value; break;
    case style_var_tab_rounding: style.tab_rounding = frame.value; break;
    case style_var_tab_baseline_size: style.tab_baseline_size = frame.value; break;
    case style_var_tab_font_size: style.tab_font_size = frame.value; break;
    case style_var_scrollbar_idle_alpha: style.scrollbar_idle_alpha = frame.value; break;
    default: break;
    }
}

void ui_context::push_style_color(int index, uint32_t color) {
    style_color_frames_.push_back({index, color});
    rebuild_style();
}

void ui_context::push_style_var(int index, float value) {
    style_var_frames_.push_back({index, value});
    rebuild_style();
}

bool ui_context::pop_style_color(int count) {
    if (count <= 0 || style_color_frames_.size() < static_cast<size_t>(count)) {
        return false;
    }
    style_color_frames_.resize(style_color_frames_.size() - static_cast<size_t>(count));
    rebuild_style();
    return true;
}

bool ui_context::pop_style_var(int count) {
    if (count <= 0 || style_var_frames_.size() < static_cast<size_t>(count)) {
        return false;
    }
    style_var_frames_.resize(style_var_frames_.size() - static_cast<size_t>(count));
    rebuild_style();
    return true;
}

uint32_t ui_context::style_color(int index) const {
    const style_sheet& current = style();
    switch (index) {
    case style_color_text: return current.text;
    case style_color_text_dim: return current.text_dim;
    case style_color_panel: return current.panel;
    case style_color_panel_hover: return current.panel_hover;
    case style_color_panel_active: return current.panel_active;
    case style_color_border: return current.border;
    case style_color_accent: return current.accent;
    case style_color_accent_hover: return current.accent_hover;
    case style_color_accent_active: return current.accent_active;
    case style_color_white: return current.white;
    case style_color_disabled: return current.disabled;
    case style_color_track: return current.track;
    case style_color_popup_bg: return current.popup_bg;
    case style_color_popup_border: return current.popup_border;
    case style_color_popup_shadow: return current.popup_shadow;
    case style_color_menu_text: return current.menu_text;
    case style_color_menu_shortcut: return current.menu_shortcut;
    case style_color_menu_item_hover: return current.menu_item_hover;
    case style_color_menu_separator: return current.menu_separator;
    case style_color_tooltip_bg: return current.tooltip_bg;
    case style_color_tooltip_border: return current.tooltip_border;
    case style_color_tooltip_shadow: return current.tooltip_shadow;
    case style_color_tab_bar_bg: return current.tab_bar_bg;
    case style_color_tab_bar_border: return current.tab_bar_border;
    case style_color_tab_bg: return current.tab_bg;
    case style_color_tab_bg_hover: return current.tab_bg_hover;
    case style_color_tab_bg_selected: return current.tab_bg_selected;
    case style_color_tab_text: return current.tab_text;
    case style_color_tab_text_selected: return current.tab_text_selected;
    case style_color_tab_baseline: return current.tab_baseline;
    case style_color_tab_focus: return current.tab_focus;
    default: return 0;
    }
}

float ui_context::style_var(int index) const {
    const style_sheet& current = style();
    switch (index) {
    case style_var_item_spacing_x: return current.item_spacing_x;
    case style_var_item_spacing_y: return current.item_spacing_y;
    case style_var_frame_padding_x: return current.frame_padding_x;
    case style_var_frame_padding_y: return current.frame_padding_y;
    case style_var_rounding: return current.rounding;
    case style_var_border_size: return current.border_size;
    case style_var_scrollbar_size: return current.scrollbar_size;
    case style_var_font_size: return current.font_size;
    case style_var_line_height: return current.line_height;
    case style_var_popup_padding_x: return current.popup_padding_x;
    case style_var_popup_padding_y: return current.popup_padding_y;
    case style_var_popup_shadow_spread: return current.popup_shadow_spread;
    case style_var_popup_shadow_offset_y: return current.popup_shadow_offset_y;
    case style_var_menu_item_height: return current.menu_item_height;
    case style_var_menu_item_padding_x: return current.menu_item_padding_x;
    case style_var_menu_separator_gap: return current.menu_separator_gap;
    case style_var_menu_separator_height: return current.menu_separator_height;
    case style_var_menu_header_line_height: return current.menu_header_line_height;
    case style_var_combo_item_height: return current.combo_item_height;
    case style_var_combo_popup_padding: return current.combo_popup_padding;
    case style_var_combo_popup_max_height: return current.combo_popup_max_height;
    case style_var_combo_popup_gap: return current.combo_popup_gap;
    case style_var_combo_popup_min_width: return current.combo_popup_min_width;
    case style_var_tooltip_padding_x: return current.tooltip_padding_x;
    case style_var_tooltip_padding_y: return current.tooltip_padding_y;
    case style_var_tooltip_shadow_spread: return current.tooltip_shadow_spread;
    case style_var_tooltip_shadow_offset_y: return current.tooltip_shadow_offset_y;
    case style_var_tab_height: return current.tab_height;
    case style_var_tab_min_width: return current.tab_min_width;
    case style_var_tab_gap: return current.tab_gap;
    case style_var_tab_padding_x: return current.tab_padding_x;
    case style_var_tab_rounding: return current.tab_rounding;
    case style_var_tab_baseline_size: return current.tab_baseline_size;
    case style_var_tab_font_size: return current.tab_font_size;
    case style_var_scrollbar_idle_alpha: return current.scrollbar_idle_alpha;
    default: return 0.0f;
    }
}

void ui_context::set_base_style_color(int index, uint32_t color) {
    switch (index) {
    case style_color_text: base_style_.text = color; break;
    case style_color_text_dim: base_style_.text_dim = color; break;
    case style_color_panel: base_style_.panel = color; break;
    case style_color_panel_hover: base_style_.panel_hover = color; break;
    case style_color_panel_active: base_style_.panel_active = color; break;
    case style_color_border: base_style_.border = color; break;
    case style_color_accent: base_style_.accent = color; break;
    case style_color_accent_hover: base_style_.accent_hover = color; break;
    case style_color_accent_active: base_style_.accent_active = color; break;
    case style_color_white: base_style_.white = color; break;
    case style_color_disabled: base_style_.disabled = color; break;
    case style_color_track: base_style_.track = color; break;
    case style_color_popup_bg: base_style_.popup_bg = color; break;
    case style_color_popup_border: base_style_.popup_border = color; break;
    case style_color_popup_shadow: base_style_.popup_shadow = color; break;
    case style_color_menu_text: base_style_.menu_text = color; break;
    case style_color_menu_shortcut: base_style_.menu_shortcut = color; break;
    case style_color_menu_item_hover: base_style_.menu_item_hover = color; break;
    case style_color_menu_separator: base_style_.menu_separator = color; break;
    case style_color_tooltip_bg: base_style_.tooltip_bg = color; break;
    case style_color_tooltip_border: base_style_.tooltip_border = color; break;
    case style_color_tooltip_shadow: base_style_.tooltip_shadow = color; break;
    case style_color_tab_bar_bg: base_style_.tab_bar_bg = color; break;
    case style_color_tab_bar_border: base_style_.tab_bar_border = color; break;
    case style_color_tab_bg: base_style_.tab_bg = color; break;
    case style_color_tab_bg_hover: base_style_.tab_bg_hover = color; break;
    case style_color_tab_bg_selected: base_style_.tab_bg_selected = color; break;
    case style_color_tab_text: base_style_.tab_text = color; break;
    case style_color_tab_text_selected: base_style_.tab_text_selected = color; break;
    case style_color_tab_baseline: base_style_.tab_baseline = color; break;
    case style_color_tab_focus: base_style_.tab_focus = color; break;
    default: break;
    }
    rebuild_style();
}

void ui_context::set_base_style_var(int index, float value) {
    switch (index) {
    case style_var_item_spacing_x: base_style_.item_spacing_x = value; break;
    case style_var_item_spacing_y: base_style_.item_spacing_y = value; break;
    case style_var_frame_padding_x: base_style_.frame_padding_x = value; break;
    case style_var_frame_padding_y: base_style_.frame_padding_y = value; break;
    case style_var_rounding: base_style_.rounding = value; break;
    case style_var_border_size: base_style_.border_size = value; break;
    case style_var_scrollbar_size: base_style_.scrollbar_size = value; break;
    case style_var_font_size: base_style_.font_size = value; break;
    case style_var_line_height: base_style_.line_height = value; break;
    case style_var_popup_padding_x: base_style_.popup_padding_x = value; break;
    case style_var_popup_padding_y: base_style_.popup_padding_y = value; break;
    case style_var_popup_shadow_spread: base_style_.popup_shadow_spread = value; break;
    case style_var_popup_shadow_offset_y: base_style_.popup_shadow_offset_y = value; break;
    case style_var_menu_item_height: base_style_.menu_item_height = value; break;
    case style_var_menu_item_padding_x: base_style_.menu_item_padding_x = value; break;
    case style_var_menu_separator_gap: base_style_.menu_separator_gap = value; break;
    case style_var_menu_separator_height: base_style_.menu_separator_height = value; break;
    case style_var_menu_header_line_height: base_style_.menu_header_line_height = value; break;
    case style_var_combo_item_height: base_style_.combo_item_height = value; break;
    case style_var_combo_popup_padding: base_style_.combo_popup_padding = value; break;
    case style_var_combo_popup_max_height: base_style_.combo_popup_max_height = value; break;
    case style_var_combo_popup_gap: base_style_.combo_popup_gap = value; break;
    case style_var_combo_popup_min_width: base_style_.combo_popup_min_width = value; break;
    case style_var_tooltip_padding_x: base_style_.tooltip_padding_x = value; break;
    case style_var_tooltip_padding_y: base_style_.tooltip_padding_y = value; break;
    case style_var_tooltip_shadow_spread: base_style_.tooltip_shadow_spread = value; break;
    case style_var_tooltip_shadow_offset_y: base_style_.tooltip_shadow_offset_y = value; break;
    case style_var_tab_height: base_style_.tab_height = value; break;
    case style_var_tab_min_width: base_style_.tab_min_width = value; break;
    case style_var_tab_gap: base_style_.tab_gap = value; break;
    case style_var_tab_padding_x: base_style_.tab_padding_x = value; break;
    case style_var_tab_rounding: base_style_.tab_rounding = value; break;
    case style_var_tab_baseline_size: base_style_.tab_baseline_size = value; break;
    case style_var_tab_font_size: base_style_.tab_font_size = value; break;
    case style_var_scrollbar_idle_alpha: base_style_.scrollbar_idle_alpha = value; break;
    default: break;
    }
    rebuild_style();
}

}
