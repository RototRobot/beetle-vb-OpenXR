// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings_menu.h"
#include "save_ram.h"
#include "ui_draw.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace bvb {
void PlayerSettings::clamp() {
    const auto defaults=default_controller_bindings();
    for(int device=0;device<3;++device) for(std::size_t i=0;i<14;++i) {
        const auto value=bindings[device][i];
        if(!valid_binding_source(value.primary,InputDevice(device)) || !valid_binding_source(value.secondary,InputDevice(device)))
            bindings[device][i]=defaults[device][i];
    }
    if(int(mapping_view)<0 || int(mapping_view)>2) mapping_view=InputDevice::vr;
    screen.clamp(); palette = std::clamp(palette, 0, 7);
    state_slot = std::clamp(state_slot, 0, 9);
    zoom = std::isfinite(zoom) ? std::clamp(zoom, .5f, 2.f) : 1;
    volume = std::isfinite(volume) ? std::clamp(volume, 0.f, 1.f) : 1;
    stick_deadzone = std::isfinite(stick_deadzone) ? std::clamp(stick_deadzone, vr_min_deadzone, vr_max_deadzone) : vr_default_deadzone;
}
PlayerSettings load_settings(const std::filesystem::path& path) {
    PlayerSettings result;
    if (!std::filesystem::exists(path)) return result;
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot read player settings");
    std::string line;
    while (std::getline(file, line)) {
        if (line.size() > 256) continue;
        const auto equals = line.find('=');
        if (equals == std::string::npos) continue;
        const auto key = line.substr(0, equals);
        std::istringstream input(line.substr(equals + 1)); input.imbue(std::locale::classic());
        bool binding_key=false;
        for(int device=0;device<3;++device) for(int target=0;target<14;++target) {
            if(key!=std::string("bind_")+device_keys[device]+"_"+vb_input_keys[target]) continue;
            binding_key=true; int primary=0,secondary=0; char colon=0;
            if(input>>primary>>colon>>secondary && colon==':') {
                input>>std::ws;
                if(input.eof() && valid_binding_source(primary,InputDevice(device)) && valid_binding_source(secondary,InputDevice(device)))
                    result.bindings[device][target]={primary,secondary};
            }
        }
        if(binding_key) continue;
        float value;
        if (!(input >> value) || !std::isfinite(value)) continue;
        input >> std::ws; if (!input.eof()) continue;
        if(key=="mapping_view" && value>=0 && value<=2 && value==std::floor(value)) result.mapping_view=InputDevice(int(value));
        else if (key == "width" && value >= .3f && value <= 3) result.screen.width = value;
        else if (key == "distance" && value >= .5f && value <= 5) result.screen.distance = value;
        else if (key == "zoom" && value >= .5f && value <= 2) result.zoom = value;
        else if (key == "volume" && value >= 0 && value <= 1) result.volume = value;
        else if (key == "vr_stick_deadzone" && value >= vr_min_deadzone && value <= vr_max_deadzone) result.stick_deadzone = value;
        else if (key == "palette" && value >= 0 && value <= 7 && value == std::floor(value)) result.palette = int(value);
        else if (key == "state_slot" && value >= 0 && value <= 9 && value == std::floor(value)) result.state_slot = int(value);
        else if (key == "swap_eyes" && (value == 0 || value == 1)) result.screen.swap_eyes = value == 1;
        else if (key == "smooth" && (value == 0 || value == 1)) result.smooth = value == 1;
        else if (key == "muted" && (value == 0 || value == 1)) result.muted = value == 1;
    }
    return result;
}
std::string encode_settings(const PlayerSettings& settings) {
    auto s = settings; s.clamp();
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(6);
    out << "# Beetle VB OpenXR player preferences\nwidth=" << s.screen.width
        << "\ndistance=" << s.screen.distance << "\nswap_eyes=" << s.screen.swap_eyes
        << "\npalette=" << s.palette << "\nzoom=" << s.zoom << "\nsmooth=" << s.smooth
        << "\nvolume=" << s.volume << "\nmuted=" << s.muted << "\nvr_stick_deadzone=" << s.stick_deadzone
        << "\nstate_slot=" << s.state_slot << '\n';
    out<<"mapping_view="<<int(s.mapping_view)<<'\n';
    for(int device=0;device<3;++device) for(int target=0;target<14;++target) {
        const auto b=s.bindings[device][target];
        out<<"bind_"<<device_keys[device]<<'_'<<vb_input_keys[target]<<'='<<b.primary<<':'<<b.secondary<<'\n';
    }
    return out.str();
}
void save_settings(const std::filesystem::path& path, const PlayerSettings& settings) {
    const auto text = encode_settings(settings);
    write_atomic_file(path, text.data(), text.size());
}
bool MenuInput::any() const { return toggle || up || down || left || right || accept || back || skip; }
MenuActions SettingsMenu::update(const MenuInput& input, PlayerSettings& settings, const SourceInput& sources) {
    MenuActions actions;
    const bool toggle = input.toggle && !previous.toggle;
    const bool was_open = open;
    const auto old_mapping_status=mapping_status;
    const int old_mapping_selected=mapping_selected, old_capture=capture_target;
    const bool old_mapping_open=mapping_open, old_ready=capture_ready;
    if(open && mapping_open) update_mapping(input,settings,sources,actions);
    else if (toggle) {open = !open; mapping_open=false; capture_target=-1; wizard=false;}
    else if (open) {
        if (input.back && !previous.back) open = false;
        else {
            if (input.up && !input.down && !previous.up) selected = (selected + menu_row_count - 1) % menu_row_count;
            if (input.down && !input.up && !previous.down) selected = (selected + 1) % menu_row_count;
            // Moving between rows must not also adjust the row's value.
            const int direction = (input.up || input.down) ? 0
                : (input.right && !previous.right) - (input.left && !previous.left);
            const bool accept = input.accept && !previous.accept;
            if (direction || accept) {
                const int step = direction ? direction : 1;
                switch (static_cast<MenuItem>(selected)) {
                case MenuItem::resume: if (accept) { open = false; actions.resume = true; } break;
                case MenuItem::palette: settings.palette = (settings.palette + step + 8) % 8; break;
                case MenuItem::width: settings.screen.width += step * .1f; break;
                case MenuItem::distance: settings.screen.distance += step * .1f; break;
                case MenuItem::zoom: settings.zoom = std::round((settings.zoom + step * .1f) * 10) / 10; break;
                case MenuItem::filter: settings.smooth = !settings.smooth; break;
                case MenuItem::stick_deadzone: settings.stick_deadzone = std::round((settings.stick_deadzone + step * .05f) * 20) / 20; break;
                case MenuItem::swap_eyes: settings.screen.swap_eyes = !settings.screen.swap_eyes; break;
                case MenuItem::recenter: if (accept) actions.recenter = true; break;
                case MenuItem::volume: settings.volume = std::round((settings.volume + step * .05f) * 20) / 20; break;
                case MenuItem::mute: settings.muted = !settings.muted; break;
                case MenuItem::state_slot: settings.state_slot = (settings.state_slot + step + 10) % 10; break;
                case MenuItem::save_state: if (accept) actions.save_state = true; break;
                case MenuItem::load_state: if (accept) actions.load_state = true; break;
                case MenuItem::controls: if(accept) {mapping_open=true; mapping_selected=0; mapping_status="SELECT WIZARD OR AN INPUT"; ++revision;} break;
                case MenuItem::library: if (accept) actions.library = true; break;
                case MenuItem::exit: if (accept) actions.exit = true; break;
                case MenuItem::count: break;
                }
                settings.clamp(); actions.changed = true;
            }
        }
    }
    if (was_open && !open) release_gate = true;
    if (!open && !input.any()) release_gate = false;
    if(old_mapping_status!=mapping_status || old_mapping_selected!=mapping_selected || old_capture!=capture_target
        || old_mapping_open!=mapping_open || old_ready!=capture_ready) ++revision;
    previous = input;
    return actions;
}
void SettingsMenu::suspend_capture() {
    if(capture_target>=0) {capture_ready=false; mapping_status="RELEASE CONTROLS AFTER FOCUS CHANGE"; ++revision;}
}
void SettingsMenu::update_mapping(const MenuInput& input, PlayerSettings& settings,const SourceInput& sources,MenuActions& actions) {
    const bool cancel=input.toggle && !previous.toggle;
    if(capture_target>=0) {
        if(cancel) {
            capture_target=-1; wizard=false; capture_ready=false;
            mapping_status="CAPTURE CANCELLED - PREVIOUS STEPS SAVED"; return;
        }
        const bool skip=input.skip || (sources[InputSource::vr_l_trigger] && sources[InputSource::vr_r_trigger])
            || (sources[InputSource::pad_lb] && sources[InputSource::pad_rb]);
        if(!capture_ready) {
            if(!sources.any() && !input.any()) {capture_ready=true; mapping_status="PRESS INPUT FOR "+std::string(vb_input_names[capture_target]);}
            return;
        }
        int source=0,count=0;
        for(int i=1;i<int(InputSource::count);++i) if(sources.held[i] && valid_binding_source(i,source_device(i))) {source=i; ++count;}
        if(!skip && !count) return;
        if(!skip && count!=1) {mapping_status="RELEASE - PRESS ONE INPUT AT A TIME"; capture_ready=false; return;}
        if(!skip) {
            const auto device=source_device(source);
            if(wizard && wizard_device>=0 && int(device)!=wizard_device) {mapping_status="USE "+std::string(device_names[wizard_device])+" OR CANCEL"; capture_ready=false; return;}
            if(wizard && wizard_device<0) wizard_device=int(device);
            settings.mapping_view=device; settings.bindings[int(device)][capture_target]={source,0}; actions.changed=true;
            mapping_status="BOUND "+std::string(vb_input_names[capture_target])+" TO "+source_name(source);
        } else mapping_status="SKIPPED "+std::string(vb_input_names[capture_target]);
        mapping_selected=capture_target+1;
        if(wizard && capture_target<13) {++capture_target; mapping_selected=capture_target+1; capture_ready=false; mapping_status="RELEASE THEN BIND "+std::string(vb_input_names[capture_target]);}
        else {capture_target=-1; if(wizard) mapping_status="WIZARD COMPLETE - MAPPINGS SAVED"; wizard=false; capture_ready=false;}
        return; // Captured accept/back/direction never navigates the menu.
    }
    if(cancel || (input.back && !previous.back)) {mapping_open=false; return;}
    if(input.up && !input.down && !previous.up) mapping_selected=(mapping_selected+15)%16;
    if(input.down && !input.up && !previous.down) mapping_selected=(mapping_selected+1)%16;
    if(!input.up && !input.down) {
        const int step=(input.right && !previous.right)-(input.left && !previous.left);
        if(step) {settings.mapping_view=InputDevice((int(settings.mapping_view)+step+3)%3); actions.changed=true;}
    }
    if(input.accept && !previous.accept && !input.up && !input.down && !input.left && !input.right) {
        if(mapping_selected==15) {mapping_open=false; return;}
        wizard=mapping_selected==0; wizard_device=-1; capture_target=wizard?0:mapping_selected-1;
        mapping_selected=capture_target+1; capture_ready=false; mapping_status="RELEASE ALL CONTROLS THEN PRESS INPUT";
    }
}
namespace {
using ui::rectangle;
using ui::text;
using ui::Color;
std::string metres(float value) {
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::fixed << std::setprecision(1) << value << " M"; return out.str();
}
}
StereoFrame SettingsMenu::render(const StereoFrame& game, const PlayerSettings& s) const {
    if (!open) return game;
    StereoFrame result;
    result.left.resize(eye_width * eye_height * 4);
    rectangle(result.left, 0, 0, eye_width, eye_height, {12,18,26});
    text(result.left, 18, 10, "VIRTUAL BOY", {235,242,255}, 2);
    if(mapping_open) {
        text(result.left,18,29,std::string("CONTROLS / ")+device_names[int(s.mapping_view)],{135,160,185});
        for(int row=0;row<16;++row) {
            const int y=47+row*9;
            if(row==mapping_selected) rectangle(result.left,12,y-1,360,9,{27,69,91});
            const auto color=row==mapping_selected?Color{128,238,255}:Color{215,225,235};
            const auto label=row==0?"REBIND WIZARD":row==15?"BACK":vb_input_names[row-1];
            std::string value=row==0?"BIND ALL INPUTS":row==15?"MAIN SETTINGS":binding_name(s.bindings[int(s.mapping_view)][row-1]);
            if(row==capture_target+1 && capture_target>=0) value=capture_ready?"PRESS INPUT":"RELEASE CONTROLS";
            value=value.substr(0,28);
            text(result.left,22,y,label,color); text(result.left,360-int(value.size())*6,y,value,color);
        }
        text(result.left,18,198,mapping_status.substr(0,58),{128,238,255});
        text(result.left,18,213,capture_target>=0?"MENU/ESC CANCEL  BOTH TRIGGERS/LB+RB/F4 SKIP":"LEFT/RIGHT DEVICE  SELECT REBIND  BACK RETURN",{135,160,185});
        result.right=result.left; return result;
    }
    text(result.left, 18, 29, "SETTINGS / GAME PAUSED", {135,160,185});
    const char* labels[] = {"RESUME GAME", "PALETTE", "SCREEN WIDTH", "DISTANCE", "IMAGE ZOOM", "ZOOM FILTER",
        "VR STICK DEADZONE", "SWAP EYES", "RECENTER SCREEN", "VOLUME", "MUTE", "STATE SLOT", "SAVE STATE", "LOAD STATE", "CONTROLLER MAPPING", "GAME LIBRARY", "EXIT PLAYER"};
    const std::string values[] = {"A/ENTER/R-TRIGGER", palette_names[s.palette], metres(s.screen.width), metres(s.screen.distance),
        std::to_string(int(std::lround(s.zoom * 100))) + "%", s.smooth ? "SMOOTH" : "CRISP",
        std::to_string(int(std::lround(s.stick_deadzone * 100))) + "%", s.screen.swap_eyes ? "ON" : "OFF",
        "A/ENTER/R-TRIGGER", std::to_string(int(std::lround(s.volume * 100))) + "%", s.muted ? "ON" : "OFF",
        std::to_string(s.state_slot+1) + (slot_occupied ? " / USED" : " / EMPTY"), "A/ENTER/R-TRIGGER",
        slot_occupied ? "A/ENTER/R-TRIGGER" : "EMPTY", "A/ENTER/R-TRIGGER", "A/ENTER/R-TRIGGER", "A/ENTER/R-TRIGGER"};
    for (int row = 0; row < menu_row_count; ++row) {
        const int y = 47 + row * 8;
        if (selected == row) rectangle(result.left, 12, y - 1, 360, 8, {27,69,91});
        const Color color = selected == row ? Color{128,238,255} : Color{215,225,235};
        text(result.left, 22, y, labels[row], color);
        text(result.left, 360 - int(values[row].size()) * 6, y, values[row], color);
    }
    text(result.left, 18, 198, status.substr(0,58), {128,238,255});
    text(result.left, 18, 213, "PAD MOVE  LEFT/RIGHT CHANGE  B/L-TRIGGER CLOSE", {135,160,185});
    result.right = result.left; // Same screen plane and zero disparity for all menu text.
    return result;
}
StereoFrame prepare_game_image(const StereoFrame& frame, const PlayerSettings& s) {
    if (frame.left.size() != eye_width * eye_height * 4 || frame.right.size() != frame.left.size())
        throw std::invalid_argument("Expected complete native eye images");
    if (s.zoom == 1) return frame;
    StereoFrame result;
    for (int eye = 0; eye < 2; ++eye) {
        const auto& source = eye ? frame.right : frame.left;
        auto& dest = eye ? result.right : result.left; dest.resize(source.size());
        for (unsigned y = 0; y < eye_height; ++y) for (unsigned x = 0; x < eye_width; ++x) {
            const float sx = (x + .5f - eye_width / 2.f) / s.zoom + eye_width / 2.f - .5f;
            const float sy = (y + .5f - eye_height / 2.f) / s.zoom + eye_height / 2.f - .5f;
            const auto i = (y * eye_width + x) * 4; dest[i+3] = 255;
            if (sx < -.5f || sy < -.5f || sx >= eye_width - .5f || sy >= eye_height - .5f) continue;
            auto sample = [&](int px, int py, int c) {
                px = std::clamp(px, 0, int(eye_width)-1); py = std::clamp(py, 0, int(eye_height)-1);
                return float(source[(py * eye_width + px)*4+c]);
            };
            for (int c = 0; c < 3; ++c) {
                float value;
                if (!s.smooth) value = sample(int(std::floor(sx + .5f)), int(std::floor(sy + .5f)), c);
                else {
                    const int ix = int(std::floor(sx)), iy = int(std::floor(sy));
                    const float fx = sx-ix, fy = sy-iy;
                    value = (sample(ix,iy,c)*(1-fx)+sample(ix+1,iy,c)*fx)*(1-fy)
                        + (sample(ix,iy+1,c)*(1-fx)+sample(ix+1,iy+1,c)*fx)*fy;
                }
                dest[i+c] = static_cast<std::uint8_t>(std::lround(value));
            }
        }
    }
    return result;
}
}
