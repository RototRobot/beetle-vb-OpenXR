// SPDX-License-Identifier: GPL-2.0-or-later
#include "vr_input.h"
#include <algorithm>
#include <cmath>

namespace bvb {
namespace {
bool threshold(float value, bool previous, float press, float release) {
    return std::isfinite(value) && value > (previous ? release : press);
}
}
void VrInputMapper::reset() { armed = false; directions = {}; triggers = {}; grips = {}; menu_axes = {}; }
VrInput VrInputMapper::update(const std::array<VrHandInput, 2>& hands, bool focused, float deadzone) {
    if (!focused) { reset(); return {}; }
    deadzone = std::isfinite(deadzone) ? std::clamp(deadzone, vr_min_deadzone, vr_max_deadzone) : vr_default_deadzone;
    const float release = deadzone * .75f;
    VrInput result;
    for (std::size_t hand = 0; hand < hands.size(); ++hand) {
        const auto& input = hands[hand]; auto& dir = directions[hand];
        if (input.axis_active && input.axis_contact && std::isfinite(input.x) && std::isfinite(input.y)) {
            dir[0] = threshold(input.y, dir[0], deadzone, release);
            dir[1] = threshold(-input.y, dir[1], deadzone, release);
            dir[2] = threshold(-input.x, dir[2], deadzone, release);
            dir[3] = threshold(input.x, dir[3], deadzone, release);
            const float x = std::abs(input.x), y = std::abs(input.y);
            if (std::max(x, y) <= release) menu_axes[hand] = 0;
            else if (!menu_axes[hand] && std::max(x, y) > deadzone) {
                // Wait out an ambiguous diagonal before committing the gesture.
                if (y > x * 1.25f) menu_axes[hand] = 2;
                else if (x > y * 1.25f) menu_axes[hand] = 1;
            }
        } else { dir = {}; menu_axes[hand] = 0; }
        triggers[hand] = input.trigger_active && threshold(input.trigger, triggers[hand], .55f, .45f);
        grips[hand] = input.grip_active && threshold(input.grip,grips[hand],.55f,.45f);
        const auto physical_offset=hand?299:288;
        for(std::size_t i=0;i<dir.size();++i) result.sources.held[physical_offset+i]=dir[i];
        const bool physical[]={triggers[hand],input.a,input.b,input.start,input.select,input.menu_click,grips[hand]};
        for(int i=0;i<7;++i) result.sources.held[physical_offset+4+i]=physical[i];
        const auto offset = hand ? Button::right_up : Button::left_up;
        for (std::size_t i = 0; i < dir.size(); ++i) {
            result.game.buttons[static_cast<std::size_t>(offset) + i] = dir[i];
            result.menu_directions.buttons[static_cast<std::size_t>(offset) + i] = dir[i]
                && menu_axes[hand] == (i < 2 ? 2 : 1);
        }
        result.game[hand ? Button::r : Button::l] = triggers[hand];
        result.game[Button::a] = result.game[Button::a] || input.a;
        result.game[Button::b] = result.game[Button::b] || input.b;
        result.game[Button::start] = result.game[Button::start] || input.start;
        result.game[Button::select] = result.game[Button::select] || input.select;
        result.held = result.held || input.menu_click || grips[hand];
    }
    for (bool button : result.game.buttons) result.held = result.held || button;
    result.menu = hands[0].menu_click && hands[1].menu_click;
    result.accept = result.game[Button::a] || triggers[1];
    result.back = result.game[Button::b] || triggers[0];
    // Initial connection, profile changes and focus recovery require release.
    if (!armed) {
        if (!result.held) armed = true;
        return {};
    }
    return result;
}
const std::vector<VrProfile>& vr_profiles() {
    static const std::vector<VrProfile> profiles = {
        {"/interaction_profiles/oculus/touch_controller", {
            {VrAction::direction, "/user/hand/left/input/thumbstick"}, {VrAction::direction, "/user/hand/right/input/thumbstick"},
            {VrAction::trigger, "/user/hand/left/input/trigger/value"}, {VrAction::trigger, "/user/hand/right/input/trigger/value"},
            {VrAction::grip, "/user/hand/left/input/squeeze/value"}, {VrAction::grip, "/user/hand/right/input/squeeze/value"},
            {VrAction::a, "/user/hand/right/input/a/click"}, {VrAction::b, "/user/hand/right/input/b/click"},
            {VrAction::select, "/user/hand/left/input/x/click"}, {VrAction::start, "/user/hand/left/input/y/click"},
            {VrAction::menu_click, "/user/hand/left/input/thumbstick/click"}, {VrAction::menu_click, "/user/hand/right/input/thumbstick/click"}}},
        {"/interaction_profiles/valve/index_controller", {
            {VrAction::direction, "/user/hand/left/input/thumbstick"}, {VrAction::direction, "/user/hand/right/input/thumbstick"},
            {VrAction::trigger, "/user/hand/left/input/trigger/value"}, {VrAction::trigger, "/user/hand/right/input/trigger/value"},
            {VrAction::grip, "/user/hand/left/input/squeeze/value"}, {VrAction::grip, "/user/hand/right/input/squeeze/value"},
            {VrAction::a, "/user/hand/right/input/a/click"}, {VrAction::b, "/user/hand/right/input/b/click"},
            {VrAction::select, "/user/hand/left/input/a/click"}, {VrAction::start, "/user/hand/left/input/b/click"},
            {VrAction::menu_click, "/user/hand/left/input/thumbstick/click"}, {VrAction::menu_click, "/user/hand/right/input/thumbstick/click"}}},
        {"/interaction_profiles/microsoft/motion_controller", {
            {VrAction::direction, "/user/hand/left/input/thumbstick"}, {VrAction::direction, "/user/hand/right/input/thumbstick"},
            {VrAction::trigger, "/user/hand/left/input/trigger/value"}, {VrAction::trigger, "/user/hand/right/input/trigger/value"},
            {VrAction::a, "/user/hand/right/input/squeeze/click"}, {VrAction::b, "/user/hand/left/input/squeeze/click"},
            {VrAction::select, "/user/hand/left/input/menu/click"}, {VrAction::start, "/user/hand/right/input/menu/click"},
            {VrAction::menu_click, "/user/hand/left/input/thumbstick/click"}, {VrAction::menu_click, "/user/hand/right/input/thumbstick/click"}}},
        {"/interaction_profiles/htc/vive_controller", {
            {VrAction::direction, "/user/hand/left/input/trackpad"}, {VrAction::direction, "/user/hand/right/input/trackpad"},
            {VrAction::axis_touch, "/user/hand/left/input/trackpad/touch"}, {VrAction::axis_touch, "/user/hand/right/input/trackpad/touch"},
            {VrAction::trigger, "/user/hand/left/input/trigger/value"}, {VrAction::trigger, "/user/hand/right/input/trigger/value"},
            {VrAction::a, "/user/hand/right/input/squeeze/click"}, {VrAction::b, "/user/hand/left/input/squeeze/click"},
            {VrAction::select, "/user/hand/left/input/menu/click"}, {VrAction::start, "/user/hand/right/input/menu/click"},
            {VrAction::menu_click, "/user/hand/left/input/trackpad/click"}, {VrAction::menu_click, "/user/hand/right/input/trackpad/click"}}}
    };
    return profiles;
}
}
