// SPDX-License-Identifier: GPL-2.0-or-later
#include "core_host.h"
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: bvb_core_probe <core-library> [game.vb | --synthetic-blank]\n";
        return 2;
    }
    try {
        bvb::CoreHost core;
        core.initialize(std::filesystem::u8path(argv[1]));
        std::cout << core.name() << '\n';
        if (argc == 3) {
            const bool synthetic = std::string(argv[2]) == "--synthetic-blank";
            if (synthetic) core.load_synthetic();
            else core.load_game(std::filesystem::u8path(argv[2]), {});
            for (unsigned i = 0; i < 120; ++i) core.run_frame();
            if (core.frame().left.empty() || core.frame().right.empty()) throw std::runtime_error("No stereo frames received");
            std::cout << core.frame_count() << " core frames, 384x224 per eye, " << core.fps() << " fps, "
                      << core.sample_rate() << " Hz. Audio discarded, inputs neutral.\n";
            if (synthetic) std::cout << "Synthetic blank cartridge; game compatibility/accuracy were not tested.\n";
            core.save_ram();
        } else std::cout << "Core API and initialization passed. No ROM executed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
