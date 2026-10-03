// SPDX-License-Identifier: GPL-2.0-or-later
#include "audio_output.h"
#include "core_host.h"
#include "desktop_window.h"
#include "frame_clock.h"
#include "openxr_session.h"
#include "settings_menu.h"
#include "state_file.h"
#include "library.h"
#include <fstream>
#include <algorithm>
#include <windows.h>
#include <xinput.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;
struct Options : bvb::PlayerSettings {
    fs::path rom, core, saves, states, settings_file, export_menu, library_file, export_library, rom_directory, art_directory;
    double seconds = 0;
    bool start_menu = false, no_vr_input = false;
    bool desktop = false, headless = false, synthetic = false, no_audio = false, help = false;
};
double number(const wchar_t* text) {
    std::size_t consumed = 0;
    const std::wstring value(text);
    double result = std::stod(value, &consumed);
    if (!std::isfinite(result) || consumed != value.size()) throw std::invalid_argument("Expected a finite number");
    return result;
}
fs::path executable_directory() {
    std::wstring path(32768, L'\0');
    auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size >= path.size()) throw std::runtime_error("Cannot determine executable directory");
    path.resize(size);
    return fs::path(path).parent_path();
}
Options parse(int argc, wchar_t** argv) {
    Options result;
    auto directory = executable_directory();
    result.core = directory / "mednafen_vb_libretro.dll";
    result.saves = directory / "saves";
    result.states = directory / "states";
    result.settings_file = directory / "settings.ini";
    bool diagnostic = false;
    for (int i = 1; i < argc; ++i) {
        const std::wstring flag(argv[i]);
        if (flag == L"--headless" || flag == L"--help" || flag == L"-h" || flag == L"--export-menu" || flag == L"--export-library") diagnostic = true;
        if (flag == L"--settings-file" && i + 1 < argc) result.settings_file = argv[++i];
    }
    if (!diagnostic) {
        try { static_cast<bvb::PlayerSettings&>(result) = bvb::load_settings(result.settings_file); }
        catch (const std::exception& error) { std::cerr << "Settings: " << error.what() << "; using defaults.\n"; }
    }
    for (int i = 1; i < argc; ++i) {
        std::wstring flag(argv[i]);
        if (flag == L"--help" || flag == L"-h") result.help = true;
        else if (flag == L"--desktop") result.desktop = true;
        else if (flag == L"--headless") result.headless = result.desktop = result.no_audio = true;
        else if (flag == L"--synthetic") result.synthetic = true;
        else if (flag == L"--start-menu") result.start_menu = true;
        else if (flag == L"--no-vr-input") result.no_vr_input = true;
        else if (flag == L"--no-audio") result.no_audio = true;
        else if (flag == L"--swap-eyes") result.screen.swap_eyes = true;
        else if (flag == L"--core" || flag == L"--save-dir" || flag == L"--state-dir" || flag == L"--width" || flag == L"--distance" || flag == L"--seconds" || flag == L"--volume" || flag == L"--settings-file" || flag == L"--export-menu" || flag == L"--export-library" || flag == L"--rom-dir" || flag == L"--boxart-dir") {
            if (++i >= argc) throw std::invalid_argument("Missing option value");
            if (flag == L"--core") result.core = argv[i];
            else if (flag == L"--save-dir") result.saves = argv[i];
            else if (flag == L"--state-dir") result.states = argv[i];
            else if (flag == L"--settings-file") result.settings_file = argv[i];
            else if (flag == L"--export-menu") result.export_menu = argv[i];
            else if (flag == L"--export-library") result.export_library = argv[i];
            else if (flag == L"--rom-dir") result.rom_directory = argv[i];
            else if (flag == L"--boxart-dir") result.art_directory = argv[i];
            else {
                double value = number(argv[i]);
                if (flag == L"--width") {
                    if (value < 0.3 || value > 3) throw std::invalid_argument("Width must be 0.3 to 3 metres");
                    result.screen.width = static_cast<float>(value);
                } else if (flag == L"--distance") {
                    if (value < 0.5 || value > 5) throw std::invalid_argument("Distance must be 0.5 to 5 metres");
                    result.screen.distance = static_cast<float>(value);
                } else if (flag == L"--seconds") {
                    if (value < 0) throw std::invalid_argument("Seconds must be nonnegative");
                    result.seconds = value;
                } else {
                    if (value < 0 || value > 1) throw std::invalid_argument("Volume must be 0 to 1");
                    result.volume = static_cast<float>(value);
                }
            }
        } else if (!flag.empty() && flag[0] == L'-') throw std::invalid_argument("Unknown launch option");
        else {
            if (!result.rom.empty()) throw std::invalid_argument("Pass only one ROM path");
            result.rom = argv[i];
        }
    }
    if (result.synthetic && !result.rom.empty()) throw std::invalid_argument("Choose a ROM or --synthetic, not both");
    if (result.headless && (!result.seconds || (!result.synthetic && result.rom.empty())) && !result.help)
        throw std::invalid_argument("Headless mode requires a ROM or --synthetic and --seconds greater than zero");
    result.library_file = result.settings_file.parent_path() / "library.ini";
    return result;
}
void help() {
    std::cout << "Beetle VB OpenXR player\n"
        "Usage: beetle_vb_openxr [game.vb] [--desktop] [--no-audio] [--volume 0..1]\n"
        "                       [--width metres] [--distance metres] [--swap-eyes]\n"
        "                       [--core library.dll] [--save-dir folder] [--seconds duration]\n"
        "No ROM argument opens the in-headset game library. --desktop plays without OpenXR.\n"
        "Keyboard: WASD left pad, arrows right pad, J/K B/A, Q/E L/R, Enter/Space Start/Select.\n"
        "XInput gamepad: D-pad/left stick left pad, right stick right pad, A/B, LB/RB, Start/Back.\n"
        "Window: F1 recenter, F2 swap eyes, P pause, M mute, +/- width, [/] distance, F3/Esc settings menu.\n"
        "Menu: Back+Start on gamepad, D-pad/arrows move, left/right adjust, A/Enter select, B/J close.\n"
        "VR controllers: two sticks/pads, triggers L/R; both stick/pad clicks toggle settings.\n"
        "VR menu: pad directions move/adjust, right trigger select, left trigger close.\n"
        "Controller Mapping: wizard or individual bindings in settings; F4 skips a capture step.\n"
        "Save states: select slot 1..10 in settings, then Save State / Load State. --state-dir folder overrides storage.\n"
        "--no-vr-input disables OpenXR controller actions while retaining keyboard/XInput.\n"
        "Library: --rom-dir folder / --boxart-dir folder override remembered folders.\n"
        "Preferences are saved beside the player. --settings-file path overrides their location.\n"
        "Diagnostics: --start-menu opens settings immediately; --export-menu file.bmp exports the menu.\n"
        "Diagnostics: --synthetic supplies a blank cartridge; --headless --seconds N tests core/timing silently.\n";
}
fs::path suggested_folder(const wchar_t* name) {
    auto parent=executable_directory();
    for(int i=0;i<6;++i) {
        const auto candidate=parent/name; std::error_code error;
        if(fs::is_directory(candidate,error)) return candidate;
        const auto next=parent.parent_path(); if(next==parent) break; parent=next;
    }
    return {};
}
bool down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }
bvb::GameInput keyboard(bool foreground) {
    bvb::GameInput input;
    if (!foreground) return input;
    using B = bvb::Button;
    input[B::left_up] = down('W'); input[B::left_down] = down('S');
    input[B::left_left] = down('A'); input[B::left_right] = down('D');
    input[B::right_up] = down(VK_UP); input[B::right_down] = down(VK_DOWN);
    input[B::right_left] = down(VK_LEFT); input[B::right_right] = down(VK_RIGHT);
    input[B::a] = down('K'); input[B::b] = down('J');
    input[B::l] = down('Q'); input[B::r] = down('E');
    input[B::start] = down(VK_RETURN); input[B::select] = down(VK_SPACE);
    return input;
}
bvb::SourceInput keyboard_sources(bool foreground) {
    bvb::SourceInput sources;
    if(foreground) for(int key=1;key<256;++key) if(bvb::bindable_key(key)) sources.held[key]=down(key);
    return sources;
}
class Gamepad {
    DWORD index = XUSER_MAX_COUNT;
    std::chrono::steady_clock::time_point next_scan{};
public:
    bvb::SourceInput sources;
    bvb::GameInput poll() {
        sources={};
        XINPUT_STATE state{};
        if (index == XUSER_MAX_COUNT || XInputGetState(index, &state) != ERROR_SUCCESS) {
            index = XUSER_MAX_COUNT;
            auto now = std::chrono::steady_clock::now();
            if (now < next_scan) return {};
            next_scan = now + std::chrono::seconds(1);
            for (DWORD candidate = 0; candidate < XUSER_MAX_COUNT; ++candidate)
                if (XInputGetState(candidate, &state) == ERROR_SUCCESS) { index = candidate; break; }
            if (index == XUSER_MAX_COUNT) return {};
        }
        const auto& pad = state.Gamepad;
        bvb::GamepadState mapped;
        mapped.left_x = pad.sThumbLX; mapped.left_y = pad.sThumbLY;
        mapped.right_x = pad.sThumbRX; mapped.right_y = pad.sThumbRY;
        mapped.up = (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
        mapped.down = (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
        mapped.left = (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
        mapped.right = (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
        mapped.a = (pad.wButtons & XINPUT_GAMEPAD_A) != 0; mapped.b = (pad.wButtons & XINPUT_GAMEPAD_B) != 0;
        mapped.l = (pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0; mapped.r = (pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
        mapped.start = (pad.wButtons & XINPUT_GAMEPAD_START) != 0; mapped.select = (pad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
        mapped.x = (pad.wButtons & XINPUT_GAMEPAD_X) != 0; mapped.y = (pad.wButtons & XINPUT_GAMEPAD_Y) != 0;
        mapped.left_click = (pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
        mapped.right_click = (pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
        mapped.left_trigger=pad.bLeftTrigger; mapped.right_trigger=pad.bRightTrigger;
        sources=bvb::gamepad_sources(mapped);
        return bvb::map_gamepad(mapped);
    }
};
void export_frame(const fs::path& path, const bvb::StereoFrame& frame) {
    const auto pixels = bvb::rgba_to_bgra(frame.left);
    BITMAPFILEHEADER file{}; BITMAPINFOHEADER image{};
    file.bfType = 0x4D42; file.bfOffBits = sizeof(file) + sizeof(image);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
    image.biSize = sizeof(image); image.biWidth = frame.width; image.biHeight = -int(frame.height);
    image.biPlanes = 1; image.biBitCount = 32; image.biCompression = BI_RGB;
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(&file), sizeof(file));
    out.write(reinterpret_cast<const char*>(&image), sizeof(image));
    out.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    if (!out) throw std::runtime_error("Cannot export UI image");
}
volatile LONG interrupted = 0;
BOOL WINAPI console_control(DWORD signal) {
    if (signal != CTRL_C_EVENT && signal != CTRL_BREAK_EVENT) return FALSE;
    InterlockedExchange(&interrupted, 1); return TRUE;
}
}

int wmain(int argc, wchar_t** argv) {
    try {
        auto options = parse(argc, argv);
        if (options.help) { help(); return 0; }
        if (!options.export_menu.empty()) {
            bvb::SettingsMenu exported; exported.open=true;
            export_frame(options.export_menu,exported.render(bvb::make_test_pattern(),options)); return 0;
        }
        bvb::LibraryConfig config;
        if(!options.headless && options.export_library.empty()) {
            try {config=bvb::load_library_config(options.library_file);}
            catch(const std::exception& e) {std::cerr<<"Library preferences: "<<e.what()<<'\n';}
        }
        if(!options.rom_directory.empty()) config.rom_directory=fs::absolute(options.rom_directory);
        if(!options.art_directory.empty()) config.art_directory=fs::absolute(options.art_directory);
        if(config.art_directory.empty() && !options.headless) config.art_directory=suggested_folder(L"Boxart");
        bvb::LibraryMenu library;
        auto suggestion=options.headless?fs::path{}:suggested_folder(L"Roms");
        if(suggestion.empty() && !options.headless) suggestion=fs::current_path();
        library.initialize(config,suggestion,options.library_file.parent_path()/"game-data");
        if(!options.export_library.empty()) {
            library.toolbar=library.games.empty(); export_frame(options.export_library,library.render()); return 0;
        }
        std::unique_ptr<bvb::CoreHost> core;
        std::unique_ptr<bvb::AudioOutput> audio;
        bvb::SettingsMenu menu;
        auto use_audio = [&](auto operation) {
            if (!audio) return;
            try { operation(*audio); } catch (const std::exception& error) {
                std::cerr << "Audio disabled: " << error.what() << '\n'; audio.reset();
            }
        };
        std::string cartridge;
        auto slot_path = [&] { return bvb::state_slot_path(options.states,cartridge,options.state_slot); };
        auto refresh_slot = [&] {
            if(!core) {menu.slot_occupied=false; return;}
            std::error_code error;
            menu.slot_occupied = fs::exists(slot_path(),error);
            if (error) { menu.status = "STATE FOLDER UNAVAILABLE"; std::cerr << "State folder: " << error.message() << '\n'; }
        };
        auto start_game = [&](const fs::path& rom,bool synthetic) {
            if(core) throw std::logic_error("Return to library before changing games");
            auto candidate=std::make_unique<bvb::CoreHost>();
            candidate->initialize(options.core);
            candidate->set_palette(bvb::palette_options[options.palette]);
            if(synthetic) candidate->load_synthetic(); else candidate->load_game(rom,options.saves);
            candidate->run_frame();
            if(candidate->frame().left.empty()) throw std::runtime_error("Core provided no initial stereo image");
            std::cout << candidate->name() << ", " << candidate->fps() << " fps, " << candidate->sample_rate() << " Hz\n";
            if(!candidate->save_path().empty()) std::cout<<"Save RAM: "<<candidate->save_path().u8string()<<'\n';
            if(!options.no_audio) {
                try {
                    audio=std::make_unique<bvb::AudioOutput>(); audio->initialize(candidate->sample_rate(),options.volume);
                    std::cout<<"Audio: Windows default output (XAudio2).\n";
                } catch(const std::exception& e) {audio.reset(); std::cerr<<"Audio unavailable: "<<e.what()<<'\n';}
            }
            cartridge=candidate->cartridge_key(); core=std::move(candidate); options.rom=rom;
            menu=bvb::SettingsMenu{}; menu.open=options.start_menu; refresh_slot();
        };
        if(options.synthetic || !options.rom.empty()) start_game(options.rom,options.synthetic);
        bool in_library=!core;
        bvb::StereoFrame display_frame=in_library?library.render():menu.render(bvb::prepare_game_image(core->frame(),options),options);
        std::unique_ptr<bvb::DesktopWindow> window;
        if (!options.headless) window = std::make_unique<bvb::DesktopWindow>(display_frame, options.screen, true,
            L"Beetle VB OpenXR - Game library and player");
        if (window) window->muted = options.muted;
        bvb::OpenXrSession xr;
        if (!options.desktop) xr.initialize(!options.no_vr_input,bvb::library_scale);
        help();
        SetConsoleCtrlHandler(console_control, TRUE);
        struct Cleanup { ~Cleanup() { SetConsoleCtrlHandler(console_control, FALSE); } } cleanup;
        bvb::FrameClock clock(core?core->fps():50.27);
        Gamepad pad;
        const auto started = std::chrono::steady_clock::now();
        auto previous = started, last_save = started, exit_time = started;
        bool exiting = false, menu_exit = false, suppress_input = true;
        std::uint64_t display_version = 0, last_core_frame = core?core->frame_count():0;
        std::string displayed_settings = bvb::encode_settings(options);
        std::string saved_settings = displayed_settings;
        bool displayed_menu = menu.open, was_focused=true;
        std::uint64_t displayed_menu_revision=menu.revision;
        int displayed_selection = menu.selected;
        int current_slot = options.state_slot;
        std::string displayed_status = menu.status;
        auto settings_changed_at = started;
        auto persist = [&] {
            if (options.headless) return;
            const auto current = bvb::encode_settings(options);
            if (current == saved_settings) return;
            try { bvb::save_settings(options.settings_file, options); saved_settings = current; }
            catch (const std::exception& error) { std::cerr << "Could not save settings: " << error.what() << '\n'; }
        };
        auto persist_library = [&] {
            if(options.headless) return;
            try {bvb::save_library_config(options.library_file,library.config);}
            catch(const std::exception& e) {library.status="FOLDER NOT SAVED - SEE CONSOLE"; ++library.revision; std::cerr<<e.what()<<'\n';}
        };
        std::uint64_t total_frames=0, displayed_library_revision=library.revision;
        for (;;) {
            if (window) window->pump();
            if (!options.desktop) { xr.poll_events(); if (xr.finished()) break; }
            const auto now = std::chrono::steady_clock::now();
            const bool timed_out = options.seconds > 0 && std::chrono::duration<double>(now - started).count() >= options.seconds;
            const bool quit = menu_exit || (window && window->close_requested) || InterlockedCompareExchange(&interrupted, 0, 0) || timed_out;
            if (quit && !exiting) {
                if (options.desktop) break;
                exiting = true; exit_time = now; xr.request_exit();
            }
            if (!options.desktop && (xr.finished() || (exiting && now - exit_time > std::chrono::seconds(5)))) break;
            if (window && window->recenter_requested) {
                if (!options.desktop) xr.recenter();
                window->recenter_requested = false;
            }
            const bool focused = !exiting && (options.headless || (options.desktop ? window->foreground() : xr.focused()));
            const auto vr = options.desktop ? bvb::VrInput{} : xr.poll_input(options.stick_deadzone);
            if(!focused && was_focused) {menu.suspend_capture(); suppress_input=true;}
            was_focused=focused;
            bvb::GameInput raw;
            bvb::SourceInput sources;
            bvb::MenuInput navigation;
            if (focused && !options.headless) {
                const auto desktop_input = bvb::combine_inputs(keyboard(window->foreground()), pad.poll());
                const auto default_input = bvb::combine_inputs(desktop_input, vr.game);
                sources=bvb::combine_sources(bvb::combine_sources(keyboard_sources(window->foreground()),pad.sources),vr.sources);
                raw=bvb::apply_controller_bindings(sources,options.bindings);
                const auto menu_input = bvb::combine_inputs(desktop_input, vr.menu_directions);
                using B = bvb::Button;
                navigation.toggle = (default_input[B::start] && default_input[B::select]) || vr.menu;
                navigation.up = menu_input[B::left_up] || menu_input[B::right_up];
                navigation.down = menu_input[B::left_down] || menu_input[B::right_down];
                navigation.left = menu_input[B::left_left] || menu_input[B::right_left];
                navigation.right = menu_input[B::left_right] || menu_input[B::right_right];
                navigation.accept = default_input[B::a] || vr.accept || (window->foreground() && down(VK_RETURN));
                navigation.back = default_input[B::b] || vr.back;
                navigation.skip=window->foreground() && down(VK_F4);
                if (window->menu_requested) navigation.toggle = true;
            }
            if (window) {
                window->menu_requested = false;
                options.muted = window->muted;
            }
            const bool menu_was_open = menu.open;
            bvb::MenuActions actions;
            bool state_loaded = false, scene_changed = false;
            if(in_library) {
                library.poll_data_scan();
                if(focused) {
                    const auto request=library.update(navigation);
                    if(request.config_changed) persist_library();
                    if(request.exit) menu_exit=true;
                    if(!request.launch.empty()) {
                        try {
                            start_game(request.launch,false); in_library=false; suppress_input=true;
                            clock=bvb::FrameClock(core->fps()); previous=last_save=now;
                            if(window) window->paused=false;
                            scene_changed=true;
                        } catch(const std::exception& e) {
                            audio.reset(); library.status="GAME COULD NOT LOAD - SEE CONSOLE"; ++library.revision;
                            std::cerr<<"Game load: "<<e.what()<<'\n';
                        }
                    }
                }
            } else {
                // While unfocused preserve button history and the release gate.
                if(focused) actions=menu.update(navigation,options,sources);
                if(actions.resume && window) window->paused=false;
                if(actions.recenter && !options.desktop) xr.recenter();
                if(actions.exit) menu_exit=true;
                if(actions.library) {
                    try {
                        core->save_ram(); total_frames+=core->frame_count();
                        audio.reset(); core.reset(); in_library=true;
                        menu=bvb::SettingsMenu{}; library.reset_input(); library.refresh();
                        suppress_input=true; clock.advance(0,false); scene_changed=true; persist();
                    } catch(const std::exception& e) {menu.status="RAM SAVE FAILED - SEE CONSOLE"; std::cerr<<e.what()<<'\n';}
                }
            }
            if(!in_library) {
                if(current_slot!=options.state_slot) {
                    current_slot=options.state_slot; menu.status="SELECT SLOT THEN SAVE OR LOAD"; refresh_slot();
                }
                if(actions.save_state || actions.load_state) {
                    try {
                        use_audio([](auto& output){output.set_active(false);});
                        const auto file=slot_path();
                        if(actions.save_state) {core->save_state(file); menu.status="SAVED SLOT "+std::to_string(options.state_slot+1);}
                        else if(!fs::exists(file)) menu.status="SLOT "+std::to_string(options.state_slot+1)+" IS EMPTY";
                        else {
                            core->load_state(file); state_loaded=true; suppress_input=true; clock.advance(0,false);
                            menu.status="LOADED SLOT "+std::to_string(options.state_slot+1)+" - SELECT RESUME";
                        }
                        refresh_slot();
                    } catch(const bvb::StateRecoveryError&) {throw;}
                    catch(const std::exception& e) {menu.status=actions.save_state?"SAVE FAILED - SEE CONSOLE":"LOAD FAILED - SEE CONSOLE"; std::cerr<<"Save state: "<<e.what()<<'\n';}
                }
            }
            if (window) window->muted = options.muted;
            if (in_library || menu.open || (menu_was_open && !menu.open)) suppress_input = true;
            const bool raw_held = sources.any() || vr.held || std::any_of(raw.buttons.begin(), raw.buttons.end(), [](bool value) { return value; });
            if (!in_library && focused && !menu.blocks_game() && !raw_held && !navigation.any()) suppress_input = false;
            if (menu_was_open && !menu.open) persist();
            if(core) core->set_palette(bvb::palette_options[options.palette]);
            const bool active = !in_library && !scene_changed && !exiting && focused && !menu.open && !(window && window->paused);
            const auto frames = clock.advance(std::chrono::duration<double>(now - previous).count(), active);
            // Exclude synchronous cartridge loading/scanning from the next game's timing.
            previous = scene_changed ? std::chrono::steady_clock::now() : now;
            use_audio([&](auto& output) { output.set_active(active); output.set_muted(options.muted); output.set_volume(options.volume); });
            bvb::GameInput input;
            if (active && !options.headless && !suppress_input && !menu.blocks_game()) input = raw;
            for (unsigned i = 0; i < frames; ++i) {
                core->run_frame(input);
                use_audio([&](auto& output) { output.submit(core->audio()); });
            }
            const auto settings_text = bvb::encode_settings(options);
            const bool settings_changed = settings_text != displayed_settings;
            if (settings_changed) settings_changed_at = now;
            const auto frame_count=core?core->frame_count():0;
            if ((in_library && displayed_library_revision!=library.revision) || scene_changed || last_core_frame != frame_count || settings_changed || displayed_menu_revision != menu.revision || displayed_menu != menu.open || displayed_selection != menu.selected || displayed_status != menu.status || state_loaded) {
                display_frame = in_library?library.render():menu.render(bvb::prepare_game_image(core->frame(),options),options);
                ++display_version; displayed_library_revision=library.revision;
                last_core_frame = frame_count; displayed_settings = settings_text;
                displayed_menu_revision=menu.revision; displayed_menu = menu.open; displayed_selection = menu.selected; displayed_status = menu.status;
            }
            if (now - settings_changed_at >= std::chrono::milliseconds(500) && settings_text != saved_settings) {
                persist(); settings_changed_at = now; // Back off if the destination is unwritable.
            }
            if (window) window->refresh(display_version, !active);
            if (core && now - last_save >= std::chrono::seconds(30)) { core->save_ram(); last_save = now; }
            if (!options.desktop && xr.running()) xr.render_frame(display_frame, options.screen);
            else Sleep(options.desktop ? 1 : 10);
        }
        if(core) {core->save_ram(); total_frames+=core->frame_count();}
        persist();
        std::cout << "Emulated " << total_frames << " frames. Player closed.\n";
        return xr.lost() ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nUse --desktop to play without OpenXR, or --help for launch options.\n";
        return 1;
    }
}
