// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace bvb {
struct GameData {
    std::string id, title, year, developer, publisher, genre, overview;
    std::vector<std::string> aliases;
    std::string image; // LaunchBox image filename; never an arbitrary URL/path.
};
using GameCatalog = std::vector<GameData>;
std::string game_title_key(const std::string& title);
const GameData* match_game_data(const GameCatalog& catalog, const std::filesystem::path& rom);
bool valid_game_image(const std::string& name);
std::string game_image_url(const std::string& name);
GameCatalog read_launchbox_xml(const std::filesystem::path& xml, const std::atomic<bool>& cancel);
GameCatalog read_launchbox_zip(const std::filesystem::path& zip, const std::filesystem::path& temporary_xml,
                              const std::atomic<bool>& cancel);
GameCatalog load_game_catalog(const std::filesystem::path& file);
void save_game_catalog(const std::filesystem::path& file, const GameCatalog& catalog);
// HTTPS only, fixed provider hosts, bounded response, no local filenames or ROM contents sent.
using DownloadProgress = std::function<void(std::uint64_t)>;
using GameDownloader = std::function<void(const std::string&, const std::filesystem::path&, std::uint64_t,
                                         const std::atomic<bool>&, const DownloadProgress&)>;
void download_game_file(const std::string& url, const std::filesystem::path& output, std::uint64_t limit,
                        const std::atomic<bool>& cancel, const DownloadProgress& progress);
struct DataScanGame { std::filesystem::path rom; bool has_local_cover = false; };
struct DataScanStatus { bool running = false, finished = false; std::string message; };
class GameDataScan {
public:
    ~GameDataScan();
    void start(const std::filesystem::path& cache, std::vector<DataScanGame> games, bool update_database,
               GameDownloader downloader = download_game_file);
    void cancel();
    DataScanStatus status() const;
private:
    std::thread worker;
    std::atomic<bool> cancelled{false};
    mutable std::mutex mutex;
    DataScanStatus state;
    void report(std::string message, bool finished = false);
};
}
