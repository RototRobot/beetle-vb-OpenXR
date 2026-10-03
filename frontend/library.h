// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "settings_menu.h"
#include <map>
namespace bvb {
inline constexpr unsigned library_scale = 4;
inline constexpr unsigned library_width = eye_width * library_scale, library_height = eye_height * library_scale;
// Fixed Virtual Boy box-front layout (9:8), independent of a particular image file.
inline constexpr int library_cover_width = 72, library_cover_height = 64;
struct LibraryConfig { std::filesystem::path rom_directory, art_directory; };
LibraryConfig load_library_config(const std::filesystem::path& file);
void save_library_config(const std::filesystem::path& file, const LibraryConfig& config);
std::string library_title(const std::filesystem::path& file);
struct LibraryGame { std::filesystem::path rom, cover; std::string title; };
std::vector<LibraryGame> scan_library(const LibraryConfig& config);
struct CoverImage { unsigned width = 0, height = 0; std::vector<std::uint8_t> pixels; };
// WIC decodes/scales locally supplied PNG/JPEG/BMP art; no network access.
CoverImage load_cover_image(const std::filesystem::path& file, unsigned width, unsigned height);
struct LibraryActions { std::filesystem::path launch; bool config_changed = false, exit = false; };
class LibraryMenu {
public:
    LibraryConfig config;
    std::vector<LibraryGame> games;
    bool browsing = false, toolbar = true;
    int selected = 0, tool = 0, folder_selected = 0;
    std::filesystem::path folder;
    std::string status = "CHOOSE A ROM FOLDER TO GET STARTED";
    std::uint64_t revision = 0;
    void initialize(LibraryConfig value, const std::filesystem::path& suggested_folder);
    void refresh();
    void begin_browse(bool artwork = false);
    void reset_input(); // Require neutral controls on entering or returning from gameplay.
    LibraryActions update(const MenuInput& input);
    StereoFrame render();
private:
    MenuInput previous;
    bool armed = false, art_browser = false;
    std::filesystem::path suggestion;
    std::vector<std::filesystem::path> folders;
    std::map<std::filesystem::path,CoverImage> covers;
    void list_folders();
};
}
