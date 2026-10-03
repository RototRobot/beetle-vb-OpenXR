// SPDX-License-Identifier: GPL-2.0-or-later
#include "core_host.h"
#include "frame_clock.h"
#include "save_ram.h"
#include <libretro.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
void require(bool result, const char* message) { if (!result) throw std::runtime_error(message); }
struct Temporary {
    fs::path directory;
    explicit Temporary(const fs::path& parent) {
        const auto base = fs::absolute(parent).lexically_normal();
        directory = base / ("host-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(directory.parent_path() == base, "Temporary files must stay in the requested build directory");
        require(fs::create_directory(directory), "Cannot create isolated test directory");
    }
    ~Temporary() { std::error_code ignored; fs::remove_all(directory, ignored); }
};
void write(const fs::path& file, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(file, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(bool(output), "Cannot write test fixture");
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::invalid_argument("Pass core-library and build directory");
        Temporary temporary(fs::u8path(argv[2]));
        for (double refresh : {72., 90., 120.}) {
            bvb::FrameClock clock(50.27);
            unsigned total = 0;
            for (unsigned i = 0; i < unsigned(refresh * 10); ++i) total += clock.advance(1 / refresh);
            require(total == 502, "Game timing varies with headset refresh rate");
        }
        bvb::FrameClock clock(50);
        require(clock.advance(0.01) == 0, "Premature game frame");
        require(clock.advance(10, false) == 0 && clock.advance(0.01) == 0, "Pause retained a catch-up backlog");
        require(clock.advance(100) <= 5 && clock.advance(0) == 0, "Unbounded catch-up after stall");
        bvb::FrameClock exact(50);
        require(exact.advance(0.02) == 1, "Exact frame period lost");
        bool bad_rate = false;
        try { bvb::FrameClock invalid(std::numeric_limits<double>::quiet_NaN()); } catch (const std::invalid_argument&) { bad_rate = true; }
        require(bad_rate, "Invalid core rate accepted");

        bvb::GamepadState pad;
        pad.left_x = -32768; pad.right_y = 32767; pad.a = true; pad.start = true;
        auto input = bvb::map_gamepad(pad);
        require(input[bvb::Button::left_left] && input[bvb::Button::right_up] && input[bvb::Button::a], "Stick/button mapping wrong");
        auto mask = bvb::libretro_buttons(input);
        require(mask == ((1u << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1u << RETRO_DEVICE_ID_JOYPAD_L2) |
            (1u << RETRO_DEVICE_ID_JOYPAD_A) | (1u << RETRO_DEVICE_ID_JOYPAD_START)), "VB right pad uses wrong libretro IDs");
        pad = {}; pad.left_x = 8192; pad.right_y = -8192;
        require(bvb::libretro_buttons(bvb::map_gamepad(pad)) == 0, "Stick deadzone not enforced");
        bvb::GameInput keys; keys[bvb::Button::left_right] = true; keys[bvb::Button::b] = true;
        auto merged = bvb::combine_inputs(keys, input);
        require(!merged[bvb::Button::left_right] && !merged[bvb::Button::left_left] && merged[bvb::Button::a] && merged[bvb::Button::b], "Input merge/opposites wrong");

        const std::vector<std::uint8_t> saved{1,2,3,4};
        auto save = temporary.directory / "save.srm";
        bvb::write_save_ram(save, saved.data(), saved.size());
        std::vector<std::uint8_t> restored(4);
        bvb::read_save_ram(save, restored.data(), restored.size());
        require(restored == saved, "Save round-trip failed");
        const std::vector<std::uint8_t> replacement{5,6,7,8};
        bvb::write_save_ram(save, replacement.data(), replacement.size());
        bvb::read_save_ram(save, restored.data(), restored.size());
        require(restored == replacement, "Existing save replacement failed");
        bool rejected = false;
        std::vector<std::uint8_t> wrong(8, 0xAA);
        try { bvb::read_save_ram(save, wrong.data(), wrong.size()); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected && wrong == std::vector<std::uint8_t>(8, 0xAA), "Malformed save was accepted or destination modified");
        require(fs::file_size(save) == 4, "Failed save read damaged original");
        bvb::read_save_ram(temporary.directory / "missing.srm", restored.data(), restored.size());
        require(restored == replacement, "Missing save changed initial RAM");
        require(bvb::cartridge_id({1,2,3}) != bvb::cartridge_id({1,2,4}), "Different cartridges share a save ID");

        auto library = fs::u8path(argv[1]);
        {
            bvb::CoreHost core; core.initialize(library); core.set_palette("black & white"); core.load_synthetic();
            bool palette_rejected = false;
            try { core.set_palette("invalid palette"); } catch (const std::invalid_argument&) { palette_rejected = true; }
            require(palette_rejected, "Host accepted an unsupported core palette");
            bvb::CoreHost second;
            bool blocked = false;
            try { second.initialize(library); } catch (const std::logic_error&) { blocked = true; }
            require(blocked, "Concurrent hosts share core global state");
            for (unsigned i = 0; i < 5; ++i) core.run_frame(input);
            core.set_palette("black & cyan"); core.run_frame(input);
            require(core.frame_count() == 6 && core.frame().left.size() == 384 * 224 * 4, "Real core video integration failed");
            require(std::abs(core.fps() - 50.27) < 0.001 && core.sample_rate() == 44100, "Real core timing ignored");
            require(!core.audio().empty() && core.audio().size() % 2 == 0, "Real core audio not delivered");
            require(core.save_path().empty(), "Synthetic test creates a persistent save");
        }
        std::vector<std::uint8_t> rom(65536);
        auto rom_file = temporary.directory / fs::u8path("blank-\xc3\xa9.vb");
        write(rom_file, rom);
        auto saves = temporary.directory / "saves";
        fs::path cartridge_save;
        {
            bvb::CoreHost core; core.initialize(library); core.load_game(rom_file, saves); core.run_frame();
            cartridge_save = core.save_path(); core.save_ram();
        }
        require(fs::file_size(cartridge_save) == 65536, "Real core save RAM missing");
        std::vector<std::uint8_t> seeded(65536, 0x5A);
        write(cartridge_save, seeded);
        {
            bvb::CoreHost core; core.initialize(library); core.load_game(rom_file, saves); core.save_ram();
        }
        std::vector<std::uint8_t> round_trip(65536);
        bvb::read_save_ram(cartridge_save, round_trip.data(), round_trip.size());
        require(round_trip == seeded, "Core startup overwrote saved cartridge RAM");
        write(cartridge_save, {1,2,3});
        {
            bvb::CoreHost core; core.initialize(library);
            bool protected_save = false;
            try { core.load_game(rom_file, saves); } catch (const std::runtime_error&) { protected_save = true; }
            require(protected_save, "Core accepted a malformed existing save");
        }
        require(fs::file_size(cartridge_save) == 3, "Failed load/destructor overwrote a malformed save");
        std::cout << "Timing across refresh rates, pause, input mapping, safe saves, Unicode ROM loading and real core reload passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
