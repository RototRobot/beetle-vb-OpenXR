// SPDX-License-Identifier: GPL-2.0-or-later
#include "library.h"
#include "save_ram.h"
#include "ui_draw.h"
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
namespace bvb {
namespace {
namespace fs = std::filesystem;
std::string lower(std::string s) { for (auto& c:s) if(c>='A' && c<='Z') c+=32; return s; }
std::string without_tags(const std::string& s) {
    std::string result; int depth=0;
    for (char c:s) {
        if (c=='(' || c=='[') ++depth;
        else if (c==')' || c==']') {if(depth) --depth;}
        else if(!depth) result+=c;
    }
    while(!result.empty() && result.back()==' ') result.pop_back();
    return result;
}
std::string title_key(const fs::path& file) {
    auto s=lower(without_tags(file.stem().u8string())); std::string result;
    for(char c:s) if((c>='a' && c<='z') || (c>='0' && c<='9')) result+=c;
    return result;
}
std::string display(std::string s) {
    for(auto& c:s) { if(c>='a' && c<='z') c-=32; else if(static_cast<unsigned char>(c)>=128) c='?'; }
    return s;
}
bool rom_extension(const fs::path& p) { const auto e=lower(p.extension().u8string()); return e==".vb" || e==".vboy" || e==".bin"; }
bool art_extension(const fs::path& p) { const auto e=lower(p.extension().u8string()); return e==".png" || e==".jpg" || e==".jpeg" || e==".bmp"; }
std::vector<fs::path> files(const fs::path& folder,bool art) {
    std::vector<fs::path> result;
    if(folder.empty()) return result;
    std::error_code error;
    // Check the selected root explicitly; skip inaccessible descendants below it.
    fs::directory_iterator root(folder,error);
    if(error) throw fs::filesystem_error("Cannot read library folder",folder,error);
    fs::recursive_directory_iterator it(folder,fs::directory_options::skip_permission_denied,error),end;
    if(error) throw fs::filesystem_error("Cannot read library folder",folder,error);
    std::size_t visited=0;
    for(;it!=end;it.increment(error)) {
        if(error) throw fs::filesystem_error("Cannot scan library",folder,error);
        if(++visited>10000) throw std::runtime_error("Library folder has too many entries; choose a smaller folder");
        // Junctions as well as symlinks can point back into the tree on Windows.
        const auto attributes=GetFileAttributesW(it->path().c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) {
            it.disable_recursion_pending(); continue;
        }
        if(it->is_regular_file(error) && (art?art_extension(it->path()):rom_extension(it->path()))) result.push_back(it->path());
        if(error) throw fs::filesystem_error("Cannot inspect library entry",it->path(),error);
    }
    if(error) throw fs::filesystem_error("Cannot finish library scan",folder,error);
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) {
        const auto first=lower(a.filename().u8string()),second=lower(b.filename().u8string());
        if(first!=second) return first<second;
        const auto first_path=lower(a.generic_u8string()),second_path=lower(b.generic_u8string());
        return first_path!=second_path?first_path<second_path:a.generic_u8string()<b.generic_u8string();
    });
    return result;
}
}
LibraryConfig load_library_config(const fs::path& file) {
    LibraryConfig result;
    if(!fs::exists(file)) return result;
    if(fs::file_size(file)>65536) throw std::runtime_error("Library preferences too large");
    std::ifstream input(file); if(!input) throw std::runtime_error("Cannot read library preferences");
    std::string line;
    while(std::getline(input,line)) {
        if(line.size()>32768) continue;
        std::istringstream row(line); std::string key,value;
        if(!(row>>key>>std::quoted(value))) continue;
        row>>std::ws; if(!row.eof() || value.find_first_of("\r\n\0",0,3)!=std::string::npos) continue;
        if(key=="rom_directory") result.rom_directory=fs::u8path(value);
        if(key=="art_directory") result.art_directory=fs::u8path(value);
    }
    return result;
}
void save_library_config(const fs::path& file,const LibraryConfig& config) {
    std::ostringstream output;
    for(const auto& entry: {std::make_pair("rom_directory",config.rom_directory),std::make_pair("art_directory",config.art_directory)}) {
        const auto value=entry.second.u8string();
        if(value.size()>32700 || value.find_first_of("\r\n\0",0,3)!=std::string::npos) throw std::invalid_argument("Unsupported library path");
        output<<entry.first<<' '<<std::quoted(value)<<'\n';
    }
    const auto text=output.str(); write_atomic_file(file,text.data(),text.size());
}
std::string library_title(const fs::path& file) {return display(without_tags(file.stem().u8string()));}
std::vector<LibraryGame> scan_library(const LibraryConfig& config) {
    const auto roms=files(config.rom_directory,false);
    std::vector<fs::path> art;
    // Unavailable art must never hide otherwise usable games.
    try {art=files(config.art_directory,true);} catch(const std::exception&) {}
    std::map<std::string,std::vector<fs::path>> exact,titles;
    for(const auto& image:art) { exact[lower(image.stem().u8string())].push_back(image); titles[title_key(image)].push_back(image); }
    std::vector<LibraryGame> result;
    for(const auto& rom:roms) {
        LibraryGame entry{rom,{},library_title(rom)};
        const auto matched=exact.find(lower(rom.stem().u8string()));
        if(matched!=exact.end() && matched->second.size()==1) entry.cover=matched->second.front();
        else {
            const auto key=title_key(rom);
            const auto title=titles.find(key);
            if(!key.empty() && title!=titles.end() && title->second.size()==1) entry.cover=title->second.front();
        }
        result.push_back(std::move(entry));
    }
    return result;
}
void LibraryMenu::initialize(LibraryConfig value,const fs::path& suggested_folder) {
    config=std::move(value); suggestion=suggested_folder; refresh(); toolbar=games.empty(); reset_input();
}
void LibraryMenu::refresh() {
    ++revision;
    covers.clear();
    try {
        games=scan_library(config); selected=std::clamp(selected,0,std::max(0,int(games.size())-1));
        status=config.rom_directory.empty()?"CHOOSE A ROM FOLDER TO GET STARTED"
            :games.empty()?"NO ROMS FOUND - CHOOSE FOLDER OR RESCAN":"SELECT A GAME AND PRESS RIGHT TRIGGER";
    } catch(const std::exception& e) {games.clear(); selected=0; status="FOLDER UNAVAILABLE - CHOOSE ANOTHER"; std::cerr<<"Library: "<<e.what()<<'\n';}
    if(games.empty()) toolbar=true;
}
void LibraryMenu::reset_input() {previous={}; armed=false;}
void LibraryMenu::begin_browse(bool artwork) {
    art_browser=artwork; browsing=true; folder=artwork?config.art_directory:config.rom_directory;
    if(folder.empty()) folder=suggestion;
    ++revision; folder_selected=0; list_folders();
}
void LibraryMenu::list_folders() {
    status="SELECT USE THIS FOLDER TO REMEMBER IT";
    folders.clear();
    if(folder.empty()) {
        const auto drives=GetLogicalDrives();
        for(int i=0;i<26;++i) if(drives&(1u<<i)) folders.emplace_back(std::wstring(1,wchar_t(L'A'+i))+L":\\");
    } else {
        std::error_code error;
        fs::directory_iterator it(folder,error),end; std::size_t visited=0;
        if(error) {status="CANNOT READ FOLDER - GO UP OR CANCEL"; return;}
        for(;it!=end && visited<10000;it.increment(error),++visited) {
            if(error) break;
            if(it->is_directory(error) && !it->is_symlink(error)) folders.push_back(it->path());
            if(error) break;
        }
        if(error) status="SOME FOLDERS COULD NOT BE READ";
        std::sort(folders.begin(),folders.end(),[](const auto& a,const auto& b){return lower(a.filename().u8string())<lower(b.filename().u8string());});
    }
    folder_selected=std::clamp(folder_selected,0,int(folders.size())+2);
}
LibraryActions LibraryMenu::update(const MenuInput& input) {
    LibraryActions result;
    if(!armed) {if(!input.any()) armed=true; previous=input; return result;}
    const bool up=input.up && !previous.up && !input.down, down=input.down && !previous.down && !input.up;
    const bool horizontal=!input.up && !input.down;
    const bool left=horizontal && input.left && !previous.left && !input.right;
    const bool right=horizontal && input.right && !previous.right && !input.left;
    const bool accept=input.accept && !previous.accept && horizontal && !input.left && !input.right;
    const bool back=(input.back && !previous.back) || (input.toggle && !previous.toggle);
    previous=input;
    if(up || down || left || right || accept || back) ++revision;
    if(browsing) {
        const int count=int(folders.size())+3;
        if(up) folder_selected=(folder_selected+count-1)%count;
        if(down) folder_selected=(folder_selected+1)%count;
        auto parent=[&] {const auto next=folder.parent_path(); folder=(next==folder)?fs::path{}:next; folder_selected=0; list_folders();};
        if(back) {browsing=false; status="FOLDER SELECTION CANCELLED";}
        else if(left) parent();
        else if(accept && !up && !down) {
            if(folder_selected==0) {
                std::error_code error;
                if(folder.empty() || !fs::is_directory(folder,error) || error) status="CHOOSE AN ACCESSIBLE FOLDER FIRST";
                else {
                    (art_browser?config.art_directory:config.rom_directory)=fs::absolute(folder);
                    browsing=false; toolbar=art_browser || games.empty(); refresh();
                    if(!art_browser && !games.empty()) toolbar=false;
                    result.config_changed=true;
                }
            } else if(folder_selected==1) parent();
            else if(folder_selected==2) {browsing=false; status="FOLDER SELECTION CANCELLED";}
            else {folder=folders[folder_selected-3]; folder_selected=0; list_folders();}
        }
    } else if(toolbar) {
        if(left && !up && !down) tool=(tool+3)%4;
        if(right && !up && !down) tool=(tool+1)%4;
        if(down && !games.empty()) toolbar=false;
        if(accept && !back && !up && !down && !left && !right) {
            if(tool<2) begin_browse(tool==1);
            else if(tool==2) refresh();
            else result.exit=true;
        }
    } else {
        if(back) toolbar=true;
        else if(up) {if(selected<3) toolbar=true; else selected-=3;}
        else if(down) {if((selected/3+1)*3<int(games.size())) selected=std::min(selected+3,int(games.size())-1);}
        else if(left && selected>0) --selected;
        else if(right && selected+1<int(games.size())) ++selected;
        else if(accept && selected>=0 && selected<int(games.size())) result.launch=games[selected].rom;
    }
    return result;
}
StereoFrame LibraryMenu::render() {
    StereoFrame frame; frame.width=library_width; frame.height=library_height;
    frame.left.resize(std::size_t(frame.width)*frame.height*4);
    auto rectangle=[&](auto& pixels,int x,int y,int w,int h,ui::Color color) {
        ui::rectangle(pixels,x*library_scale,y*library_scale,w*library_scale,h*library_scale,color,frame.width,frame.height);
    };
    auto text=[&](auto& pixels,int x,int y,const std::string& value,ui::Color color,int scale=1) {
        ui::text(pixels,x*library_scale,y*library_scale,value,color,scale*library_scale,frame.width,frame.height);
    };
    rectangle(frame.left,0,0,eye_width,eye_height,{12,18,26});
    text(frame.left,14,9,"VIRTUAL BOY",{235,242,255},2);
    text(frame.left,browsing?14:204,browsing?28:14,browsing?(art_browser?"CHOOSE COVER FOLDER":"CHOOSE ROM FOLDER"):"GAME LIBRARY",{135,160,185});
    if(browsing) {
        auto path=display(folder.empty()?"COMPUTER / DRIVES":folder.u8string());
        if(path.size()>58) path="..."+path.substr(path.size()-55);
        text(frame.left,14,43,path,{128,238,255});
        const int page=folder_selected/9*9,count=int(folders.size())+3;
        for(int i=page;i<std::min(count,page+9);++i) {
            const int y=61+(i-page)*14;
            if(i==folder_selected) rectangle(frame.left,10,y-3,364,13,{27,69,91});
            const auto label=i==0?"USE THIS FOLDER":i==1?"UP ONE LEVEL / DRIVES":i==2?"CANCEL":display(folders[i-3].filename().empty()?folders[i-3].u8string():folders[i-3].filename().u8string());
            text(frame.left,18,y,label.substr(0,57),{215,225,235});
        }
    } else {
        const char* tools[]={"ROM FOLDER","COVER FOLDER","RESCAN","EXIT"};
        for(int i=0;i<4;++i) {
            const int x=10+i*93;
            rectangle(frame.left,x,29,90,12,toolbar && tool==i?ui::Color{27,69,91}:ui::Color{21,31,43});
            text(frame.left,x+3,32,tools[i],toolbar && tool==i?ui::Color{128,238,255}:ui::Color{180,200,220});
        }
        const int page=selected/6*6;
        for(int i=page;i<std::min(int(games.size()),page+6);++i) {
            const int cell_x=10+(i-page)%3*123;
            const int x=cell_x+(117-library_cover_width)/2,y=44+(i-page)/3*74;
            const bool chosen=!toolbar && selected==i;
            rectangle(frame.left,x,y,library_cover_width,library_cover_height,chosen?ui::Color{60,145,160}:ui::Color{31,43,55});
            rectangle(frame.left,x+1,y+1,library_cover_width-2,library_cover_height-2,{17,24,34});
            const auto& cover=games[i].cover;
            if(!cover.empty() && covers.find(cover)==covers.end()) {
                // Bound memory to the current visible page; only six thumbnails are needed.
                if(covers.size()>=6) covers.clear();
                try {covers.emplace(cover,load_cover_image(cover,(library_cover_width-2)*library_scale,(library_cover_height-2)*library_scale));}
                catch(const std::exception&) {covers.emplace(cover,CoverImage{});}
            }
            const auto found=covers.find(cover);
            if(found!=covers.end() && !found->second.pixels.empty()) {
                const auto& image=found->second;
                const int px=x*library_scale+(library_cover_width*library_scale-int(image.width))/2;
                const int py=y*library_scale+(library_cover_height*library_scale-int(image.height))/2;
                for(unsigned cy=0;cy<image.height;++cy) for(unsigned cx=0;cx<image.width;++cx) {
                    const auto src=(cy*image.width+cx)*4,dst=((py+cy)*frame.width+px+cx)*4;
                    for(int c=0;c<3;++c) frame.left[dst+c]=static_cast<std::uint8_t>((image.pixels[src+c]*image.pixels[src+3]+17*(255-image.pixels[src+3]))/255);
                }
            } else {text(frame.left,x+3,y+21,"VIRTUAL BOY",{135,160,185}); text(frame.left,x+12,y+34,"NO COVER",{100,130,155});}
            const auto title=games[i].title.substr(0,18);
            const int title_width=title.empty()?0:int(title.size())*6-1;
            text(frame.left,x+(library_cover_width-title_width)/2,y+library_cover_height+3,title,
                chosen?ui::Color{255,255,255}:ui::Color{190,205,220});
        }
        if(games.empty()) {text(frame.left,38,91,"YOUR VIRTUAL BOY LIBRARY",{215,225,235}); text(frame.left,38,110,"SELECT ROM FOLDER ABOVE",{128,238,255});}
        else text(frame.left,14,194,(games[selected].title+"  "+std::to_string(selected+1)+"/"+std::to_string(games.size())).substr(0,59),{215,225,235});
    }
    text(frame.left,14,204,status.substr(0,59),{128,238,255});
    text(frame.left,14,214,browsing?"PAD MOVE  R-TRIGGER CHOOSE  L-TRIGGER CANCEL":"PAD MOVE  R-TRIGGER PLAY  L-TRIGGER TOOLBAR",{135,160,185});
    frame.right=frame.left; return frame;
}
}
