// SPDX-License-Identifier: GPL-2.0-or-later
#include "state_file.h"
#include "save_ram.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace bvb {
namespace {
constexpr char magic[] = "BVBSTATE";
constexpr std::size_t header_size = 48, digest_size = 16, eye_bytes = eye_width * eye_height * 4;
bool valid_id(const std::string& id) {
    return id.size() == 16 && id.find_first_not_of("0123456789abcdef") == std::string::npos;
}
void append32(std::vector<std::uint8_t>& data, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) data.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}
std::uint32_t read32(const std::vector<std::uint8_t>& data, std::size_t position) {
    std::uint32_t result = 0;
    for (int i = 0; i < 4; ++i) result |= std::uint32_t(data[position+i]) << (i * 8);
    return result;
}
}
std::filesystem::path state_slot_path(const std::filesystem::path& directory, const std::string& cartridge, int slot) {
    if (directory.empty() || !valid_id(cartridge) || slot < 0 || slot > 9) throw std::invalid_argument("Invalid save-state slot");
    return directory / (cartridge + "-slot" + std::to_string(slot + 1) + ".state");
}
void write_state_file(const std::filesystem::path& file, const SavedState& state) {
    if (file.empty() || !valid_id(state.cartridge) || !valid_id(state.core) || state.payload.empty()
        || state.payload.size() > max_state_size || state.frame.left.size() != eye_bytes || state.frame.right.size() != eye_bytes)
        throw std::invalid_argument("Incomplete save state");
    std::vector<std::uint8_t> bytes;
    bytes.reserve(header_size + state.payload.size() + eye_bytes * 2 + digest_size);
    bytes.insert(bytes.end(), magic, magic+8); append32(bytes, 1); append32(bytes, static_cast<std::uint32_t>(state.payload.size()));
    bytes.insert(bytes.end(), state.cartridge.begin(), state.cartridge.end());
    bytes.insert(bytes.end(), state.core.begin(), state.core.end());
    bytes.insert(bytes.end(), state.payload.begin(), state.payload.end());
    bytes.insert(bytes.end(), state.frame.left.begin(), state.frame.left.end());
    bytes.insert(bytes.end(), state.frame.right.begin(), state.frame.right.end());
    const auto checksum = data_id(bytes.data(), bytes.size());
    bytes.insert(bytes.end(), checksum.begin(), checksum.end());
    write_atomic_file(file, bytes.data(), bytes.size());
}
SavedState read_state_file(const std::filesystem::path& file, const std::string& cartridge,
                          const std::string& core, std::size_t expected_size) {
    const auto size = std::filesystem::file_size(file);
    if (!expected_size || expected_size > max_state_size || size < header_size + eye_bytes * 2 + digest_size
        || size > header_size + max_state_size + eye_bytes * 2 + digest_size)
        throw std::runtime_error("Invalid save-state size");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::ifstream input(file, std::ios::binary);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input || input.peek() != std::char_traits<char>::eof()) throw std::runtime_error("Cannot read complete save state");
    if (!std::equal(magic, magic+8, bytes.begin()) || read32(bytes,8) != 1) throw std::runtime_error("Unsupported save-state format");
    const auto payload_size = read32(bytes,12);
    if (payload_size != expected_size || size != header_size + payload_size + eye_bytes * 2 + digest_size)
        throw std::runtime_error("Incompatible save-state size");
    const std::string saved_cartridge(bytes.begin()+16,bytes.begin()+32), saved_core(bytes.begin()+32,bytes.begin()+48);
    if (saved_cartridge != cartridge) throw std::runtime_error("Save state belongs to another game");
    if (saved_core != core) throw std::runtime_error("Save state requires its original core build");
    const auto digest_start = bytes.size() - digest_size;
    if (std::string(bytes.begin()+digest_start,bytes.end()) != data_id(bytes.data(), digest_start))
        throw std::runtime_error("Save-state checksum mismatch");
    auto start = bytes.begin() + header_size;
    SavedState result{saved_cartridge,saved_core,{}, {}};
    result.payload.assign(start,start+payload_size); start += payload_size;
    result.frame.left.assign(start,start+eye_bytes); start += eye_bytes;
    result.frame.right.assign(start,start+eye_bytes);
    for (std::size_t i = 3; i < eye_bytes; i += 4) {
        if (result.frame.left[i] != 255 || result.frame.right[i] != 255) throw std::runtime_error("Invalid save-state eye image");
    }
    return result;
}
}
