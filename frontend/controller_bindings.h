// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "game_input.h"
#include <string>
namespace bvb {
enum class InputDevice : int { keyboard, gamepad, vr, count };
// Stable IDs for saved bindings. Keyboard IDs use Windows virtual-key numbers.
enum class InputSource : int {
    none=0, pad_up=256,pad_down,pad_left,pad_right,
    pad_l_up,pad_l_down,pad_l_left,pad_l_right,pad_r_up,pad_r_down,pad_r_left,pad_r_right,
    pad_a,pad_b,pad_x,pad_y,pad_lb,pad_rb,pad_lt,pad_rt,pad_start,pad_back,pad_l_click,pad_r_click,
    vr_l_up=288,vr_l_down,vr_l_left,vr_l_right,vr_l_trigger,vr_l_a,vr_l_b,vr_l_start,vr_l_select,vr_l_click,vr_l_grip,
    vr_r_up,vr_r_down,vr_r_left,vr_r_right,vr_r_trigger,vr_r_a,vr_r_b,vr_r_start,vr_r_select,vr_r_click,vr_r_grip,
    count=320
};
struct SourceInput {
    std::array<bool,static_cast<std::size_t>(InputSource::count)> held{};
    bool& operator[](InputSource source) {return held[static_cast<std::size_t>(source)];}
    bool operator[](InputSource source) const {return held[static_cast<std::size_t>(source)];}
    bool any() const;
};
struct InputBinding {int primary=0,secondary=0;};
using DeviceBindings=std::array<InputBinding,static_cast<std::size_t>(Button::count)>;
using ControllerBindings=std::array<DeviceBindings,3>;
ControllerBindings default_controller_bindings();
bool bindable_key(int key);
bool valid_binding_source(int source,InputDevice device);
InputDevice source_device(int source);
std::string source_name(int source);
std::string binding_name(const InputBinding& binding);
inline constexpr const char* vb_input_names[]={"LEFT PAD UP","LEFT PAD DOWN","LEFT PAD LEFT","LEFT PAD RIGHT",
    "RIGHT PAD UP","RIGHT PAD DOWN","RIGHT PAD LEFT","RIGHT PAD RIGHT","A","B","L","R","START","SELECT"};
inline constexpr const char* vb_input_keys[]={"left_up","left_down","left_left","left_right",
    "right_up","right_down","right_left","right_right","a","b","l","r","start","select"};
inline constexpr const char* device_names[]={"KEYBOARD","XINPUT","VR CONTROLLERS"};
inline constexpr const char* device_keys[]={"keyboard","gamepad","vr"};
SourceInput combine_sources(const SourceInput& a,const SourceInput& b);
SourceInput gamepad_sources(const GamepadState& pad);
GameInput apply_controller_bindings(const SourceInput& sources,const ControllerBindings& bindings);
}
