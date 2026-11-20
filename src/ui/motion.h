#pragma once

namespace flux::ui::motion {

// Fluent-style motion durations, in seconds.
inline constexpr float fast = 0.083f;
inline constexpr float normal = 0.167f;
inline constexpr float slow = 0.250f;

// Exponential smoothing time constants for interactive state.
inline constexpr float hover_tau = 0.080f;
inline constexpr float press_tau = 0.050f;
inline constexpr float focus_tau = 0.110f;
inline constexpr float value_tau = 0.120f;

}
