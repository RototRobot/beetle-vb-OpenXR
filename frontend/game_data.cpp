// SPDX-License-Identifier: GPL-2.0-or-later
#include "game_data.h"
#include "library.h"
#include "save_ram.h"
#include <windows.h>
#include <winhttp.h>
#include <xmllite.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <miniz.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
namespace bvb {
namespace {
namespace fs = std::filesystem;
constexpr std::uint64_t zip_limit = 256ull*1024*1024, xml_limit = 1536ull*1024*1024;
constexpr std::uint64_t image_limit = 32ull*1024*1024;
void check_cancel(const std::atomic<bool>& cancel) {if(cancel.load()) throw std::runtime_error("SCAN CANCELLED");}
void check(HRESULT result) {if(FAILED(result)) throw std::runtime_error("Cannot read LaunchBox XML");}
bool valid_id(const std::string& id) {
    return !id.empty() && id.size()<=12 && std::all_of(id.begin(),id.end(),[](char c){return c>='0' && c<='9';});
}
std::string utf8(const wchar_t* value, UINT length) {
    if(!length) return {};
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,int(length),nullptr,0,nullptr,nullptr);
    if(size<=0) throw std::runtime_error("Invalid text in LaunchBox XML");
    std::string text(size,'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,int(length),text.data(),size,nullptr,nullptr); return text;
}
struct RemoveFile {fs::path file; ~RemoveFile(){std::error_code error; fs::remove(file,error);}};
struct InternetHandle {
    HINTERNET handle=nullptr;
    explicit InternetHandle(HINTERNET value):handle(value) {if(!handle) throw std::runtime_error("Cannot connect to LaunchBox");}
    ~InternetHandle(){WinHttpCloseHandle(handle);}
    InternetHandle(const InternetHandle&)=delete;
};
std::string clean_text(std::string value) {
    for(auto& c:value) if(static_cast<unsigned char>(c)<32) c=' ';
    return value;
}
}
bool valid_game_image(const std::string& name) {
    // Older media use the legacy host; r2_ names use LaunchBox's new CDN.
    const std::size_t prefix=name.compare(0,3,"r2_")==0?3:0;
    if(name.size()!=40+prefix) return false;
    for(std::size_t i=0;i<36;++i) {
        const char c=name[i+prefix];
        if(i==8 || i==13 || i==18 || i==23) {if(c!='-') return false;}
        else if(!((c>='a' && c<='f') || (c>='A' && c<='F') || (c>='0' && c<='9'))) return false;
    }
    return name.substr(36+prefix)==".jpg" || name.substr(36+prefix)==".png";
}
std::string game_image_url(const std::string& name) {
    if(!valid_game_image(name)) throw std::runtime_error("Invalid LaunchBox image filename");
    return (name.compare(0,3,"r2_")==0?"https://gamesdb-images.launchbox.gg/":"https://images.launchbox-app.com/")+name;
}
std::string game_title_key(const std::string& title) {
    std::string result; int depth=0;
    for(char c:title) {
        if(c=='(' || c=='[') ++depth;
        else if(c==')' || c==']') {if(depth) --depth;}
        else if(!depth) {
            if(c>='A' && c<='Z') c+=32;
            if((c>='a' && c<='z') || (c>='0' && c<='9')) result+=c;
        }
    }
    return result;
}
const GameData* match_game_data(const GameCatalog& catalog,const fs::path& rom) {
    const auto key=game_title_key(rom.stem().u8string());
    if(key.empty()) return nullptr;
    const GameData* result=nullptr;
    for(const auto& game:catalog) {
        bool matches=key==game_title_key(game.title);
        for(const auto& alias:game.aliases) matches=matches || key==game_title_key(alias);
        if(matches) {if(result && result->id!=game.id) return nullptr; result=&game;}
    }
    return result;
}
GameCatalog read_launchbox_xml(const fs::path& file,const std::atomic<bool>& cancel) {
    if(fs::file_size(file)>xml_limit) throw std::runtime_error("LaunchBox XML exceeds size limit");
    const auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(initialized) && initialized!=RPC_E_CHANGED_MODE) check(initialized);
    struct Cleanup {bool owned; ~Cleanup(){if(owned) CoUninitialize();}} cleanup{SUCCEEDED(initialized)};
    using Microsoft::WRL::ComPtr;
    ComPtr<IStream> stream;
    check(SHCreateStreamOnFileEx(file.c_str(),STGM_READ|STGM_SHARE_DENY_WRITE,0,FALSE,nullptr,&stream));
    ComPtr<IXmlReader> reader;
    check(CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(reader.GetAddressOf()),nullptr));
    check(reader->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit));
    check(reader->SetProperty(XmlReaderProperty_MaxElementDepth,16));
    check(reader->SetInput(stream.Get()));
    GameCatalog result; std::map<std::string,std::size_t> ids; std::map<std::string,int> image_scores;
    std::map<std::string,std::string> fields;
    std::string record,field; bool root=false;
    auto finish=[&] {
        const auto id=fields["DatabaseID"];
        if(record=="Game" && fields["Platform"]=="Nintendo Virtual Boy") {
            if(!valid_id(id) || fields["Name"].empty() || fields["Name"].size()>512 || ids.count(id) || result.size()>=5000)
                throw std::runtime_error("Invalid Virtual Boy metadata");
            GameData game; game.id=id; game.title=fields["Name"];
            game.year=fields["ReleaseYear"].empty()?fields["ReleaseDate"].substr(0,4):fields["ReleaseYear"];
            game.developer=fields["Developer"]; game.publisher=fields["Publisher"];
            game.genre=fields["Genres"]; game.overview=fields["Overview"];
            ids[id]=result.size(); result.push_back(std::move(game));
        } else if(ids.count(id)) {
            auto& game=result[ids.at(id)];
            if(record=="GameAlternateName" && !fields["AlternateName"].empty()) {
                if(game.aliases.size()>=100 || fields["AlternateName"].size()>512) throw std::runtime_error("Too many game aliases");
                game.aliases.push_back(fields["AlternateName"]);
            } else if(record=="GameImage" && valid_game_image(fields["FileName"])) {
                const auto type=fields["Type"],region=fields["Region"];
                int score=type=="Box - Front"?100:type=="Fanart - Box - Front"?50:0;
                if(score) {
                    score+=(region=="North America" || region=="United States")?5:region=="World"?4:region.empty()?3:region=="Europe"?2:1;
                    if(score>image_scores[id] || (score==image_scores[id] && fields["FileName"]<game.image)) {
                        image_scores[id]=score; game.image=fields["FileName"];
                    }
                }
            }
        }
        fields.clear(); record.clear(); field.clear();
    };
    XmlNodeType type; HRESULT status;
    while((status=reader->Read(&type))==S_OK) {
        check_cancel(cancel);
        UINT depth=0; check(reader->GetDepth(&depth));
        if(type==XmlNodeType_Element || type==XmlNodeType_EndElement) {
            const wchar_t* name=nullptr; UINT length=0; check(reader->GetLocalName(&name,&length));
            const auto tag=utf8(name,length);
            if(type==XmlNodeType_Element) {
                if(depth==0) {if(tag!="LaunchBox" || root) throw std::runtime_error("Unexpected LaunchBox XML root"); root=true;}
                else if(depth==1) {record=tag; fields.clear(); field.clear();}
                else if(depth==2) {
                    field=tag; fields[field]={};
                    if(fields.size()>100) throw std::runtime_error("Oversized LaunchBox XML record");
                }
                if(reader->IsEmptyElement()) {if(depth==1) finish(); else if(depth==2) field.clear();}
            } else {
                // XmlLite includes the closing element in its reported depth.
                if(depth==2) finish(); else if(depth==3) field.clear();
            }
        } else if((type==XmlNodeType_Text || type==XmlNodeType_CDATA || type==XmlNodeType_Whitespace)
                   && depth==3 && !field.empty()) {
            const wchar_t* value=nullptr; UINT length=0; check(reader->GetValue(&value,&length));
            auto& text=fields[field]; text+=utf8(value,length);
            if(text.size()>262144 || fields.size()>100) throw std::runtime_error("Oversized LaunchBox XML record");
        }
    }
    check(status); if(!root) throw std::runtime_error("Empty LaunchBox XML");
    return result;
}
GameCatalog read_launchbox_zip(const fs::path& file,const fs::path& temporary_xml,const std::atomic<bool>& cancel) {
    check_cancel(cancel);
    const auto size=fs::file_size(file);
    if(size>zip_limit) throw std::runtime_error("LaunchBox download exceeds size limit");
    std::ifstream input(file,std::ios::binary);
    if(!input) throw std::runtime_error("Cannot read metadata archive");
    mz_zip_archive zip{};
    zip.m_pIO_opaque=&input;
    zip.m_pRead=[](void* opaque,mz_uint64 offset,void* buffer,size_t count)->size_t {
        auto& stream=*static_cast<std::ifstream*>(opaque);
        stream.clear(); stream.seekg(static_cast<std::streamoff>(offset));
        stream.read(static_cast<char*>(buffer),static_cast<std::streamsize>(count)); return static_cast<size_t>(stream.gcount());
    };
    if(!mz_zip_reader_init(&zip,size,0)) throw std::runtime_error("Invalid metadata ZIP");
    struct ZipCleanup {mz_zip_archive* zip; ~ZipCleanup(){mz_zip_reader_end(zip);}} zip_cleanup{&zip};
    const int index=mz_zip_reader_locate_file(&zip,"Metadata.xml",nullptr,0);
    mz_zip_archive_file_stat info{};
    if(index<0 || !mz_zip_reader_file_stat(&zip,index,&info) || info.m_uncomp_size>xml_limit)
        throw std::runtime_error("Metadata.xml missing or too large");
    // Extract only this named member; never use a remote member path on disk.
    RemoveFile remove_xml{temporary_xml};
    std::ofstream output(temporary_xml,std::ios::binary|std::ios::trunc);
    if(!output) throw std::runtime_error("Cannot create temporary metadata file");
    struct Sink {std::ofstream& stream; const std::atomic<bool>& cancel;} sink{output,cancel};
    const auto write=[](void* opaque,mz_uint64 offset,const void* buffer,size_t count)->size_t {
        auto& target=*static_cast<Sink*>(opaque);
        if(target.cancel.load() || offset>xml_limit || count>xml_limit-offset) return 0;
        target.stream.write(static_cast<const char*>(buffer),static_cast<std::streamsize>(count));
        return target.stream?count:0;
    };
    if(!mz_zip_reader_extract_to_callback(&zip,index,write,&sink,0)) {
        check_cancel(cancel); throw std::runtime_error("Cannot unpack metadata archive");
    }
    output.close(); if(!output) throw std::runtime_error("Cannot write temporary metadata file");
    return read_launchbox_xml(temporary_xml,cancel);
}
void save_game_catalog(const fs::path& file,const GameCatalog& catalog) {
    std::ostringstream output; output<<"BVB_LAUNCHBOX 1\n";
    for(const auto& game:catalog) {
        output<<"game "<<game.id;
        for(const auto* text:{&game.title,&game.year,&game.developer,&game.publisher,&game.genre,&game.overview,&game.image})
            output<<' '<<std::quoted(clean_text(*text));
        output<<'\n';
        for(const auto& alias:game.aliases) output<<"alias "<<game.id<<' '<<std::quoted(clean_text(alias))<<'\n';
    }
    const auto text=output.str();
    if(text.size()>4*1024*1024) throw std::runtime_error("Virtual Boy catalog exceeds size limit");
    write_atomic_file(file,text.data(),text.size());
}
GameCatalog load_game_catalog(const fs::path& file) {
    if(!fs::exists(file)) return {};
    if(fs::file_size(file)>4*1024*1024) throw std::runtime_error("Cached game catalog too large");
    std::ifstream input(file); std::string line;
    if(!std::getline(input,line) || line!="BVB_LAUNCHBOX 1") throw std::runtime_error("Unsupported game catalog");
    GameCatalog result; std::map<std::string,std::size_t> ids;
    while(std::getline(input,line)) {
        std::istringstream row(line); std::string kind,id; row>>kind>>id;
        if(!valid_id(id)) throw std::runtime_error("Invalid cached game ID");
        if(kind=="game") {
            GameData game; game.id=id;
            for(auto* text:{&game.title,&game.year,&game.developer,&game.publisher,&game.genre,&game.overview,&game.image})
                if(!(row>>std::quoted(*text))) throw std::runtime_error("Invalid cached game record");
            if(game.title.empty() || game.title.size()>512 || (!game.image.empty() && !valid_game_image(game.image))
               || ids.count(id) || result.size()>=5000) throw std::runtime_error("Invalid cached game record");
            ids[id]=result.size(); result.push_back(std::move(game));
        } else if(kind=="alias" && ids.count(id)) {
            std::string alias;
            if(!(row>>std::quoted(alias)) || alias.size()>512 || result[ids[id]].aliases.size()>=100)
                throw std::runtime_error("Invalid cached alias");
            result[ids[id]].aliases.push_back(std::move(alias));
        } else throw std::runtime_error("Unknown cached game record");
        row>>std::ws; if(!row.eof()) throw std::runtime_error("Unexpected cached game data");
    }
    if(input.bad()) throw std::runtime_error("Cannot read cached game data");
    return result;
}
void download_game_file(const std::string& url,const fs::path& output,std::uint64_t limit,
                        const std::atomic<bool>& cancel,const DownloadProgress& progress) {
    check_cancel(cancel);
    std::wstring host,resource;
    if(url=="https://gamesdb.launchbox-app.com/Metadata.zip") {host=L"gamesdb.launchbox-app.com"; resource=L"/Metadata.zip";}
    else {
        const std::string legacy="https://images.launchbox-app.com/",cdn="https://gamesdb-images.launchbox.gg/";
        const auto prefix=url.compare(0,cdn.size(),cdn)==0?cdn:legacy;
        const auto path=url.size()>=prefix.size()?url.substr(prefix.size()):std::string{};
        if(url.compare(0,prefix.size(),prefix)!=0 || !valid_game_image(path) || game_image_url(path)!=url)
            throw std::runtime_error("Unsupported game data URL");
        host=prefix==cdn?L"gamesdb-images.launchbox.gg":L"images.launchbox-app.com";
        resource=L"/"+std::wstring(path.begin(),path.end());
    }
    InternetHandle session(WinHttpOpen(L"Beetle-vb-OpenXR/0.1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0));
    if(!WinHttpSetTimeouts(session.handle,5000,5000,5000,5000)) throw std::runtime_error("Cannot set download timeout");
    InternetHandle connection(WinHttpConnect(session.handle,host.c_str(),INTERNET_DEFAULT_HTTPS_PORT,0));
    InternetHandle request(WinHttpOpenRequest(connection.handle,L"GET",resource.c_str(),nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE));
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if(!WinHttpSetOption(request.handle,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects)))
        throw std::runtime_error("Cannot configure secure download");
    if(!WinHttpSendRequest(request.handle,nullptr,0,nullptr,0,0,0) || !WinHttpReceiveResponse(request.handle,nullptr))
        throw std::runtime_error("LaunchBox connection failed - try again later");
    DWORD status=0,length=sizeof(status);
    if(!WinHttpQueryHeaders(request.handle,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&length,nullptr) || status!=200)
        throw std::runtime_error("LaunchBox download unavailable (HTTP "+std::to_string(status)+")");
    RemoveFile cleanup{output};
    std::ofstream file(output,std::ios::binary|std::ios::trunc);
    if(!file) throw std::runtime_error("Cannot save game data download");
    char buffer[65536]; std::uint64_t total=0;
    for(;;) {
        check_cancel(cancel); DWORD received=0;
        if(!WinHttpReadData(request.handle,buffer,sizeof(buffer),&received)) throw std::runtime_error("Game data download interrupted");
        if(!received) break;
        if(total>limit || received>limit-total) throw std::runtime_error("Game data download exceeds size limit");
        file.write(buffer,received); if(!file) throw std::runtime_error("Cannot save game data download");
        total+=received; progress(total);
    }
    file.close(); if(!file || !total) throw std::runtime_error("Empty or unwritable game data download");
    check_cancel(cancel); cleanup.file.clear();
}
GameDataScan::~GameDataScan() {cancel(); if(worker.joinable()) worker.join();}
void GameDataScan::cancel() {cancelled.store(true);}
DataScanStatus GameDataScan::status() const {std::lock_guard<std::mutex> lock(mutex); return state;}
void GameDataScan::report(std::string message,bool finished) {
    std::lock_guard<std::mutex> lock(mutex); state={ !finished,finished,std::move(message)};
}
void GameDataScan::start(const fs::path& cache,std::vector<DataScanGame> games,bool update_database,GameDownloader downloader) {
    if(status().running) return;
    if(worker.joinable()) worker.join();
    cancelled.store(false); report("PREPARING GAME DATA");
    worker=std::thread([this,cache,games=std::move(games),update_database,downloader=std::move(downloader)] {
        try {
            fs::create_directories(cache/"covers");
            const auto catalog_file=cache/"virtual-boy.txt";
            GameCatalog catalog;
            try {catalog=load_game_catalog(catalog_file);} catch(const std::exception&) {}
            if(update_database || catalog.empty()) {
                const auto zip=cache/"metadata-download.tmp",xml=cache/"metadata-unpacked.tmp";
                RemoveFile remove_zip{zip};
                downloader("https://gamesdb.launchbox-app.com/Metadata.zip",zip,zip_limit,cancelled,[this](std::uint64_t bytes){
                    report("DOWNLOADING DATABASE: "+std::to_string(bytes/(1024*1024))+" MIB");
                });
                report("READING VIRTUAL BOY DATABASE");
                auto updated=read_launchbox_zip(zip,xml,cancelled);
                if(updated.empty()) throw std::runtime_error("No Virtual Boy entries in LaunchBox database");
                check_cancel(cancelled); save_game_catalog(catalog_file,updated); catalog=std::move(updated);
            }
            unsigned matched=0,downloaded=0,failed=0; std::set<std::string> seen;
            for(std::size_t i=0;i<games.size();++i) {
                check_cancel(cancelled);
                report("MATCHING GAME "+std::to_string(i+1)+"/"+std::to_string(games.size()));
                const auto* data=match_game_data(catalog,games[i].rom);
                if(!data) continue;
                ++matched;
                if(games[i].has_local_cover || data->image.empty() || !seen.insert(data->image).second) continue;
                const auto cover=cache/"covers"/fs::u8path(data->image);
                if(fs::exists(cover)) {
                    try {load_cover_image(cover,16,16); continue;} catch(const std::exception&) {}
                }
                const auto temporary=cache/"cover-download.tmp"; RemoveFile remove_cover{temporary};
                try {
                    report("DOWNLOADING COVER "+std::to_string(i+1)+"/"+std::to_string(games.size()));
                    downloader(game_image_url(data->image),temporary,image_limit,cancelled,[](std::uint64_t){});
                    check_cancel(cancelled); load_cover_image(temporary,16,16);
                    if(!MoveFileExW(temporary.c_str(),cover.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
                        throw std::runtime_error("Cannot store cover image");
                    ++downloaded;
                } catch(const std::exception&) {check_cancel(cancelled); ++failed;}
            }
            report(std::to_string(matched)+" MATCHED  "+std::to_string(downloaded)+" COVERS  "+std::to_string(failed)+" FAILED",true);
        } catch(const std::exception& e) {report(cancelled.load()?"SCAN CANCELLED":"SCAN FAILED - "+std::string(e.what()),true);}
    });
}
}
