// SPDX-License-Identifier: GPL-2.0-or-later
#include "library.h"
#include "save_ram.h"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
namespace fs=std::filesystem;
void require(bool result,const char* message) {if(!result) throw std::runtime_error(message);}
template<class F> void rejects(F f,const char* message) {bool rejected=false; try {f();} catch(const std::exception&) {rejected=true;} require(rejected,message);}
void write(const fs::path& path,const std::string& value="fixture") {bvb::write_atomic_file(path,value.data(),value.size());}
struct Temporary {
    fs::path path;
    explicit Temporary(const fs::path& parent) {
        const auto root=fs::absolute(parent).lexically_normal();
        path=root/("library-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(path.parent_path()==root && fs::create_directory(path),"Cannot create isolated library fixtures");
    }
    ~Temporary() {std::error_code error; fs::remove_all(path,error);}
};
void bitmap(const fs::path& file, unsigned width=64, unsigned height=32, bool detailed=false) {
    BITMAPFILEHEADER header{}; BITMAPINFOHEADER image{};
    std::vector<std::uint8_t> pixels(width*height*4);
    for(std::size_t i=0;i<pixels.size();i+=4) {
        const auto stripe=static_cast<std::uint8_t>((i/4%width/4)%2?220:20);
        pixels[i]=detailed?stripe:30; pixels[i+1]=detailed?stripe:90; pixels[i+2]=detailed?stripe:210; pixels[i+3]=255;
    }
    header.bfType=0x4D42; header.bfOffBits=sizeof(header)+sizeof(image); header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size());
    image.biSize=sizeof(image); image.biWidth=width; image.biHeight=-int(height); image.biPlanes=1; image.biBitCount=32;
    std::ofstream out(file,std::ios::binary); out.write(reinterpret_cast<const char*>(&header),sizeof(header));
    out.write(reinterpret_cast<const char*>(&image),sizeof(image)); out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
    require(bool(out),"Cannot write cover fixture");
}
void export_frame(const fs::path& file,const bvb::StereoFrame& frame) {
    BITMAPFILEHEADER header{}; BITMAPINFOHEADER image{};
    const auto pixels=bvb::rgba_to_bgra(frame.left);
    header.bfType=0x4D42; header.bfOffBits=sizeof(header)+sizeof(image); header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size());
    image.biSize=sizeof(image); image.biWidth=frame.width; image.biHeight=-int(frame.height); image.biPlanes=1; image.biBitCount=32;
    std::ofstream out(file,std::ios::binary); out.write(reinterpret_cast<const char*>(&header),sizeof(header));
    out.write(reinterpret_cast<const char*>(&image),sizeof(image)); out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
    require(bool(out),"Cannot export library fixture");
}
bvb::LibraryActions press(bvb::LibraryMenu& menu,bvb::MenuInput input) {menu.update({}); return menu.update(input);}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Pass test output directory"); Temporary temp(fs::u8path(argv[1]));
        const auto roms=temp.path/"roms",art=temp.path/"art",child=roms/"Child";
        fs::create_directories(child); fs::create_directories(art);
        write(roms/"Virtual Boy Wario Land (Japan, USA).VB");
        bitmap(art/"Virtual Boy Wario Land (Japan, USA) (En).bmp");
        const auto nested_art=art/"Region"/"Covers";
        fs::create_directories(nested_art);
        write(roms/"README.txt"); write(child/"ZZ Nested.vb");
        bitmap(nested_art/"ZZ Nested.bmp");
        auto scanned=bvb::scan_library({roms,art});
        require(scanned.size()==2 && !scanned[0].cover.empty() && scanned[0].title=="VIRTUAL BOY WARIO LAND","Extension, recursive scan or tagged title match failed");
        require(scanned[1].rom==child/"ZZ Nested.vb" && scanned[1].cover==nested_art/"ZZ Nested.bmp",
            "Nested ROM or deep artwork path not preserved");
        write(art/"Virtual Boy Wario Land (Other).png");
        require(bvb::scan_library({roms,art})[0].cover.empty(),"Ambiguous title guessed a cover");
        bitmap(art/"Virtual Boy Wario Land (Japan, USA).bmp");
        require(bvb::scan_library({roms,art})[0].cover.filename()=="Virtual Boy Wario Land (Japan, USA).bmp","Exact title not preferred");
        require(bvb::scan_library({roms,temp.path/"missing"}).size()==2,"Missing art hid a game");
        rejects([&]{bvb::scan_library({temp.path/"missing",art});},"Missing ROM directory silently scanned");
        fs::remove(child/"ZZ Nested.vb"); // Keep the existing grid fixture unchanged.

        const auto tree=temp.path/"recursive",tree_art=temp.path/"recursive-art";
        fs::create_directories(tree/"A"/"Deep"); fs::create_directories(tree/"B");
        fs::create_directories(tree_art/"A"); fs::create_directories(tree_art/"B");
        write(tree/"A"/"Deep"/"Same.BIN"); write(tree/"B"/"Same.BIN");
        write(tree/"A"/"Deep"/"Ignored.zip"); bitmap(tree_art/"A"/"Same.bmp");
        auto nested=bvb::scan_library({tree,tree_art});
        require(nested.size()==2 && nested[0].rom==tree/"A"/"Deep"/"Same.BIN"
            && nested[1].rom==tree/"B"/"Same.BIN" && nested[0].cover==tree_art/"A"/"Same.bmp"
            && nested[1].cover==nested[0].cover,"Nested duplicate ROMs lost paths, stable order or shared cover");
        bitmap(tree_art/"B"/"Same.bmp"); nested=bvb::scan_library({tree,tree_art});
        require(nested[0].cover.empty() && nested[1].cover.empty(),"Ambiguous nested covers guessed a match");
        bvb::LibraryMenu nested_menu; nested_menu.initialize({tree,tree_art},tree);
        bvb::MenuInput nested_accept; nested_accept.accept=true;
        require(press(nested_menu,nested_accept).launch==tree/"A"/"Deep"/"Same.BIN","Nested launch lost full path");
        write(tree/"B"/"New.vboy"); nested_menu.refresh();
        require(nested_menu.games.size()==3,"Rescan missed new nested ROM");
        // Directory junction creation needs no symlink privilege on Windows.
        const auto link=tree/"B"/"Loop";
        const std::wstring command=L"cmd /c mklink /J \""+link.wstring()+L"\" \""+tree.wstring()+L"\" >nul";
        require(_wsystem(command.c_str())==0,"Cannot create isolated junction fixture");
        struct JunctionCleanup {fs::path path; ~JunctionCleanup(){RemoveDirectoryW(path.c_str());}} junction_cleanup{link};
        require(bvb::scan_library({tree,tree_art}).size()==3,"Recursive scan followed a junction loop");
        require(RemoveDirectoryW(link.c_str())!=0,"Cannot remove junction fixture");

        const auto config_file=temp.path/"library.ini";
        bvb::LibraryConfig config{roms,art}; bvb::save_library_config(config_file,config);
        const auto saved=bvb::load_library_config(config_file);
        require(saved.rom_directory==roms && saved.art_directory==art,"Folder choices not persisted");
        config.rom_directory=temp.path/fs::u8path("unicode-\xc3\xa9 path");
        bvb::save_library_config(config_file,config);
        require(bvb::load_library_config(config_file).rom_directory==config.rom_directory,"Unicode path roundtrip failed");
        config.rom_directory=fs::path("invalid\npath");
        rejects([&]{bvb::save_library_config(config_file,config);},"Multiline path accepted");
        require(bvb::load_library_config(config_file).rom_directory!=config.rom_directory,"Failed config write replaced previous file");
        write(config_file,"unknown \"ignored\"\nrom_directory \"chosen\" trailing\nart_directory \"art\"\n");
        require(bvb::load_library_config(config_file).rom_directory.empty(),"Trailing garbage accepted");

        const auto image=bvb::load_cover_image(art/"Virtual Boy Wario Land (Japan, USA).bmp",40,40);
        require(image.width==40 && image.height==20 && image.pixels.size()==3200,"Cover aspect ratio or pixel size wrong");
        require(image.pixels[0]==210 && image.pixels[1]==90 && image.pixels[2]==30,"WIC RGBA channel order wrong");
        rejects([&]{bvb::load_cover_image(art/"Virtual Boy Wario Land (Other).png",40,40);},"Malformed image accepted");
        rejects([&]{bvb::load_cover_image(art/"Virtual Boy Wario Land (Japan, USA).bmp",0,40);},"Invalid thumbnail size accepted");

        bvb::LibraryMenu menu; menu.initialize({roms,art},roms);
        bvb::MenuInput accept; accept.accept=true;
        require(menu.update(accept).launch.empty(),"Held startup accept launched a game");
        require(press(menu,accept).launch==roms/"Virtual Boy Wario Land (Japan, USA).VB","Selected game did not launch");
        require(menu.update(accept).launch.empty(),"Held accept repeatedly launched game");
        menu.reset_input(); require(menu.update(accept).launch.empty(),"Return from game lacked neutral gate");
        bvb::MenuInput back; back.back=true;
        press(menu,back); require(menu.toolbar,"Back did not reach library toolbar");
        menu.tool=0; press(menu,accept); require(menu.browsing && menu.folder==roms,"ROM folder browser did not open");
        menu.folder_selected=3; press(menu,accept); require(menu.folder==child,"Folder entry did not navigate");
        menu.folder_selected=0; require(press(menu,accept).config_changed && menu.config.rom_directory==child,"Use folder did not update default");
        menu.begin_browse(); press(menu,back); require(!menu.browsing && menu.config.rom_directory==child,"Cancel changed folder choice");
        menu.begin_browse(); menu.folder_selected=1; press(menu,accept); require(menu.folder==roms,"Parent navigation failed");
        menu.folder=menu.folder.root_path(); menu.folder_selected=1; press(menu,accept); require(menu.folder.empty(),"Root did not navigate to drives");
        menu.folder_selected=0; require(!press(menu,accept).config_changed,"Computer/drives was accepted as a ROM folder");
        menu.begin_browse(true); menu.folder_selected=0;
        require(press(menu,accept).config_changed && menu.config.art_directory==art,"Cover folder choice not accepted");
        menu.browsing=false; menu.toolbar=true; menu.tool=2;
        press(menu,accept); require(menu.data_open,"Find Data did not open");
        press(menu,back); require(!menu.data_open && menu.toolbar,"Find Data did not return to toolbar");
        menu.tool=4; require(press(menu,accept).exit,"Library exit action missing");

        for(int i=0;i<7;++i) write(roms/("Game "+std::to_string(i)+".vboy"));
        bitmap(art/"Game 0.bmp",512,456,true);
        const auto fine=bvb::load_cover_image(art/"Game 0.bmp",280,248);
        const auto coarse=bvb::load_cover_image(art/"Game 0.bmp",54,48);
        auto contrast=[](const bvb::CoverImage& image) {
            int darkest=255,brightest=0;
            for(std::size_t i=0;i<image.pixels.size();i+=4) {darkest=std::min(darkest,int(image.pixels[i])); brightest=std::max(brightest,int(image.pixels[i]));}
            return brightest-darkest;
        };
        require(contrast(fine)>150 && contrast(coarse)<100,"Larger thumbnails did not retain fine source detail");
        menu.initialize({roms,art},roms); require(menu.games.size()==8,"Grid scan missed games");
        bvb::MenuInput down; down.down=true;
        press(menu,down); require(menu.selected==3,"Grid down did not move one row");
        auto diagonal=down; diagonal.right=true; menu.update(diagonal);
        require(menu.selected==3,"Sideways drift during held vertical navigation moved selection");
        press(menu,down); require(menu.selected==6,"Grid did not move to next page");
        bvb::MenuInput right; right.right=true; press(menu,right); require(menu.selected==7,"Last partial row inaccessible");
        bvb::MenuInput up; up.up=true; press(menu,up); require(menu.selected==4,"Grid up did not preserve column");
        menu.selected=5; press(menu,down); require(menu.selected==7,"Down into partial row did not clamp to last tile");
        const auto rendered=menu.render();
        require(rendered.left==rendered.right && rendered.width==bvb::library_width && rendered.height==bvb::library_height && bvb::valid_stereo_frame(rendered),"Library stereo plane invalid");
        for(std::size_t i=3;i<rendered.left.size();i+=4) require(rendered.left[i]==255,"Library image not opaque");
        const auto sample=(std::size_t(76*bvb::library_scale)*rendered.width+191*bvb::library_scale)*4;
        require(rendered.left[sample]==210 && rendered.left[sample+1]==90 && rendered.left[sample+2]==30,
            "High resolution cover placement or channels incorrect");
        export_frame(fs::u8path(argv[1])/"library-grid-page2-test.bmp",rendered);
        menu.selected=0; export_frame(fs::u8path(argv[1])/"library-grid-test.bmp",menu.render());
        menu.begin_browse(); export_frame(fs::u8path(argv[1])/"library-folders-test.bmp",menu.render());
        menu.browsing=false; menu.data_open=true;
        export_frame(fs::u8path(argv[1])/"library-game-data-test.bmp",menu.render());
        menu.data_open=false;
        menu.initialize({temp.path/"missing",art},roms);
        require(menu.games.empty() && menu.toolbar,"Unavailable default did not offer folder recovery");
        menu.render();
        std::cout<<"Library scan, art matching/decoding, persistence, folder navigation, grid pages and input gates passed.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
}
