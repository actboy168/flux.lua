#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace flux::ui {

namespace keys {

// Platform-neutral key codes; values match Windows virtual-key codes.
constexpr int backspace = 0x08;
constexpr int tab = 0x09;
constexpr int enter = 0x0D;
constexpr int shift = 0x10;
constexpr int control = 0x11;
constexpr int alt = 0x12;
constexpr int escape = 0x1B;
constexpr int space = 0x20;
constexpr int page_up = 0x21;
constexpr int page_down = 0x22;
constexpr int end = 0x23;
constexpr int home = 0x24;
constexpr int left = 0x25;
constexpr int up = 0x26;
constexpr int right = 0x27;
constexpr int down = 0x28;
constexpr int del = 0x2E;
constexpr int a = 0x41;

}

struct input_state {
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    std::array<bool, 3> mouse_down{};
    std::array<bool, 3> mouse_pressed{};
    std::array<bool, 3> mouse_released{};
    std::array<bool, 3> mouse_double_clicked{};
    float wheel = 0.0f;
    std::array<bool, 256> key_down{};
    std::array<bool, 256> key_pressed{};
    std::array<bool, 256> key_released{};
    std::array<bool, 256> key_ctrl{};
    std::array<bool, 256> key_shift{};
    std::array<bool, 256> key_alt{};
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool window_active = true;
    bool mouse_inside = false;
    bool lost_focus = false;
    std::string text;
    std::string ime_composition;
    int ime_cursor = 0;
    bool ime_composing = false;
    uint32_t pending_high_surrogate = 0;
    void end_frame();
    void reset();
};

}
