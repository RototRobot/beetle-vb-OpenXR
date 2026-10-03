// SPDX-License-Identifier: GPL-2.0-or-later
#include "vr_input.h"
#include "settings_menu.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <regex>
#include <set>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string attribute(const std::string& tag, const char* name) {
    std::smatch found;
    if (!std::regex_search(tag, found, std::regex(std::string(name) + "=\"([^\"]*)\""))) return {};
    return found[1];
}
void validate_registry(const char* path) {
    std::ifstream input(path); require(bool(input), "Cannot read SDK interaction-profile registry");
    const std::string registry((std::istreambuf_iterator<char>(input)), {});
    for (const auto& profile : bvb::vr_profiles()) {
        auto start = registry.find(std::string("<interaction_profile name=\"") + profile.path + "\"");
        require(start != std::string::npos, "Suggested profile missing from Khronos registry");
        auto end = registry.find("</interaction_profile>", start);
        require(end != std::string::npos, "Malformed registry profile");
        const auto xml = registry.substr(start, end-start);
        std::set<std::string> seen;
        for (const auto& binding : profile.bindings) {
            require(seen.insert(binding.component).second, "Physical component assigned to two actions");
            const std::string full(binding.component);
            auto split = full.find("/input/"); require(split != std::string::npos, "Binding has no input component");
            const auto user = full.substr(0, split), component = full.substr(split);
            require(xml.find("<user_path path=\"" + user + "\"") != std::string::npos, "Binding uses unsupported hand");
            bool valid = false;
            const std::regex tags("<component[^>]*>");
            for (auto it = std::sregex_iterator(xml.begin(), xml.end(), tags); it != std::sregex_iterator(); ++it) {
                const auto tag = it->str();
                if (attribute(tag,"subpath") != component) continue;
                const auto hand = attribute(tag,"user_path"); if (!hand.empty() && hand != user) continue;
                const auto kind = bvb::vr_actions[static_cast<std::size_t>(binding.action)].type;
                const char* expected = kind == bvb::VrActionType::vector2 ? "XR_ACTION_TYPE_VECTOR2F_INPUT"
                    : kind == bvb::VrActionType::scalar ? "XR_ACTION_TYPE_FLOAT_INPUT" : "XR_ACTION_TYPE_BOOLEAN_INPUT";
                require(attribute(tag,"type") == expected, "Action type differs from physical component type");
                require(attribute(tag,"system") != "true", "Binding tries to use reserved system button");
                valid = true;
            }
            require(valid, "Suggested binding not allowed by Khronos registry");
        }
    }
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2) validate_registry(argv[1]);
        using B = bvb::Button;
        bvb::VrInputMapper mapper;
        std::array<bvb::VrHandInput, 2> hands{};
        hands[1].a = true;
        require(!mapper.update(hands,true).held, "Held startup input was accepted before neutral");
        hands = {}; mapper.update(hands,true);
        hands[0].axis_active = hands[1].axis_active = true;
        hands[0].x = -1; hands[0].y = 1; hands[1].x = 1; hands[1].y = -1;
        auto mapped = mapper.update(hands,true);
        require(mapped.game[B::left_left] && mapped.game[B::left_up] && mapped.game[B::right_right]
            && mapped.game[B::right_down], "Dual pads / diagonal directions mapped incorrectly");
        hands[0].x = -.085f; hands[0].y = .085f;
        require(mapper.update(hands,true).game[B::left_up], "Axis hysteresis did not hold above release threshold");
        hands[0].x = -.07f; hands[0].y = .07f;
        require(!mapper.update(hands,true).game[B::left_up], "Axis failed to return neutral");
        hands[0].x = std::numeric_limits<float>::quiet_NaN(); hands[0].y = std::numeric_limits<float>::infinity();
        mapped = mapper.update(hands,true);
        require(!mapped.game[B::left_up] && !mapped.game[B::left_left], "Non-finite axis caused button presses");
        hands[1].axis_contact = false;
        mapped = mapper.update(hands,true);
        require(!mapped.game[B::right_right] && !mapped.game[B::right_down], "Untouched trackpad retains old directions");
        hands = {}; hands[0].trigger_active = hands[1].trigger_active = true;
        hands[0].trigger = hands[1].trigger = .56f;
        mapped = mapper.update(hands,true);
        require(mapped.game[B::l] && mapped.game[B::r] && mapped.accept && mapped.back, "Trigger shoulders/menu mapping failed");
        hands[1].trigger = .5f; require(mapper.update(hands,true).game[B::r], "Trigger jitter changed pressed state");
        hands[1].trigger = .44f; require(!mapper.update(hands,true).game[B::r], "Trigger release ignored");
        hands[0].trigger_active = false; require(!mapper.update(hands,true).game[B::l], "Inactive trigger retained pressure");
        hands = {}; hands[1].a = hands[1].b = hands[0].start = hands[0].select = true;
        mapped = mapper.update(hands,true);
        require(mapped.game[B::a] && mapped.game[B::b] && mapped.game[B::start] && mapped.game[B::select], "Face buttons lost VB semantics");
        require(!mapper.update(hands,false).held, "Unfocused controller input not neutral");
        require(!mapper.update(hands,true).held, "Focus recovery accepted controls before release");
        hands = {}; mapper.update(hands,true);
        hands[0].menu_click = true;
        require(!mapper.update(hands,true).menu, "One stick click opens menu");
        hands[1].menu_click = true; mapped = mapper.update(hands,true);
        require(mapped.menu && bvb::libretro_buttons(mapped.game)==0, "Settings chord leaked into gameplay");
        bvb::SettingsMenu menu; bvb::PlayerSettings settings;
        bvb::MenuInput navigation; navigation.toggle = mapped.menu;
        menu.update(navigation,settings); menu.update(navigation,settings);
        require(menu.open, "Held VR chord repeatedly toggles menu");
        menu.update({},settings);
        navigation = {}; navigation.accept = true; menu.update(navigation,settings);
        require(!menu.open && menu.blocks_game(), "VR select resume lacks release gate");
        menu.update({},settings); require(!menu.blocks_game(), "Menu did not resume after VR control release");
        mapper.reset(); require(!mapper.update(hands,true).menu, "Profile change retained a held menu chord");
        hands = {}; mapper.update(hands,true);
        hands[0].axis_active = true; hands[0].x = .09f;
        require(!mapper.update(hands,true).held, "10% deadzone accepted near-centre drift");
        hands[0].x = .35f; hands[0].y = .8f;
        mapped = mapper.update(hands,true);
        require(mapped.game[B::left_up] && mapped.game[B::left_right] && mapped.menu_directions[B::left_up]
            && !mapped.menu_directions[B::left_right], "Up gesture with sideways tilt changed menu axis or lost gameplay diagonal");
        hands[0].x = .9f; hands[0].y = .5f; mapped = mapper.update(hands,true);
        require(mapped.menu_directions[B::left_up] && !mapped.menu_directions[B::left_right], "Menu gesture switched axis before recentering");
        hands[0].y = 0; mapped = mapper.update(hands,true);
        require(bvb::libretro_buttons(mapped.menu_directions)==0, "A rolled stick gesture generated a horizontal menu action");
        hands[0].x = 0; mapper.update(hands,true);
        hands[0].x = .8f; hands[0].y = .8f; mapped = mapper.update(hands,true);
        require(bvb::libretro_buttons(mapped.menu_directions)==0, "Ambiguous diagonal selected a menu direction");
        hands[0].x = .2f; mapped = mapper.update(hands,true);
        require(mapped.menu_directions[B::left_up], "Clear vertical intent after ambiguous diagonal was ignored");
        hands[0].x = hands[0].y = 0; mapper.update(hands,true);
        hands[0].x = .8f; hands[0].y = .3f; mapped = mapper.update(hands,true);
        require(mapped.menu_directions[B::left_right] && !mapped.menu_directions[B::left_up], "Deliberate horizontal gesture was ignored");
        hands[0].x = hands[0].y = 0; mapper.update(hands,true,.4f);
        hands[0].x = .39f; require(!mapper.update(hands,true,.4f).held, "Custom deadzone was ignored");
        hands[0].x = .45f; require(mapper.update(hands,true,.4f).game[B::left_right], "Custom deadzone did not allow movement above threshold");
        hands[0].x = .35f; require(mapper.update(hands,true,.4f).game[B::left_right], "Custom deadzone lost hysteresis");
        hands[0].x = .29f; require(!mapper.update(hands,true,.4f).held, "Custom deadzone did not release below threshold");
        std::cout << "VR dual pads, hysteresis, triggers, focus recovery, menu release and registry bindings passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
