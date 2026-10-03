// SPDX-License-Identifier: GPL-2.0-or-later
#include "settings_menu.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <limits>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Temporary {
    std::filesystem::path directory;
    ~Temporary() { std::error_code ignored; std::filesystem::remove_all(directory, ignored); }
};
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected test output directory");
        Temporary temp{std::filesystem::u8path(argv[1]) / ("menu-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
        std::filesystem::create_directories(temp.directory);
        const auto path = temp.directory / "settings.ini";
        auto settings = bvb::load_settings(path);
        require(settings.palette == 0 && settings.zoom == 1 && settings.stick_deadzone == .1f, "Missing settings must use defaults");
        settings.palette = 6; settings.screen.width = 2.3f; settings.screen.distance = 4;
        settings.screen.swap_eyes = true; settings.zoom = .7f; settings.smooth = true;
        settings.volume = .35f; settings.muted = true;
        settings.stick_deadzone = .35f;
        settings.state_slot = 9;
        bvb::save_settings(path, settings);
        require(bvb::encode_settings(bvb::load_settings(path)) == bvb::encode_settings(settings), "Settings round trip lost preferences");
        settings.palette = 3; bvb::save_settings(path, settings);
        require(bvb::load_settings(path).palette == 3, "Existing preferences not atomically replaced");
        for (const auto& file : std::filesystem::directory_iterator(temp.directory))
            require(file.path() == path, "Settings temporary file leaked");
        {
            std::ofstream out(path);
            out << "palette=123\nwidth=nan\ndistance=2.7\nvolume=-1\nswap_eyes=2\nzoom=1.5junk\nsmooth=1\nunknown=9\n"
                "vr_stick_deadzone=-1\nvr_stick_deadzone=0.8\nvr_stick_deadzone=nan\nvr_stick_deadzone=0.2junk\nstate_slot=10\nstate_slot=-1\nstate_slot=1.5\n";
        }
        settings = bvb::load_settings(path);
        require(settings.palette == 0 && settings.screen.width == 1.2f && settings.volume == 1
            && !settings.screen.swap_eyes && settings.zoom == 1 && settings.smooth
            && settings.stick_deadzone == .1f && settings.state_slot == 0 && std::abs(settings.screen.distance-2.7f)<.001f,
            "Malformed preferences bypassed validation or lost valid keys");
        settings.stick_deadzone = std::numeric_limits<float>::quiet_NaN(); settings.clamp();
        require(settings.stick_deadzone == .1f, "Non-finite deadzone did not recover default");
        bvb::SettingsMenu menu;
        bvb::MenuInput input; input.toggle = true;
        menu.update(input, settings); require(menu.open && menu.blocks_game(), "Opening must block gameplay");
        menu.update(input, settings); require(menu.open, "Held toggle repeatedly opens and closes menu");
        menu.update({}, settings); input = {}; input.down = true;
        menu.update(input, settings); menu.update(input, settings);
        require(menu.selected == 1, "Held navigation skipped rows");
        menu.update({}, settings); input = {}; input.left = true;
        menu.update(input, settings); require(settings.palette == 7, "Palette decrement did not wrap");
        menu.update({}, settings); input = {}; input.right = true;
        menu.update(input, settings); require(settings.palette == 0, "Palette increment did not wrap");
        menu.update({}, settings); input = {}; input.down = input.right = true;
        const auto before_navigation = bvb::encode_settings(settings);
        menu.update(input, settings);
        require(menu.selected == 2 && bvb::encode_settings(settings) == before_navigation,
            "Diagonal menu navigation changed a setting while moving rows");
        menu.update({}, settings); input = {}; input.down = true; menu.update(input, settings);
        input.right = true; menu.update(input, settings);
        require(bvb::encode_settings(settings) == before_navigation, "Sideways drift during held vertical input changed a setting");
        menu.selected = 2; settings.screen.width = 3;
        input = {}; input.right = true;
        menu.update({}, settings); menu.update(input, settings);
        require(settings.screen.width == 3, "Menu width exceeds safe bounds");
        menu.selected = static_cast<int>(bvb::MenuItem::stick_deadzone);
        settings.stick_deadzone = .1f;
        menu.update({}, settings); menu.update(input, settings);
        require(std::abs(settings.stick_deadzone-.15f)<.001f, "Menu did not increase deadzone in 5% steps");
        input = {}; input.left = true; menu.update({}, settings); menu.update(input, settings);
        require(settings.stick_deadzone == .1f, "Menu did not decrease deadzone");
        settings.stick_deadzone = .05f; menu.update({}, settings); menu.update(input, settings);
        require(settings.stick_deadzone == .05f, "Deadzone minimum not enforced");
        settings.stick_deadzone = .6f; input = {}; input.right = true; menu.update({}, settings); menu.update(input, settings);
        require(settings.stick_deadzone == .6f, "Deadzone maximum not enforced");
        menu.selected = 0; input = {}; input.up = true; menu.update({}, settings); menu.update(input, settings);
        require(menu.selected == bvb::menu_row_count-1, "Up navigation failed to wrap through new row");
        input = {}; input.down = true; menu.update({}, settings); menu.update(input, settings);
        require(menu.selected == 0, "Down navigation failed to wrap through new row");
        menu.selected = static_cast<int>(bvb::MenuItem::state_slot);
        input={}; input.left=true; menu.update({},settings); menu.update(input,settings);
        require(settings.state_slot==9,"State slot decrement did not wrap");
        input={}; input.right=true; menu.update({},settings); menu.update(input,settings);
        require(settings.state_slot==0,"State slot increment did not wrap");
        menu.selected=static_cast<int>(bvb::MenuItem::save_state);
        menu.update({},settings); require(!menu.update(input,settings).save_state,"Horizontal input saved state");
        input={}; input.accept=true; menu.update({},settings);
        require(menu.update(input,settings).save_state && menu.open,"Save action missing or closed menu");
        require(!menu.update(input,settings).save_state,"Held accept repeated save");
        menu.selected=static_cast<int>(bvb::MenuItem::load_state); menu.update({},settings);
        require(menu.update(input,settings).load_state && menu.open,"Load action missing or resumed game");
        require(!menu.update(input,settings).load_state,"Held accept repeated load");
        menu.selected=static_cast<int>(bvb::MenuItem::library); menu.update({},settings);
        require(menu.update(input,settings).library && menu.open,"Return to library action missing");
        require(!menu.update(input,settings).library,"Held accept repeated library action");
        menu.selected = static_cast<int>(bvb::MenuItem::recenter); input = {}; input.accept = true;
        menu.update({}, settings); require(menu.update(input, settings).recenter, "Recenter action missing");
        require(!menu.update(input, settings).recenter, "Held accept repeats recenter");
        menu.selected = static_cast<int>(bvb::MenuItem::exit); menu.update({}, settings);
        require(menu.update(input, settings).exit, "Exit action missing");
        menu.selected = 0; menu.update({}, settings);
        require(menu.update(input, settings).resume, "Resume action missing");
        require(!menu.open && menu.blocks_game(), "Closing must wait for button release");
        menu.update(input, settings); require(menu.blocks_game(), "Held accept leaked into gameplay");
        menu.update({}, settings); require(!menu.blocks_game(), "Neutral input must release gameplay gate");

        const auto original = bvb::make_test_pattern();
        menu.open = true;
        auto rendered = menu.render(original, settings);
        require(rendered.left == rendered.right && rendered.left.size() == original.left.size(), "Menu has stereo disparity or wrong size");
        require(original.left != original.right, "Menu destroyed original stereo frame");
        const auto first = rendered.left; menu.selected = 4;
        require(menu.render(original, settings).left != first, "Selection is not visibly highlighted");
        menu.status="SAVED SLOT 1"; menu.slot_occupied=true;
        require(menu.render(original,settings).left != rendered.left,"State feedback not rendered");
        for (std::size_t i=3; i<rendered.left.size(); i+=4) require(rendered.left[i]==255, "Menu text plane is transparent");
        menu.open = false; require(menu.render(original, settings).left == original.left, "Closing did not restore game image");
        settings.zoom = 1;
        require(bvb::prepare_game_image(original, settings).left == original.left, "Default zoom alters native pixels");
        settings.zoom = .5f; auto smaller = bvb::prepare_game_image(original, settings);
        require(smaller.left[0]==0 && smaller.left[1]==0 && smaller.left[2]==0 && smaller.left[3]==255, "Reduced zoom lacks opaque black border");
        settings.zoom = 1.3f; settings.smooth = false; auto crisp = bvb::prepare_game_image(original, settings);
        settings.smooth = true; auto smooth = bvb::prepare_game_image(original, settings);
        require(crisp.left != smooth.left && smooth.left != smooth.right, "Zoom filter failed or merged stereo eyes");
        std::cout << "Preferences, validation, input edges, release gate, menu stereo plane and zoom filtering passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
