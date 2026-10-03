// SPDX-License-Identifier: GPL-2.0-or-later
#include "controller_bindings.h"
#include <algorithm>
namespace bvb {
bool SourceInput::any() const {return std::any_of(held.begin(),held.end(),[](bool b){return b;});}
bool bindable_key(int key) {
    if(key=='P' || key=='M') return false; // Existing pause/mute shortcuts.
    return (key>='A' && key<='Z') || (key>='0' && key<='9') || (key>=0x60 && key<=0x69)
        || key==0x08 || key==0x09 || key==0x0D || key==0x20 || (key>=0x21 && key<=0x28) || key==0x2D || key==0x2E;
}
bool valid_binding_source(int source,InputDevice device) {
    if(!source) return true;
    switch(device) {
    case InputDevice::keyboard: return bindable_key(source);
    case InputDevice::gamepad: return source>=256 && source<=279;
    case InputDevice::vr: return source>=288 && source<=309;
    default: return false;
    }
}
InputDevice source_device(int source) {return source<256?InputDevice::keyboard:source<288?InputDevice::gamepad:InputDevice::vr;}
ControllerBindings default_controller_bindings() {
    ControllerBindings result{};
    const int keys[]={'W','S','A','D',0x26,0x28,0x25,0x27,'K','J','Q','E',0x0D,0x20};
    const InputSource pads[]={InputSource::pad_l_up,InputSource::pad_l_down,InputSource::pad_l_left,InputSource::pad_l_right,
        InputSource::pad_r_up,InputSource::pad_r_down,InputSource::pad_r_left,InputSource::pad_r_right,InputSource::pad_a,InputSource::pad_b,
        InputSource::pad_lb,InputSource::pad_rb,InputSource::pad_start,InputSource::pad_back};
    const InputSource vr[]={InputSource::vr_l_up,InputSource::vr_l_down,InputSource::vr_l_left,InputSource::vr_l_right,
        InputSource::vr_r_up,InputSource::vr_r_down,InputSource::vr_r_left,InputSource::vr_r_right,InputSource::vr_r_a,InputSource::vr_r_b,
        InputSource::vr_l_trigger,InputSource::vr_r_trigger,InputSource::vr_l_start,InputSource::vr_l_select};
    for(std::size_t i=0;i<14;++i) {result[0][i].primary=keys[i]; result[1][i].primary=int(pads[i]); result[2][i].primary=int(vr[i]);}
    for(int i=0;i<4;++i) result[1][i].secondary=256+i; // Default D-pad and left stick both work.
    result[2][8].secondary=int(InputSource::vr_l_a); result[2][9].secondary=int(InputSource::vr_l_b);
    result[2][12].secondary=int(InputSource::vr_r_start); result[2][13].secondary=int(InputSource::vr_r_select);
    return result;
}
std::string source_name(int source) {
    if(!source) return "UNBOUND";
    if(source<256) {
        if((source>='A' && source<='Z') || (source>='0' && source<='9')) return std::string("KEY ")+char(source);
        switch(source) {case 0x08:return "KEY BACKSPACE"; case 0x09:return "KEY TAB"; case 0x0D:return "KEY ENTER";
        case 0x20:return "KEY SPACE"; case 0x25:return "KEY LEFT"; case 0x26:return "KEY UP";
        case 0x27:return "KEY RIGHT"; case 0x28:return "KEY DOWN"; default:return "KEY "+std::to_string(source);}
    }
    static const char* pads[]={"PAD DPAD UP","PAD DPAD DOWN","PAD DPAD LEFT","PAD DPAD RIGHT",
        "PAD L STICK UP","PAD L STICK DOWN","PAD L STICK LEFT","PAD L STICK RIGHT",
        "PAD R STICK UP","PAD R STICK DOWN","PAD R STICK LEFT","PAD R STICK RIGHT",
        "PAD A","PAD B","PAD X","PAD Y","PAD LB","PAD RB","PAD LT","PAD RT","PAD START","PAD BACK","PAD L CLICK","PAD R CLICK"};
    static const char* vr[]={"VR L STICK UP","VR L STICK DOWN","VR L STICK LEFT","VR L STICK RIGHT","VR L TRIGGER",
        "VR L A/GRIP","VR L B/GRIP","VR L Y/B/MENU","VR L X/A/MENU","VR L CLICK","VR L GRIP",
        "VR R STICK UP","VR R STICK DOWN","VR R STICK LEFT","VR R STICK RIGHT","VR R TRIGGER",
        "VR R A/GRIP","VR R B","VR R MENU","VR R SELECT","VR R CLICK","VR R GRIP"};
    if(source>=256 && source<=279) return pads[source-256];
    if(source>=288 && source<=309) return vr[source-288];
    return "INVALID";
}
std::string binding_name(const InputBinding& binding) {
    auto name=source_name(binding.primary);
    if(binding.secondary) name+=" + "+source_name(binding.secondary);
    return name;
}
SourceInput combine_sources(const SourceInput& a,const SourceInput& b) {
    SourceInput result; for(std::size_t i=0;i<result.held.size();++i) result.held[i]=a.held[i] || b.held[i]; return result;
}
SourceInput gamepad_sources(const GamepadState& p) {
    SourceInput s;
    const bool values[]={p.up,p.down,p.left,p.right,p.left_y>8192,p.left_y< -8192,p.left_x< -8192,p.left_x>8192,
        p.right_y>8192,p.right_y< -8192,p.right_x< -8192,p.right_x>8192,p.a,p.b,p.x,p.y,p.l,p.r,
        p.left_trigger>140,p.right_trigger>140,p.start,p.select,p.left_click,p.right_click};
    for(int i=0;i<24;++i) s.held[256+i]=values[i]; return s;
}
GameInput apply_controller_bindings(const SourceInput& sources,const ControllerBindings& bindings) {
    GameInput result;
    for(int device=0;device<3;++device) for(std::size_t target=0;target<result.buttons.size();++target) {
        const auto b=bindings[device][target];
        auto held=[&](int id){return id && valid_binding_source(id,InputDevice(device)) && sources.held[id];};
        result.buttons[target]=result.buttons[target] || held(b.primary) || held(b.secondary);
    }
    return combine_inputs({},result);
}
}
