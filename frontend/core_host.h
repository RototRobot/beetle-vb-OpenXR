// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "stereo_frame.h"
#include "game_input.h"
#include <filesystem>
#include <memory>
#include <string>
#include <stdexcept>

namespace bvb {
class StateRecoveryError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
// A single active libretro core per process, owned/run on the calling thread.
class CoreHost {
public:
    CoreHost();
    ~CoreHost();
    CoreHost(const CoreHost&) = delete;
    CoreHost& operator=(const CoreHost&) = delete;
    void initialize(const std::filesystem::path& library);
    // An empty save directory disables persistence (used by the diagnostic probe).
    void load_game(const std::filesystem::path& rom, const std::filesystem::path& save_directory);
    void load_synthetic(); // Generated blank cartridge; no persistent save file.
    void run_frame(const GameInput& input = {});
    void save_ram();
    void save_state(const std::filesystem::path& file);
    void load_state(const std::filesystem::path& file);
    std::string cartridge_key() const;
    void set_palette(const std::string& palette);
    const StereoFrame& frame() const;
    const std::vector<std::int16_t>& audio() const; // Interleaved PCM from the last run_frame.
    double fps() const;
    unsigned sample_rate() const;
    std::uint64_t frame_count() const;
    std::string name() const;
    std::filesystem::path save_path() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
