// SPDX-License-Identifier: GPL-2.0-or-later
#include "core_host.h"
#include "state_file.h"
#include "save_ram.h"
#include "presentation.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
namespace fs = std::filesystem;
void require(bool result, const char* message) { if (!result) throw std::runtime_error(message); }
template<class F> void rejects(F function, const char* message) {
    bool rejected = false; try { function(); } catch (const std::exception&) { rejected = true; }
    require(rejected,message);
}
struct Temporary {
    fs::path directory;
    explicit Temporary(const fs::path& parent) {
        const auto root = fs::absolute(parent).lexically_normal();
        directory = root / ("state-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(directory.parent_path()==root && fs::create_directory(directory),"Cannot create isolated state test folder");
    }
    ~Temporary() { std::error_code ignored; fs::remove_all(directory,ignored); }
};
std::vector<std::uint8_t> bytes(const fs::path& path) {
    std::ifstream input(path,std::ios::binary); require(bool(input),"Missing test fixture");
    return {std::istreambuf_iterator<char>(input),{}};
}
void write(const fs::path& path,const std::vector<std::uint8_t>& data) { bvb::write_atomic_file(path,data.data(),data.size()); }
bvb::SavedState inspect(const fs::path& file,const std::string& cartridge) {
    const auto raw = bytes(file); require(raw.size()>48,"No state header");
    std::size_t length=0; for(int i=0;i<4;++i) length |= std::size_t(raw[12+i]) << (i*8);
    const std::string core(raw.begin()+32,raw.begin()+48);
    return bvb::read_state_file(file,cartridge,core,length);
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Pass core library and output directory"); Temporary temp(fs::u8path(argv[2]));
        const std::string id="0123456789abcdef", core_id="fedcba9876543210";
        bvb::SavedState fixture{id,core_id,{1,2,3,4},bvb::make_test_pattern()};
        const auto file=temp.directory/"fixture.state";
        bvb::write_state_file(file,fixture);
        auto restored=bvb::read_state_file(file,id,core_id,4);
        require(restored.payload==fixture.payload && restored.frame.left==fixture.frame.left
            && restored.frame.right==fixture.frame.right,"State envelope lost payload or stereo image");
        rejects([&]{bvb::read_state_file(file,core_id,core_id,4);},"Wrong game accepted");
        rejects([&]{bvb::read_state_file(file,id,id,4);},"Wrong core build accepted");
        rejects([&]{bvb::read_state_file(file,id,core_id,5);},"Wrong state size accepted");
        auto original=bytes(file), damaged=original; damaged[48]^=1; write(file,damaged);
        rejects([&]{bvb::read_state_file(file,id,core_id,4);},"Corrupt payload accepted");
        damaged=original; damaged.back()^=1; write(file,damaged);
        rejects([&]{bvb::read_state_file(file,id,core_id,4);},"Corrupt checksum accepted");
        damaged=original; damaged.resize(damaged.size()-1); write(file,damaged);
        rejects([&]{bvb::read_state_file(file,id,core_id,4);},"Truncated state accepted");
        damaged=original; damaged.push_back(0); write(file,damaged);
        rejects([&]{bvb::read_state_file(file,id,core_id,4);},"Trailing bytes accepted");
        damaged=original; damaged[8]=2; write(file,damaged);
        rejects([&]{bvb::read_state_file(file,id,core_id,4);},"Unknown format version accepted");
        bvb::write_state_file(file,fixture);
        fixture.payload.clear(); rejects([&]{bvb::write_state_file(file,fixture);},"Empty core state saved");
        require(bytes(file)==original,"Failed save overwrote a valid slot");
        require(bvb::state_slot_path(temp.directory,id,9).filename()==id+"-slot10.state","Slot filename numbering wrong");
        rejects([&]{bvb::state_slot_path(temp.directory,id,10);},"Invalid slot accepted");
        rejects([&]{bvb::state_slot_path(temp.directory,"../other",0);},"Invalid cartridge path accepted");

        std::vector<std::uint8_t> rom(65536), seed_a(65536,0xA5), seed_b(65536,0x5A);
        const auto rom_file=temp.directory/fs::u8path("blank-\xc3\xa9.vb"); write(rom_file,rom);
        const auto cartridge=bvb::cartridge_id(rom);
        const auto saves=temp.directory/"ram";
        const auto ram=saves/(cartridge+".srm"), states=temp.directory/"states";
        const auto slot_a=bvb::state_slot_path(states,cartridge,0), slot_b=bvb::state_slot_path(states,cartridge,1);
        const auto library=fs::u8path(argv[1]);
        write(ram,seed_a);
        {
            bvb::CoreHost host; host.initialize(library); host.load_game(rom_file,saves);
            for(int i=0;i<5;++i) host.run_frame();
            host.save_state(slot_a); require(host.frame_count()==5,"Saving advanced emulation");
        }
        const auto state_a=inspect(slot_a,cartridge);
        write(ram,seed_b);
        {
            bvb::CoreHost host; host.initialize(library); host.load_game(rom_file,saves);
            for(int i=0;i<7;++i) host.run_frame(); host.save_state(slot_b);
            host.load_state(slot_a); require(host.frame_count()==7,"Loading advanced or rewound host frame counter");
            require(host.frame().left==state_a.frame.left && host.frame().right==state_a.frame.right
                && host.audio().empty(),"Load did not restore paused stereo image and clear PCM");
            host.save_ram(); require(bytes(ram)==seed_a,"Real core state did not restore cartridge RAM");
            host.run_frame(); require(host.frame_count()==8 && !host.audio().empty(),"Core failed to resume after restore");
            host.load_state(slot_b); host.save_ram(); require(bytes(ram)==seed_b,"Second slot did not restore independent checkpoint");
            const auto count=host.frame_count();
            damaged=bytes(slot_a); damaged[48]^=1; const auto invalid=states/"invalid.state"; write(invalid,damaged);
            rejects([&]{host.load_state(invalid);},"Host accepted a corrupt state");
            host.save_ram(); require(bytes(ram)==seed_b && host.frame_count()==count,"Rejected file altered current game");
            auto wrong=state_a; wrong.core=core_id; bvb::write_state_file(invalid,wrong);
            rejects([&]{host.load_state(invalid);},"Host accepted another core build");
            wrong=state_a; wrong.cartridge=id; bvb::write_state_file(invalid,wrong);
            rejects([&]{host.load_state(invalid);},"Host accepted another cartridge");
            wrong=state_a;
            const std::string section="V810";
            auto found=std::search(wrong.payload.begin(),wrong.payload.end(),section.begin(),section.end());
            require(found!=wrong.payload.end(),"CPU section missing from actual core snapshot");
            *found='X'; bvb::write_state_file(invalid,wrong); // Valid envelope; missing CPU section forces core rejection after MAIN RAM loads.
            bool rejected=false;
            try {host.load_state(invalid);} catch(const bvb::StateRecoveryError&) {throw;}
            catch(const std::runtime_error&) {rejected=true;}
            require(rejected,"Core accepted state with missing CPU section");
            host.save_ram(); require(bytes(ram)==seed_b,"Failed core load did not roll back partially restored RAM");
            host.run_frame(); require(host.frame_count()==count+1,"Core failed to resume after rollback");
        }
        for(const auto& entry:fs::recursive_directory_iterator(temp.directory))
            require(entry.path().filename().string().find(".tmp-")==std::string::npos,"State temporary file leaked");
        std::cout<<"State format, identity, corruption, slots, actual core restore across sessions, RAM and rollback passed.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n'; return 1;}
}
