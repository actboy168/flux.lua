#include "ui/ui_context.h"

namespace flux::ui {

void input_state::end_frame() {
    mouse_pressed.fill(false);
    mouse_released.fill(false);
    mouse_double_clicked.fill(false);
    wheel = 0.0f;
    key_pressed.fill(false);
    key_released.fill(false);
    key_ctrl.fill(false);
    key_shift.fill(false);
    key_alt.fill(false);
    text.clear();
    lost_focus = false;
    pending_high_surrogate = 0;
}

void input_state::reset() {
    mouse_down.fill(false);
    mouse_pressed.fill(false);
    mouse_released.fill(false);
    mouse_double_clicked.fill(false);
    wheel = 0.0f;
    key_down.fill(false);
    key_pressed.fill(false);
    key_released.fill(false);
    key_ctrl.fill(false);
    key_shift.fill(false);
    key_alt.fill(false);
    shift = false;
    ctrl = false;
    alt = false;
    mouse_inside = false;
    lost_focus = true;
    text.clear();
    ime_composition.clear();
    ime_cursor = 0;
    ime_composing = false;
    pending_high_surrogate = 0;
}

}
