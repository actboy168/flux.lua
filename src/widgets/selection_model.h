#pragma once

#include <cstdint>
#include <vector>

namespace flux::widgets {

enum class selection_mode {
    single,
    multiple,
    extended,
};

struct selection_model {
    selection_mode mode = selection_mode::single;
    int count = 0;
    int focused = -1;
    int anchor = -1;
    int selected_count = 0;
    std::vector<uint8_t> selected;

    bool resize(int target_count, selection_mode target_mode);
    bool is_selected(int index) const;
    bool set_selected(int index, bool value);
    bool clear();
    bool select_all();
    bool select_only(int index);
    bool select_range(int from, int to);

    bool apply_pointer(int index, bool ctrl, bool shift);
    bool apply_keyboard_move(int target, bool ctrl, bool shift);
    bool apply_right_click(int index);
};

}
