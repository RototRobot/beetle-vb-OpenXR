// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "game_input.h"
#include "controller_bindings.h"
#include <array>
#include <vector>

namespace bvb {
inline constexpr float vr_default_deadzone = .1f;
inline constexpr float vr_min_deadzone = .05f, vr_max_deadzone = .6f;
// Runtime adapter clears inactive values before handing them to this mapper.
struct VrHandInput {
    float x = 0, y = 0, trigger = 0;
    bool axis_active = false, axis_contact = true, trigger_active = false;
    bool a = false, b = false, start = false, select = false, menu_click = false;
    float grip = 0;
    bool grip_active = false;
};
struct VrInput {
    GameInput game;
    GameInput menu_directions; // One axis per stick gesture, separate from gameplay diagonals.
    bool menu = false, accept = false, back = false;
    bool held = false;
    SourceInput sources;
};
class VrInputMapper {
public:
    VrInput update(const std::array<VrHandInput, 2>& hands, bool focused, float deadzone = vr_default_deadzone);
    void reset();
private:
    bool armed = false;
    std::array<std::array<bool, 4>, 2> directions{};
    std::array<bool, 2> triggers{};
    std::array<bool, 2> grips{};
    std::array<int, 2> menu_axes{}; // 0 neutral, 1 horizontal, 2 vertical; reset at centre.
};
// Semantic actions and suggested component bindings are shared by the runtime
// adapter and registry-validation tests; these are OpenXR paths, not SDK APIs.
enum class VrAction { direction, axis_touch, trigger, a, b, start, select, menu_click, grip, count };
enum class VrActionType { vector2, boolean, scalar };
struct VrActionDefinition { const char* name; const char* label; VrActionType type; };
inline constexpr VrActionDefinition vr_actions[] = {
    {"direction", "Directional pad", VrActionType::vector2},
    {"axis_touch", "Trackpad contact", VrActionType::boolean},
    {"trigger", "Virtual Boy L or R", VrActionType::scalar},
    {"vb_a", "Virtual Boy A", VrActionType::boolean},
    {"vb_b", "Virtual Boy B", VrActionType::boolean},
    {"vb_start", "Virtual Boy Start", VrActionType::boolean},
    {"vb_select", "Virtual Boy Select", VrActionType::boolean},
    {"menu_click", "Settings chord click", VrActionType::boolean},
    {"grip", "Controller grip", VrActionType::scalar}};
struct VrBinding { VrAction action; const char* component; };
struct VrProfile { const char* path; std::vector<VrBinding> bindings; };
const std::vector<VrProfile>& vr_profiles();
}
