#include "widgets/selection_model.h"

#include <algorithm>

namespace flux::widgets {

namespace {

bool in_range(int index, int count) {
    return index >= 0 && index < count;
}

void clamp_focus(selection_model& model) {
    if (model.count == 0) {
        model.focused = -1;
        model.anchor = -1;
        return;
    }
    if (model.focused >= model.count) {
        model.focused = model.count - 1;
    }
    if (model.anchor >= model.count) {
        model.anchor = -1;
    }
}

}

bool selection_model::resize(int target_count, selection_mode target_mode) {
    target_count = std::max(0, target_count);
    bool changed = false;

    if (target_count != count) {
        if (target_count < count) {
            const int previous = selected_count;
            selected.resize(static_cast<size_t>(target_count));
            selected_count = 0;
            for (uint8_t value : selected) {
                if (value != 0) {
                    ++selected_count;
                }
            }
            changed = changed || previous != selected_count;
        } else {
            selected.resize(static_cast<size_t>(target_count), 0);
        }
        count = target_count;
        clamp_focus(*this);
    }

    if (mode != target_mode) {
        mode = target_mode;
        if (mode == selection_mode::single && selected_count > 1) {
            int keep = focused >= 0 && is_selected(focused) ? focused : -1;
            if (keep < 0) {
                for (int i = 0; i < count; ++i) {
                    if (selected[static_cast<size_t>(i)] != 0) {
                        keep = i;
                        break;
                    }
                }
            }
            std::fill(selected.begin(), selected.end(), 0);
            selected_count = 0;
            if (keep >= 0) {
                selected[static_cast<size_t>(keep)] = 1;
                selected_count = 1;
                focused = keep;
                anchor = keep;
            }
            changed = true;
        }
    }

    return changed;
}

bool selection_model::is_selected(int index) const {
    return in_range(index, count) && selected[static_cast<size_t>(index)] != 0;
}

bool selection_model::set_selected(int index, bool value) {
    if (!in_range(index, count)) {
        return false;
    }
    uint8_t& slot = selected[static_cast<size_t>(index)];
    const uint8_t next = value ? 1 : 0;
    if (slot == next) {
        return false;
    }
    slot = next;
    selected_count += value ? 1 : -1;
    return true;
}

bool selection_model::clear() {
    if (selected_count == 0) {
        return false;
    }
    std::fill(selected.begin(), selected.end(), 0);
    selected_count = 0;
    anchor = -1;
    return true;
}

bool selection_model::select_all() {
    if (count <= 0 || selected_count == count) {
        return false;
    }
    std::fill(selected.begin(), selected.end(), 1);
    selected_count = count;
    return true;
}

bool selection_model::select_only(int index) {
    if (!in_range(index, count)) {
        return clear();
    }
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        const uint8_t next = i == index ? 1 : 0;
        uint8_t& slot = selected[static_cast<size_t>(i)];
        if (slot != next) {
            slot = next;
            changed = true;
        }
    }
    selected_count = 1;
    focused = index;
    anchor = index;
    return changed;
}

bool selection_model::select_range(int from, int to) {
    if (count <= 0) {
        return false;
    }
    from = std::clamp(from, 0, count - 1);
    to = std::clamp(to, 0, count - 1);
    if (from > to) {
        std::swap(from, to);
    }
    bool changed = false;
    for (int i = from; i <= to; ++i) {
        uint8_t& slot = selected[static_cast<size_t>(i)];
        if (slot == 0) {
            slot = 1;
            ++selected_count;
            changed = true;
        }
    }
    return changed;
}

bool selection_model::apply_pointer(int index, bool ctrl, bool shift) {
    if (!in_range(index, count)) {
        return false;
    }

    if (shift) {
        if (!in_range(anchor, count)) {
            anchor = focused >= 0 ? focused : index;
        }
        bool changed = false;
        if (mode == selection_mode::single) {
            changed = select_only(index);
        } else if (mode == selection_mode::multiple) {
            changed = select_range(anchor, index);
            focused = index;
        } else if (ctrl) {
            changed = select_range(anchor, index);
            focused = index;
        } else {
            const int from = anchor;
            changed = clear();
            changed = select_range(from, index) || changed;
            focused = index;
            anchor = from;
        }
        return changed;
    }

    if (ctrl && mode != selection_mode::single) {
        const bool changed = set_selected(index, !is_selected(index));
        focused = index;
        anchor = index;
        return changed;
    }

    if (mode == selection_mode::multiple) {
        const bool changed = set_selected(index, !is_selected(index));
        focused = index;
        anchor = index;
        return changed;
    }

    return select_only(index);
}

bool selection_model::apply_keyboard_move(int target, bool ctrl, bool shift) {
    if (!in_range(target, count)) {
        return false;
    }

    if (shift) {
        if (!in_range(anchor, count)) {
            anchor = focused >= 0 ? focused : target;
        }
        bool changed = false;
        if (mode == selection_mode::single) {
            changed = select_only(target);
        } else if (mode == selection_mode::multiple) {
            changed = select_range(anchor, target);
            focused = target;
        } else if (ctrl) {
            changed = select_range(anchor, target);
            focused = target;
        } else {
            const int from = anchor;
            changed = clear();
            changed = select_range(from, target) || changed;
            focused = target;
            anchor = from;
        }
        return changed;
    }

    if (ctrl && mode != selection_mode::single) {
        focused = target;
        return false;
    }

    return select_only(target);
}

bool selection_model::apply_right_click(int index) {
    if (!in_range(index, count)) {
        return false;
    }
    if (is_selected(index)) {
        focused = index;
        return false;
    }
    if (mode == selection_mode::multiple) {
        const bool changed = set_selected(index, true);
        focused = index;
        anchor = index;
        return changed;
    }
    return select_only(index);
}

}
