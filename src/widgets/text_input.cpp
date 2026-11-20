#include "widgets/widgets.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

#include "runtime/services.h"
#include "text/charconv.h"
#include "text/utf8.h"
#include "widgets/control_anim.h"
#include "widgets/widget_runtime.h"
#include "widgets/widget_paint.h"

namespace flux::widgets {

namespace {

bool is_combining(uint32_t codepoint) {
    return (codepoint >= 0x0300 && codepoint <= 0x036F)
        || (codepoint >= 0x1AB0 && codepoint <= 0x1AFF)
        || (codepoint >= 0x1DC0 && codepoint <= 0x1DFF)
        || (codepoint >= 0x20D0 && codepoint <= 0x20FF)
        || (codepoint >= 0xFE20 && codepoint <= 0xFE2F);
}

bool is_word_codepoint(uint32_t codepoint) {
    if (codepoint >= 0x80) {
        return true;
    }
    return (codepoint >= '0' && codepoint <= '9')
        || (codepoint >= 'A' && codepoint <= 'Z')
        || (codepoint >= 'a' && codepoint <= 'z')
        || codepoint == '_';
}

bool is_space_codepoint(uint32_t codepoint) {
    return codepoint == ' ' || codepoint == '\t' || codepoint == '\r'
        || codepoint == '\n' || codepoint == 0x3000;
}

size_t next_grapheme(std::string_view text, size_t offset) {
    size_t next = flux::text::utf8_next_offset(text, offset);
    while (next < text.size()) {
        size_t length = 0;
        if (!is_combining(flux::text::utf8_codepoint_at(text, next, &length))) {
            break;
        }
        next = flux::text::utf8_next_offset(text, next);
    }
    return next;
}

size_t prev_grapheme(std::string_view text, size_t offset) {
    if (offset == 0) {
        return 0;
    }
    size_t previous = flux::text::utf8_prev_offset(text, offset);
    while (previous > 0) {
        size_t length = 0;
        if (!is_combining(flux::text::utf8_codepoint_at(text, previous, &length))) {
            break;
        }
        previous = flux::text::utf8_prev_offset(text, previous);
    }
    return previous;
}

int clamp_offset(std::string_view text, int offset) {
    if (offset <= 0) {
        return 0;
    }
    if (offset >= static_cast<int>(text.size())) {
        return static_cast<int>(text.size());
    }
    size_t index = static_cast<size_t>(offset);
    while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80) {
        --index;
    }
    return static_cast<int>(index);
}

int word_left(std::string_view text, int offset) {
    size_t index = static_cast<size_t>(std::max(offset, 0));
    while (index > 0) {
        const size_t previous = prev_grapheme(text, index);
        size_t length = 0;
        if (is_word_codepoint(flux::text::utf8_codepoint_at(text, previous, &length))) {
            break;
        }
        index = previous;
    }
    while (index > 0) {
        const size_t previous = prev_grapheme(text, index);
        size_t length = 0;
        if (!is_word_codepoint(flux::text::utf8_codepoint_at(text, previous, &length))) {
            break;
        }
        index = previous;
    }
    return static_cast<int>(index);
}

int word_right(std::string_view text, int offset) {
    size_t index = static_cast<size_t>(std::max(offset, 0));
    while (index < text.size()) {
        size_t length = 0;
        if (is_word_codepoint(flux::text::utf8_codepoint_at(text, index, &length))) {
            break;
        }
        index = next_grapheme(text, index);
    }
    while (index < text.size()) {
        size_t length = 0;
        if (!is_word_codepoint(flux::text::utf8_codepoint_at(text, index, &length))) {
            break;
        }
        index = next_grapheme(text, index);
    }
    return static_cast<int>(index);
}

std::string mask_text(std::string_view text) {
    std::string masked;
    masked.reserve(text.size());
    size_t index = 0;
    while (index < text.size()) {
        masked += "\xE2\x97\x8F";
        index = next_grapheme(text, index);
    }
    return masked;
}

size_t display_offset(std::string_view text, size_t offset, bool password) {
    if (!password) {
        return std::min(offset, text.size());
    }
    size_t index = 0;
    size_t count = 0;
    while (index < text.size() && index < offset) {
        index = next_grapheme(text, index);
        ++count;
    }
    return count * 3;
}

size_t display_to_real(std::string_view text, size_t display, bool password) {
    if (!password) {
        return std::min(display, text.size());
    }
    const size_t count = display / 3;
    size_t index = 0;
    for (size_t i = 0; i < count && index < text.size(); ++i) {
        index = next_grapheme(text, index);
    }
    return index;
}

std::pair<int, int> selection(const edit_state& state) {
    return state.anchor <= state.caret
        ? std::make_pair(state.anchor, state.caret)
        : std::make_pair(state.caret, state.anchor);
}

void push_undo(edit_state& state, const std::string& text) {
    if (!state.undo.empty()
        && state.undo.back().text == text
        && state.undo.back().caret == state.caret
        && state.undo.back().anchor == state.anchor) {
        return;
    }
    edit_snapshot snapshot;
    snapshot.text = text;
    snapshot.caret = state.caret;
    snapshot.anchor = state.anchor;
    state.undo.push_back(std::move(snapshot));
    if (state.undo.size() > 50) {
        state.undo.erase(state.undo.begin());
    }
    state.redo.clear();
}

void begin_edit(edit_state& state, const std::string& text, bool typing, bool whitespace) {
    const double now = widget_ui().time();
    const bool boundary = !state.coalescing || typing == false
        || whitespace || now - state.last_edit_time > 1.0;
    if (boundary) {
        push_undo(state, text);
    }
    state.last_edit_time = now;
    state.coalescing = typing;
    state.session_edited = true;
}

std::string filter_insert(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    size_t index = 0;
    while (index < text.size()) {
        size_t length = 0;
        const uint32_t codepoint = flux::text::utf8_codepoint_at(text, index, &length);
        if (codepoint >= 0x20 && codepoint != 0x7F) {
            result.append(text.data() + index, length);
        }
        index += length;
    }
    return result;
}

struct input_paint {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float inner_x = 0.0f;
    float inner_y = 0.0f;
    float inner_w = 0.0f;
    float inner_h = 0.0f;
    float text_size = 14.0f;
};

text_input_result edit_core(const char* label, std::string& text,
                                       float width, float height,
                                       bool password, bool enabled,
                                       edit_state& state, bool sync_external, flux::ui::ui_id id) {
    text_input_result result;
    flux::ui::ui_context& context = widget_ui();
    const flux::ui::style_sheet& style = context.style();
    const flux::ui::ui_id resolved_id = id != 0 ? id : context.make_id(label);

    if (width <= 0.0f) {
        width = 220.0f;
    }
    if (height <= 0.0f) {
        height = std::max(28.0f, style.font_size + style.frame_padding_y * 2.0f + 8.0f);
    }

    if (sync_external && state.known_set && state.known != text) {
        state.undo.clear();
        state.redo.clear();
        state.caret = clamp_offset(text, state.caret);
        state.anchor = clamp_offset(text, state.anchor);
        state.coalescing = false;
    }
    state.caret = clamp_offset(text, state.caret);
    state.anchor = clamp_offset(text, state.anchor);

    bool hovered = false;
    bool active = false;
    bool clicked = false;
    bool double_clicked = false;
    bool focused = false;
    const bool focusable = enabled;
    context.widget(label, width, height, focusable, hovered, active, clicked, double_clicked, focused);
    result.focused = focused;
    if (focused) {
        context.set_edit_focused(true);
    }

    if (!context.is_item_visible()) {
        state.known = text;
        state.known_set = true;
        return result;
    }

    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    context.last_item_rect(x, y, w, h);
    const float pad_x = style.frame_padding_x;
    const float pad_y = style.frame_padding_y;
    input_paint paint;
    paint.x = x;
    paint.y = y;
    paint.w = w;
    paint.h = h;
    paint.inner_x = x + pad_x;
    paint.inner_y = y + pad_y;
    paint.inner_w = std::max(1.0f, w - pad_x * 2.0f);
    paint.inner_h = std::max(1.0f, h - pad_y * 2.0f);
    paint.text_size = style.font_size;

    const bool gained_focus = focused && !state.focused_last;
    const bool lost_focus = !focused && state.focused_last;
    if (gained_focus) {
        state.focus_text = text;
        state.session_edited = false;
        state.coalescing = false;
        state.caret_blink_start = widget_ui().time();
        state.caret_blink_caret = state.caret;
    }

    if (focused && enabled && !widget_ui().input.ime_composing) {
        const flux::ui::input_state& input = widget_ui().input;

        auto move_caret = [&](int position, bool extend) {
            const int old = state.caret;
            state.caret = clamp_offset(text, position);
            if (!extend) {
                state.anchor = state.caret;
            } else if (state.anchor == old) {
                state.anchor = old;
            }
        };
        auto erase_range = [&](int begin, int end) {
            if (begin >= end) {
                return;
            }
            begin_edit(state, text, false, true);
            text.erase(static_cast<size_t>(begin), static_cast<size_t>(end - begin));
            state.caret = begin;
            state.anchor = begin;
            result.changed = true;
        };

        const auto range = selection(state);
        if (input.key_pressed[flux::ui::keys::backspace]) {
            if (range.first != range.second) {
                erase_range(range.first, range.second);
            } else {
                const int begin = input.key_ctrl[flux::ui::keys::backspace]
                    ? word_left(text, state.caret)
                    : static_cast<int>(prev_grapheme(text, static_cast<size_t>(state.caret)));
                erase_range(begin, state.caret);
            }
        }
        if (input.key_pressed[flux::ui::keys::del]) {
            if (range.first != range.second) {
                erase_range(range.first, range.second);
            } else {
                const int end = input.key_ctrl[flux::ui::keys::del]
                    ? word_right(text, state.caret)
                    : static_cast<int>(next_grapheme(text, static_cast<size_t>(state.caret)));
                erase_range(state.caret, end);
            }
        }
        if (input.key_pressed[flux::ui::keys::left]) {
            move_caret(input.key_ctrl[flux::ui::keys::left]
                    ? word_left(text, state.caret)
                    : static_cast<int>(prev_grapheme(text, static_cast<size_t>(state.caret))),
                input.key_shift[flux::ui::keys::left]);
        }
        if (input.key_pressed[flux::ui::keys::right]) {
            move_caret(input.key_ctrl[flux::ui::keys::right]
                    ? word_right(text, state.caret)
                    : static_cast<int>(next_grapheme(text, static_cast<size_t>(state.caret))),
                input.key_shift[flux::ui::keys::right]);
        }
        if (input.key_pressed[flux::ui::keys::home]) {
            move_caret(0, input.key_shift[flux::ui::keys::home]);
        }
        if (input.key_pressed[flux::ui::keys::end]) {
            move_caret(static_cast<int>(text.size()), input.key_shift[flux::ui::keys::end]);
        }
        if (input.key_ctrl['A'] && input.key_pressed['A']) {
            state.anchor = 0;
            state.caret = static_cast<int>(text.size());
        }
        if (input.key_ctrl['C'] && input.key_pressed['C'] && range.first != range.second) {
            flux::runtime::services().set_clipboard_text(
                text.substr(static_cast<size_t>(range.first),
                            static_cast<size_t>(range.second - range.first)));
        }
        if (input.key_ctrl['X'] && input.key_pressed['X'] && range.first != range.second) {
            flux::runtime::services().set_clipboard_text(
                text.substr(static_cast<size_t>(range.first),
                            static_cast<size_t>(range.second - range.first)));
            erase_range(range.first, range.second);
        }
        if (input.key_ctrl['V'] && input.key_pressed['V']) {
            std::string pasted = flux::runtime::services().clipboard_text();
            pasted.erase(std::remove(pasted.begin(), pasted.end(), '\r'), pasted.end());
            pasted.erase(std::remove(pasted.begin(), pasted.end(), '\n'), pasted.end());
            if (!pasted.empty()) {
                begin_edit(state, text, false, true);
                const auto paste_range = selection(state);
                if (paste_range.first != paste_range.second) {
                    text.erase(static_cast<size_t>(paste_range.first),
                               static_cast<size_t>(paste_range.second - paste_range.first));
                }
                text.insert(static_cast<size_t>(paste_range.first), pasted);
                state.caret = paste_range.first + static_cast<int>(pasted.size());
                state.anchor = state.caret;
                result.changed = true;
            }
        }
        if (input.key_ctrl['Z'] && input.key_pressed['Z']) {
            if (!state.undo.empty()) {
                edit_snapshot current;
                current.text = text;
                current.caret = state.caret;
                current.anchor = state.anchor;
                state.redo.push_back(std::move(current));
                edit_snapshot snapshot = state.undo.back();
                state.undo.pop_back();
                text = snapshot.text;
                state.caret = clamp_offset(text, snapshot.caret);
                state.anchor = clamp_offset(text, snapshot.anchor);
                state.coalescing = false;
                result.changed = true;
            }
        }
        if (input.key_ctrl['Y'] && input.key_pressed['Y']) {
            if (!state.redo.empty()) {
                edit_snapshot current;
                current.text = text;
                current.caret = state.caret;
                current.anchor = state.anchor;
                state.undo.push_back(std::move(current));
                edit_snapshot snapshot = state.redo.back();
                state.redo.pop_back();
                text = snapshot.text;
                state.caret = clamp_offset(text, snapshot.caret);
                state.anchor = clamp_offset(text, snapshot.anchor);
                state.coalescing = false;
                result.changed = true;
            }
        }
        if (input.key_pressed[flux::ui::keys::enter]) {
            result.submitted = true;
            state.coalescing = false;
        }
        if (input.key_pressed[flux::ui::keys::escape]) {
            if (text != state.focus_text) {
                text = state.focus_text;
                state.caret = static_cast<int>(text.size());
                state.anchor = state.caret;
                state.undo.clear();
                state.redo.clear();
                result.changed = true;
            }
        }
    }

    if (focused && enabled && !widget_ui().input.ime_composing && !widget_ui().input.text.empty()) {
        const std::string inserted = filter_insert(widget_ui().input.text);
        if (!inserted.empty()) {
            begin_edit(state, text, true, is_space_codepoint(
                flux::text::utf8_codepoint_at(inserted, 0, nullptr)));
            const auto range = selection(state);
            if (range.first != range.second) {
                text.erase(static_cast<size_t>(range.first),
                           static_cast<size_t>(range.second - range.first));
            }
            text.insert(static_cast<size_t>(range.first), inserted);
            state.caret = range.first + static_cast<int>(inserted.size());
            state.anchor = state.caret;
            result.changed = true;
        }
    }

    if (enabled && hovered && widget_ui().input.mouse_pressed[0] && !widget_ui().input.ime_composing) {
        state.dragging = true;
    }
    if (state.dragging && !widget_ui().input.mouse_down[0]) {
        state.dragging = false;
    }

    if (lost_focus) {
        if (state.session_edited) {
            result.deactivated = true;
        }
        state.dragging = false;
        state.coalescing = false;
        flux::runtime::services().cancel_ime();
    }
    if (focused && (result.changed || widget_ui().input.mouse_pressed[0]
                    || state.caret != state.caret_blink_caret)) {
        state.caret_blink_start = widget_ui().time();
        state.caret_blink_caret = state.caret;
    }
    state.focused_last = focused;
    state.known = text;
    state.known_set = true;
    result.value = text;

    // Display string: mask passwords and splice IME composition at the caret.
    std::string visible = password ? mask_text(text) : text;
    size_t caret_display = display_offset(text, static_cast<size_t>(state.caret), password);
    size_t compose_start = caret_display;
    size_t compose_end = caret_display;
    if (widget_ui().input.ime_composing && !widget_ui().input.ime_composition.empty()) {
        const auto range = selection(state);
        const std::string prefix = text.substr(0, static_cast<size_t>(range.first));
        const std::string suffix = text.substr(static_cast<size_t>(range.second));
        const std::string composition = password
            ? mask_text(widget_ui().input.ime_composition)
            : widget_ui().input.ime_composition;
        visible = (password ? mask_text(prefix) : prefix)
            + composition
            + (password ? mask_text(suffix) : suffix);
        compose_start = display_offset(text, static_cast<size_t>(range.first), password);
        compose_end = compose_start + composition.size();
        caret_display = compose_start + display_offset(
            widget_ui().input.ime_composition,
            static_cast<size_t>(std::max(widget_ui().input.ime_cursor, 0)),
            password);
    }

    flux::text::text_options options;
    options.weight = 400;
    float text_width = 0.0f;
    float text_height = 0.0f;
    flux::runtime::services().measure_text(visible.c_str(), paint.text_size, options, text_width, text_height);
    if (text_height <= 0.0f) {
        text_height = paint.text_size * 1.2f;
    }
    const float text_y = y + (h - text_height) * 0.5f;

    if (enabled && !widget_ui().input.ime_composing
        && (hovered && widget_ui().input.mouse_pressed[0]
            || (state.dragging && widget_ui().input.mouse_down[0]))) {
        const float local_x = widget_ui().input.mouse_x - (paint.inner_x - state.scroll);
        const float local_y = widget_ui().input.mouse_y - text_y;
        const int display_hit = flux::runtime::services().hit_test_text(
            visible.c_str(), paint.text_size, options, local_x, local_y);
        const int real_hit = static_cast<int>(
            display_to_real(text, static_cast<size_t>(std::max(display_hit, 0)), password));
        if (widget_ui().input.mouse_pressed[0] && double_clicked) {
            size_t begin = static_cast<size_t>(std::min(real_hit, static_cast<int>(text.size())));
            size_t end = begin;
            while (begin > 0) {
                const size_t previous = prev_grapheme(text, begin);
                size_t length = 0;
                if (!is_word_codepoint(flux::text::utf8_codepoint_at(text, previous, &length))) {
                    break;
                }
                begin = previous;
            }
            while (end < text.size()) {
                size_t length = 0;
                if (!is_word_codepoint(flux::text::utf8_codepoint_at(text, end, &length))) {
                    break;
                }
                end = next_grapheme(text, end);
            }
            if (begin == end) {
                begin = static_cast<size_t>(
                    clamp_offset(text, static_cast<int>(prev_grapheme(text, begin))));
                end = flux::text::utf8_next_offset(text, begin);
            }
            state.anchor = static_cast<int>(begin);
            state.caret = static_cast<int>(end);
        } else {
            state.caret = clamp_offset(text, real_hit);
            if (widget_ui().input.mouse_pressed[0] && !widget_ui().input.shift) {
                state.anchor = state.caret;
            }
        }
    }

    float caret_x = 0.0f;
    float caret_y = 0.0f;
    float caret_height = text_height;
    flux::runtime::services().caret_metrics_text(
        visible.c_str(), paint.text_size, options,
        static_cast<int>(caret_display), caret_x, caret_y, caret_height);

    const float max_scroll = std::max(0.0f, text_width - paint.inner_w + 2.0f);
    if (caret_x - state.scroll > paint.inner_w - 2.0f) {
        state.scroll = caret_x - paint.inner_w + 2.0f;
    }
    if (caret_x - state.scroll < 0.0f) {
        state.scroll = caret_x;
    }
    state.scroll = std::clamp(state.scroll, 0.0f, max_scroll);

    control_anim_targets anim_targets;
    anim_targets.hovered = hovered;
    anim_targets.active = active;
    anim_targets.focused = focused;
    const control_anim_state& anim =
        control_anim(resolved_id, anim_targets, context.frame_index());
    uint32_t fill = style.panel;
    if (enabled) {
        fill = mix_color(fill, style.panel_hover, anim.hover);
        fill = mix_color(fill, style.panel_active, std::max(anim.press, anim.focus));
    }
    flux::ui::draw_list& draw = flux::runtime::services().draw;
    draw.rect_filled(x, y, w, h, fill, style.rounding);
    draw.rect(x, y, w, h, mix_color(style.border, style.accent, anim.focus),
              style.rounding,
              style.border_size + (2.0f - style.border_size) * anim.focus);
    if (context.focus_visible() && anim.focus > 0.002f) {
        draw.rect(x - 2.0f, y - 2.0f, w + 4.0f, h + 4.0f,
                  with_alpha(style.accent, anim.focus), style.rounding + 2.0f, 1.0f);
    }

    draw.push_clip(paint.inner_x, paint.inner_y, paint.inner_w, paint.inner_h);
    const float text_x = paint.inner_x - state.scroll;

    if (focused && !widget_ui().input.ime_composing) {
        const auto range = selection(state);
        if (range.first != range.second) {
            float sel_x1 = 0.0f;
            float sel_x2 = 0.0f;
            float sel_y = 0.0f;
            float sel_h = text_height;
            flux::runtime::services().caret_metrics_text(
                visible.c_str(), paint.text_size, options,
                static_cast<int>(display_offset(text, static_cast<size_t>(range.first), password)),
                sel_x1, sel_y, sel_h);
            flux::runtime::services().caret_metrics_text(
                visible.c_str(), paint.text_size, options,
                static_cast<int>(display_offset(text, static_cast<size_t>(range.second), password)),
                sel_x2, sel_y, sel_h);
            if (sel_x2 < sel_x1) {
                std::swap(sel_x1, sel_x2);
            }
            draw.rect_filled(text_x + sel_x1, text_y, sel_x2 - sel_x1, text_height,
                             (style.accent & 0x00FFFFFF) | 0x66000000, 2.0f);
        }
    }

    if (!visible.empty()) {
        draw.text_options(text_x, text_y, visible, enabled ? style.text : style.disabled,
                          paint.text_size, 0.0f, 0.0f, 0.0f, 0, 0, 0, 400, false, false);
    }

    if (widget_ui().input.ime_composing && compose_end > compose_start) {
        float underline_start = 0.0f;
        float underline_end = 0.0f;
        float underline_y = text_y;
        float underline_h = text_height;
        flux::runtime::services().caret_metrics_text(
            visible.c_str(), paint.text_size, options,
            static_cast<int>(compose_start), underline_start, underline_y, underline_h);
        flux::runtime::services().caret_metrics_text(
            visible.c_str(), paint.text_size, options,
            static_cast<int>(compose_end), underline_end, underline_y, underline_h);
        draw.rect_filled(text_x + underline_start, text_y + text_height - 1.0f,
                         std::max(1.0f, underline_end - underline_start), 1.0f,
                         style.accent, 0.0f);
    }

    if (focused && enabled) {
        bool caret_visible = true;
        if (control_anim_animations_enabled(context.frame_index())
            && !widget_ui().input.ime_composing) {
            const double elapsed = widget_ui().time() - state.caret_blink_start;
            caret_visible = std::fmod(std::max(elapsed, 0.0), 1.06) < 0.53;
        }
        if (caret_visible) {
            draw.rect_filled(text_x + caret_x, text_y, 1.0f, text_height,
                             style.accent, 0.0f);
        }
        flux::runtime::services().update_ime_window(paint.inner_x + caret_x - state.scroll,
                                    text_y, text_height, paint.text_size);
    }

    draw.pop_clip();

    return result;
}

}

void text_input_push_undo(edit_state& state, const std::string& text) {
    push_undo(state, text);
}

text_input_result text_input_core(const char* label, std::string& text,
                                  float width, float height, bool password,
                                  bool enabled, edit_state& state,
                                  bool sync_external, flux::ui::ui_id id) {
    return edit_core(label, text, width, height, password, enabled, state,
                     sync_external, id);
}

text_input_result input_text(const char* label, const char* value, float width, float height,
                           bool password, bool enabled, flux::ui::ui_id id) {
    const flux::ui::ui_id resolved_id = id != 0 ? id : widget_ui().make_id(label);
    edit_state& state = widget_runtime().states.get<edit_state>(resolved_id, widget_ui().frame_index());
    std::string text = value != nullptr ? value : "";
    text_input_result result = edit_core(label, text, width, height, password, enabled, state, true,
                                        resolved_id);
    result.value = text;
    return result;
}

}
