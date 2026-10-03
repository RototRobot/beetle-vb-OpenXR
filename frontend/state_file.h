// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "stereo_frame.h"
#include <filesystem>
#include <string>
namespace bvb {
inline constexpr std::size_t max_state_size = 16 * 1024 * 1024;
struct SavedState {
    std::string cartridge, core;
    std::vector<std::uint8_t> payload;
    StereoFrame frame;
};
void write_state_file(const std::filesystem::path& file, const SavedState& state);
SavedState read_state_file(const std::filesystem::path& file, const std::string& cartridge,
                          const std::string& core, std::size_t expected_size);
std::filesystem::path state_slot_path(const std::filesystem::path& directory, const std::string& cartridge, int slot);
}
