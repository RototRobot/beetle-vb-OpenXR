// SPDX-License-Identifier: GPL-2.0-or-later
#include "save_ram.h"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace bvb {
std::string data_id(const void* data, std::size_t size) {
    // FNV-1a content identifier (not a cryptographic authentication hash).
    std::uint64_t hash = 14695981039346656037ull;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= 1099511628211ull; }
    std::ostringstream text;
    text << std::hex << std::setfill('0') << std::setw(16) << hash;
    return text.str();
}
std::string cartridge_id(const std::vector<std::uint8_t>& rom) { return data_id(rom.data(), rom.size()); }
void read_save_ram(const std::filesystem::path& file, void* destination, std::size_t size) {
    if (!std::filesystem::exists(file)) return;
    if (!destination || !size) throw std::runtime_error("Core has no save RAM for an existing save file");
    if (std::filesystem::file_size(file) != size)
        throw std::runtime_error("Save RAM size mismatch; preserved existing file: " + file.u8string());
    std::vector<char> buffer(size);
    std::ifstream input(file, std::ios::binary);
    input.read(buffer.data(), static_cast<std::streamsize>(size));
    if (!input || input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Cannot read complete save RAM: " + file.u8string());
    std::memcpy(destination, buffer.data(), size);
}
void write_atomic_file(const std::filesystem::path& file, const void* data, std::size_t size) {
    if (file.empty() || !data || !size) return;
    if (!file.parent_path().empty()) std::filesystem::create_directories(file.parent_path());
    auto temporary = file;
    temporary += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        output.flush();
        if (!output) throw std::runtime_error("Cannot write file: " + file.u8string());
        output.close();
        if (!output) throw std::runtime_error("Cannot close file: " + file.u8string());
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace file: " + file.u8string());
#else
        std::filesystem::rename(temporary, file);
#endif
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
void write_save_ram(const std::filesystem::path& file, const void* data, std::size_t size) {
    write_atomic_file(file, data, size);
}
}
