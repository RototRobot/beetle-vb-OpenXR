// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <filesystem>
#include <vector>
#include <cstdint>
#include <string>

namespace bvb {
std::string data_id(const void* data, std::size_t size);
void write_atomic_file(const std::filesystem::path& file, const void* data, std::size_t size);
std::string cartridge_id(const std::vector<std::uint8_t>& rom);
void read_save_ram(const std::filesystem::path& file, void* destination, std::size_t size);
void write_save_ram(const std::filesystem::path& file, const void* data, std::size_t size);
}
