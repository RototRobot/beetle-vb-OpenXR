// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "presentation.h"
#include "vr_input.h"
#include <filesystem>
#include <string>

namespace bvb {
inline constexpr const char* palette_names[] = {
    "RED", "WHITE", "BLUE", "CYAN", "ELECTRIC CYAN", "GREEN", "MAGENTA", "YELLOW"};
inline constexpr const char* palette_options[] = {
    "black & red", "black & white", "black & blue", "black & cyan",
    "black & electric cyan", "black & green", "black & magenta", "black & yellow"};
struct PlayerSettings {
    ScreenSettings screen;
    int palette = 0;
    int state_slot = 0;
    float zoom = 1, volume = 1;
    float stick_deadzone = vr_default_deadzone;
    bool smooth = false, muted = false;
    ControllerBindings bindings = default_controller_bindings();
    InputDevice mapping_view = InputDevice::vr;
    void clamp();
};
PlayerSettings load_settings(const std::filesystem::path& path);
std::string encode_settings(const PlayerSettings& settings);
void save_settings(const std::filesystem::path& path, const PlayerSettings& settings);
struct MenuInput {
    bool toggle = false, up = false, down = false, left = false, right = false;
    bool accept = false, back = false;
    bool skip = false; // F4; controller skip chords are read from physical sources.
    bool any() const;
};
struct MenuActions { bool changed = false, resume = false, recenter = false, exit = false, save_state = false, load_state = false, library = false; };
enum class MenuItem { resume, palette, width, distance, zoom, filter, stick_deadzone, swap_eyes, recenter, volume, mute, state_slot, save_state, load_state, controls, library, exit, count };
inline constexpr int menu_row_count = static_cast<int>(MenuItem::count);
class SettingsMenu {
public:
    bool open = false;
    int selected = 0;
    bool slot_occupied = false;
    std::string status = "SELECT SLOT THEN SAVE OR LOAD";
    // Button edges only. Closing suppresses gameplay until menu buttons are released.
    MenuActions update(const MenuInput& input, PlayerSettings& settings, const SourceInput& sources = {});
    void suspend_capture();
    bool mapping_open = false;
    int mapping_selected = 0;
    int capture_target = -1;
    bool wizard = false;
    std::string mapping_status = "SELECT WIZARD OR AN INPUT";
    std::uint64_t revision = 0;
    bool blocks_game() const { return open || release_gate; }
    StereoFrame render(const StereoFrame& game, const PlayerSettings& settings) const;
private:
    MenuInput previous;
    bool release_gate = false;
    bool capture_ready = false;
    int wizard_device = -1;
    void update_mapping(const MenuInput& input, PlayerSettings& settings, const SourceInput& sources, MenuActions& actions);
};
StereoFrame prepare_game_image(const StereoFrame& frame, const PlayerSettings& settings);
}
