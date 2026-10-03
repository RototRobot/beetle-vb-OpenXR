// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings_menu.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace bvb;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
SourceInput pressed(InputSource source) {SourceInput s; s[source]=true; return s;}
MenuInput accept() {MenuInput m; m.accept=true; return m;}
MenuInput back() {MenuInput m; m.back=true; return m;}
void capture(SettingsMenu& m,PlayerSettings& s,int row) {
    m.mapping_selected=row; m.update({},s); m.update(accept(),s,pressed(InputSource::vr_r_trigger));
    require(m.capture_target==row-1 || row==0,"Selection did not start capture");
}
void export_ppm(const std::filesystem::path& path,const StereoFrame& frame) {
    std::ofstream out(path,std::ios::binary); out<<"P6\n"<<frame.width<<' '<<frame.height<<"\n255\n";
    for(std::size_t i=0;i<frame.left.size();i+=4) out.write(reinterpret_cast<const char*>(frame.left.data()+i),3);
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected output directory");
        PlayerSettings s; const auto defaults=s.bindings;
        GamepadState pad; pad.up=true; pad.left_x=-20000; pad.right_y=-20000;
        pad.a=pad.b=pad.l=pad.r=pad.start=pad.select=true;
        require(apply_controller_bindings(gamepad_sources(pad),defaults).buttons==map_gamepad(pad).buttons,
            "Default mappings changed existing XInput behavior");
        pad={}; pad.x=true; pad.y=true; pad.left_click=true; pad.right_click=true; pad.left_trigger=255;
        auto physical=gamepad_sources(pad);
        require(physical[InputSource::pad_x] && physical[InputSource::pad_y] && physical[InputSource::pad_lt]
            && physical[InputSource::pad_l_click] && physical[InputSource::pad_r_click],"Extra XInput controls missing");
        s.bindings[1][8]={int(InputSource::pad_x),0};
        require(apply_controller_bindings(physical,s.bindings)[Button::a],"New binding not used by gameplay");
        s.bindings[1][9]=s.bindings[1][8];
        require(apply_controller_bindings(physical,s.bindings)[Button::b],"Duplicate bindings not supported");
        s.bindings[1][0]=s.bindings[1][1]=s.bindings[1][8];
        const auto opposed=apply_controller_bindings(physical,s.bindings);
        require(!opposed[Button::left_up] && !opposed[Button::left_down],"Opposed mapped directions not cancelled");
        VrInputMapper mapper; std::array<VrHandInput,2> hands{}; mapper.update(hands,true);
        hands[0].grip_active=true; hands[0].grip=.6f; hands[1].a=true;
        auto vr=mapper.update(hands,true);
        require(vr.sources[InputSource::vr_l_grip] && vr.sources[InputSource::vr_r_a] && vr.held,"VR raw controls missing");
        require(apply_controller_bindings(vr.sources,defaults).buttons==vr.game.buttons,"Default VR bindings changed gameplay");
        hands[0].grip=.5f; require(mapper.update(hands,true).sources[InputSource::vr_l_grip],"Grip release hysteresis missing");
        hands[0].grip=.4f; require(!mapper.update(hands,true).sources[InputSource::vr_l_grip],"Grip did not release");
        mapper.update(hands,false); require(!mapper.update(hands,true).sources.any(),"Focus recovery leaked held bindings");
        hands={}; mapper.update(hands,true);
        hands[0].menu_click=hands[1].menu_click=true; vr=mapper.update(hands,true);
        require(vr.menu,"Fixed settings chord no longer works");

        s=PlayerSettings{}; SettingsMenu m; MenuInput toggle; toggle.toggle=true;
        m.update(toggle,s); m.update({},s); m.selected=int(MenuItem::controls); m.update(accept(),s);
        require(m.open && m.mapping_open,"Controller submenu not opened");
        m.update({},s); capture(m,s,0);
        auto held=pressed(InputSource::vr_r_trigger);
        m.update(accept(),s,held); require(m.capture_target==0 && s.bindings[2][0].primary==defaults[2][0].primary,
            "Opening press captured before release");
        m.update({},s);
        for(int target=0;target<14;++target) {
            held=pressed(InputSource::vr_l_trigger);
            m.update(back(),s,held); // Back must be bindable during capture.
            require(s.bindings[2][target].primary==int(InputSource::vr_l_trigger),"Wizard did not bind target");
            const int next=m.capture_target; m.update(back(),s,held);
            require(m.capture_target==next,"Held binding skipped a wizard step");
            require(m.open && m.mapping_open,"Captured Back closed the menu");
            m.update({},s);
        }
        require(m.capture_target<0 && !m.wizard,"Wizard did not complete after 14 inputs");
        require(s.bindings[0][0].primary==defaults[0][0].primary && s.bindings[1][0].primary==defaults[1][0].primary,
            "Wizard overwrote another device family");
        capture(m,s,9); m.update({},s); m.update({},s,pressed(InputSource::pad_y));
        require(s.mapping_view==InputDevice::gamepad && s.bindings[1][8].primary==int(InputSource::pad_y)
            && s.bindings[1][9].primary==defaults[1][9].primary,"Individual capture changed unrelated input");
        capture(m,s,10); m.update({},s);
        held=pressed(InputSource::pad_x); held[InputSource::pad_y]=true; m.update({},s,held);
        m.update({},s,pressed(InputSource::pad_x));
        require(m.capture_target==9,"Ambiguous capture accepted a residual held button");
        m.update({},s); m.suspend_capture(); m.update({},s,pressed(InputSource::pad_x));
        require(m.capture_target==9,"Focus return captured a held button");
        m.update({},s); m.update({},s,pressed(InputSource::pad_x));
        require(m.capture_target<0 && s.bindings[1][9].primary==int(InputSource::pad_x),"Capture did not rearm after release");
        capture(m,s,0); m.update({},s); m.update({},s,pressed(InputSource::vr_r_b)); m.update({},s);
        m.update({},s,pressed(InputSource::pad_x)); require(m.capture_target==1,"Wizard mixed device families");
        m.update({},s); MenuInput skip; skip.skip=true; m.update(skip,s);
        require(m.capture_target==2,"Wizard skip did not advance"); m.update({},s);
        m.update(toggle,s); require(m.capture_target<0 && m.mapping_open && m.open,"Cancel did not return to bindings list");
        require(s.bindings[2][0].primary==int(InputSource::vr_r_b),"Cancel discarded completed wizard steps");
        m.update({},s); m.mapping_selected=15; m.update(accept(),s);
        require(!m.mapping_open && m.open && m.selected==int(MenuItem::controls),"Back did not return to main settings");
        m.update({},s); m.update(accept(),s); m.update({},s); m.mapping_selected=0;
        export_ppm(std::filesystem::path(argv[1])/"controller-mapping.ppm",m.render({},s));
        capture(m,s,9); export_ppm(std::filesystem::path(argv[1])/"controller-capture.ppm",m.render({},s));
        m.update({},s); export_ppm(std::filesystem::path(argv[1])/"controller-ready.ppm",m.render({},s));
        const auto prior=s.bindings[2][8].primary;
        held=pressed(InputSource::vr_l_trigger); held[InputSource::vr_r_trigger]=true;
        m.update({},s,held); require(m.capture_target<0 && s.bindings[2][8].primary==prior,"Trigger skip changed individual binding");
        capture(m,s,9); m.update({},s); held=pressed(InputSource::pad_lb); held[InputSource::pad_rb]=true;
        m.update({},s,held); require(m.capture_target<0,"XInput skip chord did not end capture");
        capture(m,s,0); m.update({},s);
        const char keys[]="ABCDEFGHIJKLNO"; // Unique keyboard controls; P/M remain reserved.
        for(int target=0;target<14;++target) {
            SourceInput key; key.held[static_cast<unsigned char>(keys[target])]=true;
            m.update({},s,key); require(s.bindings[0][target].primary==keys[target],"Keyboard wizard lost input ordering");
            m.update({},s);
        }
        require(m.capture_target<0,"Keyboard wizard did not finish");

        const auto path=std::filesystem::path(argv[1])/("bindings-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ini");
        struct Cleanup {std::filesystem::path path; ~Cleanup(){std::error_code ec;std::filesystem::remove(path,ec);}} cleanup{path};
        save_settings(path,s); require(encode_settings(load_settings(path))==encode_settings(s),"Remappings did not persist");
        {std::ofstream out(path); out<<"bind_vr_a=-1:0\nbind_vr_b=9999:0\nbind_gamepad_a=288:0\n"
            "bind_keyboard_a=77:0\nbind_keyboard_b=80:0\nbind_vr_start=299:0junk\nbind_vr_select=299.0:0\n"
            "bind_gamepad_b=270:0\n";}
        const auto loaded=load_settings(path);
        require(loaded.bindings[2][8].primary==defaults[2][8].primary && loaded.bindings[2][9].primary==defaults[2][9].primary
            && loaded.bindings[1][8].primary==defaults[1][8].primary && loaded.bindings[0][8].primary==defaults[0][8].primary
            && loaded.bindings[0][9].primary==defaults[0][9].primary && loaded.bindings[2][12].primary==defaults[2][12].primary
            && loaded.bindings[2][13].primary==defaults[2][13].primary && loaded.bindings[1][9].primary==270,
            "Invalid mapping IDs, reserved keys or trailing text bypassed validation");
        std::cout<<"Binding capture, wizard, release/focus gates, device isolation, gameplay and persistence passed.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
