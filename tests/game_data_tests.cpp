// SPDX-License-Identifier: GPL-2.0-or-later
#include "game_data.h"
#include "library.h"
#include "save_ram.h"
#include <windows.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
namespace fs=std::filesystem;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F action,const char* message) {
    bool rejected=false; try {action();} catch(const std::exception&) {rejected=true;} require(rejected,message);
}
void write(const fs::path& file,const std::string& value) {bvb::write_atomic_file(file,value.data(),value.size());}
void bitmap(const fs::path& file) {
    BITMAPFILEHEADER header{}; BITMAPINFOHEADER image{}; unsigned pixels[16]{};
    header.bfType=0x4D42; header.bfOffBits=sizeof(header)+sizeof(image); header.bfSize=header.bfOffBits+sizeof(pixels);
    image.biSize=sizeof(image); image.biWidth=4; image.biHeight=4; image.biPlanes=1; image.biBitCount=32;
    std::ofstream output(file,std::ios::binary); output.write(reinterpret_cast<char*>(&header),sizeof(header));
    output.write(reinterpret_cast<char*>(&image),sizeof(image)); output.write(reinterpret_cast<char*>(pixels),sizeof(pixels));
}
void wait(bvb::GameDataScan& scan) {
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(scan.status().running && std::chrono::steady_clock::now()<until) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    require(scan.status().finished,"Scan worker did not finish");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==4 && (std::string(argv[1])=="--inspect" || std::string(argv[1])=="--download-inspect")) {
            std::atomic<bool> cancel{false};
            const auto path=fs::u8path(argv[3]); fs::create_directories(path);
            if(std::string(argv[1])=="--download-inspect") {
                std::cout<<"Downloading public metadata using WinHTTP\n";
                bvb::download_game_file("https://gamesdb.launchbox-app.com/Metadata.zip",fs::u8path(argv[2]),256*1024*1024,cancel,[](std::uint64_t){});
            }
            const auto catalog=bvb::read_launchbox_zip(fs::u8path(argv[2]),path/"unpacked.tmp",cancel);
            bvb::save_game_catalog(path/"virtual-boy.txt",catalog);
            std::cout<<catalog.size()<<" Virtual Boy entries\n";
            for(const auto& game:catalog) if(game.title=="Mario Clash" || game.title=="Test Chamber") {
                const auto file=path/"probe-cover.tmp";
                bvb::download_game_file(bvb::game_image_url(game.image),file,32*1024*1024,cancel,[](std::uint64_t){});
                const auto image=bvb::load_cover_image(file,280,248);
                std::cout<<game.title<<": downloaded and decoded "<<image.width<<'x'<<image.height<<" cover\n";
            }
            return 0;
        }
        require(argc==3,"Pass output directory and fixture directory");
        const auto root=fs::absolute(fs::u8path(argv[1])).lexically_normal();
        const auto temp=root/("game-data-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(temp.parent_path()==root && fs::create_directory(temp),"Cannot create isolated test directory");
        struct Cleanup {fs::path path; ~Cleanup(){std::error_code error; fs::remove_all(path,error);}} cleanup{temp};
        const auto fixtures=fs::u8path(argv[2]); std::atomic<bool> cancel{false};
        auto catalog=bvb::read_launchbox_xml(fixtures/"launchbox.xml",cancel);
        require(catalog.size()==2,"Platform filtering failed");
        const auto* game=bvb::match_game_data(catalog,"Regional Adventure (Japan) [Rev 1].vb");
        require(game && game->id=="100" && game->year=="1995" && game->developer=="Example & Studio","Aliases, entities or fields failed");
        require(game->image=="00000000-0000-0000-0000-000000000003.png","Front art and region preference failed");
        require(catalog[1].image=="r2_00000000-0000-0000-0000-000000000004.png","Homebrew fanart fallback failed");
        require(bvb::game_image_url(catalog[1].image)=="https://gamesdb-images.launchbox.gg/"+catalog[1].image,"New CDN URL failed");
        require(!bvb::match_game_data(catalog,"Adventure Hack.vb"),"Uncertain title was guessed");
        require(!bvb::valid_game_image("../outside.png") && !bvb::valid_game_image("https://example.com/cover.png"),"Unsafe image filename accepted");
        catalog[1].aliases.push_back("Adventure");
        require(!bvb::match_game_data(catalog,"Adventure.vb"),"Ambiguous alias was guessed"); catalog[1].aliases.clear();
        const auto cache=temp/"cache"; fs::create_directory(cache);
        bvb::save_game_catalog(cache/"virtual-boy.txt",catalog);
        require(bvb::load_game_catalog(cache/"virtual-boy.txt")[0].developer=="Example & Studio","Cache roundtrip failed");
        write(temp/"bad.xml","<!DOCTYPE LaunchBox [<!ENTITY e SYSTEM 'file:///unavailable'>]><LaunchBox><Game><Name>&e;</Name></Game></LaunchBox>");
        rejects([&]{bvb::read_launchbox_xml(temp/"bad.xml",cancel);},"DTD accepted");
        write(temp/"bad.xml","<OtherRoot/>"); rejects([&]{bvb::read_launchbox_xml(temp/"bad.xml",cancel);},"Unexpected root accepted");
        write(temp/"bad.xml","<LaunchBox><Game>"); rejects([&]{bvb::read_launchbox_xml(temp/"bad.xml",cancel);},"Truncated XML accepted");
        auto zipped=bvb::read_launchbox_zip(fixtures/"launchbox.zip",temp/"unpacked.tmp",cancel);
        require(zipped.size()==2 && !fs::exists(temp/"unpacked.tmp"),"ZIP import or temporary cleanup failed");
        write(temp/"bad.zip","Not a ZIP"); rejects([&]{bvb::read_launchbox_zip(temp/"bad.zip",temp/"unpacked.tmp",cancel);},"Corrupt ZIP accepted");
        cancel=true; rejects([&]{bvb::read_launchbox_zip(fixtures/"launchbox.zip",temp/"unpacked.tmp",cancel);},"Cancelled import continued"); cancel=false;
        rejects([&]{bvb::download_game_file("http://example.com/cover.png",temp/"network.tmp",1024,cancel,[](std::uint64_t){});},"Unapproved URL accepted");
        const auto roms=temp/"roms",art=temp/"art"; fs::create_directory(roms); fs::create_directory(art);
        write(roms/"Regional Adventure (Japan).vb","fixture"); write(roms/"Space Race.vb","fixture");
        write(roms/"Unknown Homebrew.vb","fixture");
        const auto cover_fixture=temp/"image.bmp"; bitmap(cover_fixture);
        unsigned requests=0;
        auto downloader=[&](const std::string& url,const fs::path& out,std::uint64_t,const std::atomic<bool>&,const bvb::DownloadProgress& progress) {
            ++requests;
            fs::copy_file(url.find("Metadata.zip")!=std::string::npos?fixtures/"launchbox.zip":cover_fixture,out,fs::copy_options::overwrite_existing);
            progress(fs::file_size(out));
        };
        bvb::GameDataScan scan;
        scan.start(cache,{{roms/"Regional Adventure (Japan).vb",false},{roms/"Space Race.vb",false},{roms/"Unknown Homebrew.vb",false}},false,downloader);
        wait(scan); require(requests==2 && scan.status().message=="2 MATCHED  2 COVERS  0 FAILED","Cache reuse or scan summary failed");
        auto library=bvb::scan_library({roms,art},cache);
        require(library.size()==3 && library[0].title=="ADVENTURE" && !library[0].cover.empty() && library[2].cover.empty(),"Cached library data not applied");
        bitmap(art/"Regional Adventure (Japan).bmp"); library=bvb::scan_library({roms,art},cache);
        require(library[0].cover==art/"Regional Adventure (Japan).bmp","Downloaded art displaced manual art");
        scan.start(cache,{{roms/"Space Race.vb",false}},false,downloader); wait(scan);
        require(requests==2,"Already cached cover downloaded again");
        scan.start(cache,{{roms/"Space Race.vb",true}},true,downloader); wait(scan);
        require(requests==3 && !fs::exists(cache/"metadata-download.tmp") && !fs::exists(cache/"metadata-unpacked.tmp"),"Update or metadata cleanup failed");
        const auto failed_cache=temp/"failed-cache"; fs::create_directory(failed_cache);
        bvb::save_game_catalog(failed_cache/"virtual-boy.txt",catalog);
        auto failed_cover=[&](const std::string&,const fs::path& out,std::uint64_t,const std::atomic<bool>&,const bvb::DownloadProgress&) {
            write(out,"not an image");
        };
        scan.start(failed_cache,{{roms/"Space Race.vb",false}},false,failed_cover); wait(scan);
        require(scan.status().message=="1 MATCHED  0 COVERS  1 FAILED" && !fs::exists(failed_cache/"cover-download.tmp"),
                "Bad cover download was cached or interrupted scan");
        auto failed_database=[&](const std::string&,const fs::path&,std::uint64_t,const std::atomic<bool>&,const bvb::DownloadProgress&) {
            throw std::runtime_error("offline fixture");
        };
        scan.start(cache,{},true,failed_database); wait(scan);
        require(scan.status().message.find("SCAN FAILED")==0 && bvb::load_game_catalog(cache/"virtual-boy.txt").size()==2,
                "Failed database update damaged previous cache");
        std::atomic<bool> started{false};
        auto cancelling=[&](const std::string&,const fs::path&,std::uint64_t,const std::atomic<bool>& cancelled,const bvb::DownloadProgress&) {
            started=true; while(!cancelled.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            throw std::runtime_error("cancelled");
        };
        scan.start(cache,{},true,cancelling);
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!started && std::chrono::steady_clock::now()<until) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        require(started,"Cancellation fixture did not start"); scan.cancel(); wait(scan);
        require(scan.status().message=="SCAN CANCELLED" && bvb::load_game_catalog(cache/"virtual-boy.txt").size()==2,"Cancellation damaged existing catalog");
        std::cout<<"Game data tests passed\n"; return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n'; return 1;}
}
