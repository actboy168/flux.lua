#include "widgets/widgets.h"
#include "widgets/text_input_internal.h"

#include <cstdlib>
#include <string>

#include "text/charconv.h"
#include "runtime/services.h"
#include "widgets/widget_runtime.h"
#include "widgets/widget_paint.h"

namespace flux::widgets {

bool parse_int(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str()) {
        return false;
    }
    while (end != nullptr && *end == ' ') {
        ++end;
    }
    if (end != nullptr && *end != '\0') {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}
bool parse_float(const std::string& text, double& value) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (end == text.c_str()) {
        return false;
    }
    while (end != nullptr && *end == ' ') {
        ++end;
    }
    if (end != nullptr && *end != '\0') {
        return false;
    }
    value = parsed;
    return true;
}
int_input_result input_int(const char* label, int value, float width, float height,
                         bool enabled, int step, flux::ui::ui_id id) {
    const flux::ui::ui_id resolved_id = id != 0 ? id : widget_ui().make_id(label);
    edit_state& state = widget_runtime().states.get<edit_state>(resolved_id, widget_ui().frame_index());
    if (!state.number_init
        || (!state.focused_last && static_cast<int>(state.number_committed) != value)) {
        state.number_text = flux::text::int_to_string(value);
        state.number_committed = value;
        state.number_init = true;
    }
    if (step <= 0) {
        step = 1;
    }
    std::string text = state.number_text;
    text_input_result edited = text_input_core(label, text, width, height, false, enabled, state, false,
                                        resolved_id);
    state.number_text = text;

    int current = value;
    bool parsed = parse_int(text, current);
    if (edited.focused && enabled && !widget_ui().input.ime_composing) {
        if (widget_ui().input.key_pressed[flux::ui::keys::up]) {
            text_input_push_undo(state, text);
            current = static_cast<int>(state.number_committed) + step;
            parsed = true;
            state.number_text = flux::text::int_to_string(current);
            text = state.number_text;
            edited.changed = true;
            edited.submitted = true;
        } else if (widget_ui().input.key_pressed[flux::ui::keys::down]) {
            text_input_push_undo(state, text);
            current = static_cast<int>(state.number_committed) - step;
            parsed = true;
            state.number_text = flux::text::int_to_string(current);
            text = state.number_text;
            edited.changed = true;
            edited.submitted = true;
        }
    }
    if (parsed) {
        state.number_committed = current;
    }
    if (edited.submitted || edited.deactivated) {
        if (!parsed) {
            current = static_cast<int>(state.number_committed);
            state.number_text = flux::text::int_to_string(current);
        } else {
            state.number_text = flux::text::int_to_string(current);
        }
    }

    int_input_result result;
    result.changed = parsed && current != value;
    result.submitted = edited.submitted;
    result.deactivated = edited.deactivated;
    result.focused = edited.focused;
    result.value = current;
    return result;
}
float_input_result input_float(const char* label, double value, float width, float height,
                             bool enabled, double step, flux::ui::ui_id id) {
    const flux::ui::ui_id resolved_id = id != 0 ? id : widget_ui().make_id(label);
    edit_state& state = widget_runtime().states.get<edit_state>(resolved_id, widget_ui().frame_index());
    if (!state.number_init
        || (!state.focused_last && state.number_committed != value)) {
        state.number_text = flux::text::float_to_string(value);
        state.number_committed = value;
        state.number_init = true;
    }
    if (step <= 0.0) {
        step = 1.0;
    }
    std::string text = state.number_text;
    text_input_result edited = text_input_core(label, text, width, height, false, enabled, state, false,
                                        resolved_id);
    state.number_text = text;

    double current = value;
    bool parsed = parse_float(text, current);
    if (edited.focused && enabled && !widget_ui().input.ime_composing) {
        if (widget_ui().input.key_pressed[flux::ui::keys::up]) {
            text_input_push_undo(state, text);
            current = state.number_committed + step;
            parsed = true;
            state.number_text = flux::text::float_to_string(current);
            text = state.number_text;
            edited.changed = true;
            edited.submitted = true;
        } else if (widget_ui().input.key_pressed[flux::ui::keys::down]) {
            text_input_push_undo(state, text);
            current = state.number_committed - step;
            parsed = true;
            state.number_text = flux::text::float_to_string(current);
            text = state.number_text;
            edited.changed = true;
            edited.submitted = true;
        }
    }
    if (parsed) {
        state.number_committed = current;
    }
    if (edited.submitted || edited.deactivated) {
        if (!parsed) {
            current = state.number_committed;
        }
        state.number_text = flux::text::float_to_string(current);
    }

    float_input_result result;
    result.changed = parsed && current != value;
    result.submitted = edited.submitted;
    result.deactivated = edited.deactivated;
    result.focused = edited.focused;
    result.value = current;
    return result;
}
}
