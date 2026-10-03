// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace bvb {
enum class Button : std::size_t {
    left_up, left_down, left_left, left_right,
    right_up, right_down, right_left, right_right,
    a, b, l, r, start, select, count
};
struct GameInput {
    std::array<bool, static_cast<std::size_t>(Button::count)> buttons{};
    bool& operator[](Button button) { return buttons[static_cast<std::size_t>(button)]; }
    bool operator[](Button button) const { return buttons[static_cast<std::size_t>(button)]; }
};
struct GamepadState {
    std::int16_t left_x = 0, left_y = 0, right_x = 0, right_y = 0;
    bool up = false, down = false, left = false, right = false;
    bool a = false, b = false, l = false, r = false, start = false, select = false;
    bool x = false, y = false, left_click = false, right_click = false;
    std::uint8_t left_trigger = 0, right_trigger = 0;
};
GameInput map_gamepad(const GamepadState& pad);
GameInput combine_inputs(const GameInput& keyboard, const GameInput& pad);
// Converts semantic VB buttons to the existing core's libretro button IDs.
std::uint16_t libretro_buttons(const GameInput& input);
}
