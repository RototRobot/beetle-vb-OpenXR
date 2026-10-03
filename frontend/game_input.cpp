// SPDX-License-Identifier: GPL-2.0-or-later
#include "game_input.h"
#include <libretro.h>
#include <initializer_list>

namespace bvb {
GameInput combine_inputs(const GameInput& keyboard, const GameInput& pad) {
    GameInput result;
    for (std::size_t i = 0; i < result.buttons.size(); ++i) result.buttons[i] = keyboard.buttons[i] || pad.buttons[i];
    for (auto pair : {std::array<Button, 2>{Button::left_up, Button::left_down},
            std::array<Button, 2>{Button::left_left, Button::left_right},
            std::array<Button, 2>{Button::right_up, Button::right_down},
            std::array<Button, 2>{Button::right_left, Button::right_right}})
        if (result[pair[0]] && result[pair[1]]) result[pair[0]] = result[pair[1]] = false;
    return result;
}
GameInput map_gamepad(const GamepadState& pad) {
    constexpr int deadzone = 8192;
    GameInput result;
    result[Button::left_up] = pad.up || pad.left_y > deadzone;
    result[Button::left_down] = pad.down || pad.left_y < -deadzone;
    result[Button::left_left] = pad.left || pad.left_x < -deadzone;
    result[Button::left_right] = pad.right || pad.left_x > deadzone;
    result[Button::right_up] = pad.right_y > deadzone;
    result[Button::right_down] = pad.right_y < -deadzone;
    result[Button::right_left] = pad.right_x < -deadzone;
    result[Button::right_right] = pad.right_x > deadzone;
    result[Button::a] = pad.a; result[Button::b] = pad.b;
    result[Button::l] = pad.l; result[Button::r] = pad.r;
    result[Button::start] = pad.start; result[Button::select] = pad.select;
    return combine_inputs({}, result);
}
std::uint16_t libretro_buttons(const GameInput& input) {
    constexpr std::array<unsigned, static_cast<std::size_t>(Button::count)> mapping = {
        RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
        RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT,
        RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_L3,
        RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_ID_JOYPAD_R3,
        RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B,
        RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R,
        RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT};
    std::uint16_t mask = 0;
    for (std::size_t i = 0; i < mapping.size(); ++i)
        if (input.buttons[i]) mask |= std::uint16_t(1u << mapping[i]);
    return mask;
}
}
