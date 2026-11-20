#include "widgets/widget_runtime.h"

#include "runtime/services.h"

namespace flux::widgets {

widget_runtime_state& widget_runtime() {
    static widget_runtime_state state;
    const uint64_t frame = flux::runtime::services().ui.frame_index();
    // Prune once per frame on first widget access so controls carry no lifecycle code.
    if (state.prune_frame != frame) {
        state.prune_frame = frame;
        state.states.prune(frame);
        for (auto it = state.anims.begin(); it != state.anims.end();) {
            if (frame > it->second.last_frame + widget_state_ttl_frames) {
                it = state.anims.erase(it);
            } else {
                ++it;
            }
        }
        if (state.combo_open_id != 0
            && state.states.find<combo_state>(state.combo_open_id) == nullptr) {
            state.combo_open_id = 0;
        }
    }
    return state;
}

void widget_runtime_reset() {
    widget_runtime().reset();
}

bool widget_runtime_state::animations_active(uint64_t frame) const {
    if (tooltip.pending) {
        return true;
    }
    constexpr float epsilon = 0.001f;
    auto mid = [](float value) {
        return value > epsilon && value < 1.0f - epsilon;
    };
    for (const auto& entry : anims) {
        if (mid(entry.second.anim.hover) || mid(entry.second.anim.press)
            || mid(entry.second.anim.focus) || mid(entry.second.anim.value)) {
            return true;
        }
    }
    return animation_frame > frame;
}

void widget_runtime_state::reset() {
    states.clear();
    combo_open_id = 0;
    anims.clear();
    anim_enabled_frame = UINT64_MAX;
    anim_enabled = true;
    prune_frame = UINT64_MAX;
    animation_frame = 0;
    tooltip_reveal = 0.0f;
    tooltip_frame = 0;
    tooltip_hover_start = -100.0;
    tooltip_text.clear();
    tooltip = tooltip_request{};
    popup_scopes.clear();
    table_scopes.clear();
    list_scopes.clear();
    menu_scopes.clear();
    frame_ok = true;
    frame_error.clear();
}

void widget_runtime_state::fail(std::string_view message) {
    if (frame_ok) {
        frame_ok = false;
        frame_error = message;
    }
}

void widget_frame_begin() {
    widget_runtime_state& state = widget_runtime();
    state.frame_ok = true;
    state.frame_error.clear();
    if (!state.popup_scopes.empty()) {
        state.fail("popup scope was not ended before frame begin");
        state.popup_scopes.clear();
    }
    if (!state.table_scopes.empty()) {
        state.fail("table scope was not ended before frame begin");
        state.table_scopes.clear();
    }
    if (!state.list_scopes.empty()) {
        state.fail("list scope was not ended before frame begin");
        state.list_scopes.clear();
    }
    if (!state.menu_scopes.empty()) {
        state.fail("menu scope was not ended before frame begin");
        state.menu_scopes.clear();
    }
}

bool widget_frame_end(std::string& error) {
    widget_runtime_state& state = widget_runtime();
    if (!state.popup_scopes.empty()) {
        state.fail("popup scope is not balanced");
        state.popup_scopes.clear();
    }
    if (!state.table_scopes.empty()) {
        state.fail("table scope is not balanced");
        state.table_scopes.clear();
    }
    if (!state.list_scopes.empty()) {
        state.fail("list scope is not balanced");
        state.list_scopes.clear();
    }
    if (!state.menu_scopes.empty()) {
        state.fail("menu scope is not balanced");
        state.menu_scopes.clear();
    }
    if (!state.frame_ok) {
        error = state.frame_error;
        return false;
    }
    return true;
}

}
