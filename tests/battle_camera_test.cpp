#include "battle_camera.h"
#include "battle_camera_preferences.h"
#include <chrono>
#include <iostream>
#include <vector>
#define CHECK(x) do { if(!(x)) throw std::runtime_error("Failed: " #x); } while(0)
static int applied=0;
void set_swordcraft3_battle_camera_mode(int value) { applied=value; }
void set_swordcraft3_battle_edge_cover(bool) {}
int main(int argc,char** argv) {
    using namespace swordcraft3;
    for(unsigned width:{240u,284u,384u}) for(unsigned stage:{384u,512u})
        for(int camera=0;camera<=int(stage)-256;++camera) {
            const auto off=battle_framing(width,camera,stage,false);
            CHECK(!off.bounded && off.anchor==(width-240)/2 && off.begin==0 && off.end==width);
            const auto on=battle_framing(width,camera,stage,true);
            CHECK(on.bounded==(width>240));
            if(!on.bounded) continue;
            CHECK(on.origin>=0 && on.origin+int(on.end-on.begin)<=int(stage)-16);
            CHECK(int(on.begin)-int(on.anchor)+camera==on.origin);
            CHECK(int(on.end)-int(on.anchor)+camera==on.origin+int(on.end-on.begin));
            CHECK(on.anchor<=144 && on.begin<on.end && on.end<=width);
            for(unsigned x=0;x<width;++x) {
                const auto edge=battle_edge_column(width,on,x);
                CHECK((edge.band!=BattleEdgeBand::None)==(x<on.begin || x>=on.end));
                const auto mirror=battle_edge_column(width,on,width-1-x);
                CHECK(edge.band==mirror.band && edge.depth==mirror.depth);
                CHECK(battle_edge_column(width,off,x).band==BattleEdgeBand::None);
            }
            // A 64px OBJ cannot have two visible 512-wrapped interpretations.
            CHECK(width+64<=512);
            const int right=int(width-on.anchor)-1,negative=64+int(on.anchor);
            CHECK(right+negative<512);
            for(int x=1-negative;x<=right;++x) {
                const int raw=x&511;
                const int unwrapped=raw>=512-negative?raw-512:raw;
                CHECK(unwrapped==x);
            }
        }
    CHECK(battle_framing(384,0,384,true).anchor==8);
    CHECK(battle_framing(384,128,384,true).anchor==136);
    CHECK(battle_framing(384,64,384,true).anchor==72);
    CHECK(battle_framing(284,0,384,true).anchor==0);
    CHECK(battle_framing(284,128,384,true).anchor==44);
    CHECK(!battle_framing(384,-1,384,true).bounded);
    CHECK(!battle_framing(384,129,384,true).bounded);
    CHECK(!battle_framing(384,0,0,true).bounded);
    // Layout assertions only: no pixel, framebuffer or palette comparisons.
    for(unsigned pad=1;pad<=16;++pad) {
        const BattleFraming f{72,pad,384-pad,0,true};
        unsigned matte=0,trim=0,shadow=0;
        for(unsigned x=0;x<pad;++x) {
            const auto edge=battle_edge_column(384,f,x);
            matte+=edge.band==BattleEdgeBand::Matte;
            trim+=edge.band==BattleEdgeBand::Trim;
            shadow+=edge.band==BattleEdgeBand::Shadow;
        }
        CHECK(matte>=1 && trim==(pad>=2) && shadow==std::min(3u,pad>2?pad-2:0u));
    }
    const BattleFraming odd{72,7,375,0,true};
    for(unsigned distance=0;distance<7;++distance) {
        const auto left=battle_edge_column(383,odd,6-distance);
        const auto right=battle_edge_column(383,odd,375+distance);
        CHECK(left.band==right.band && left.depth==right.depth);
    }
    CHECK(battle_edge_column(383,odd,7).band==BattleEdgeBand::None);
    CHECK(battle_edge_column(383,odd,374).band==BattleEdgeBand::None);
    CHECK(battle_edge_column(383,odd,383).band==BattleEdgeBand::None);
    CHECK(battle_edge_column(384,{0,12,10,0,true},0).band==BattleEdgeBand::None);
    CHECK(!battle_camera_rom_supported(nullptr,0));
    if(argc==2) {
        std::ifstream input(argv[1],std::ios::binary);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
        CHECK(battle_camera_rom_supported(bytes.data(),bytes.size()));
        bytes[0x31810]^=1;
        CHECK(!battle_camera_rom_supported(bytes.data(),bytes.size()));
        bytes[0x31810]^=1; bytes[0xb801d0]^=1;
        CHECK(!battle_camera_rom_supported(bytes.data(),bytes.size()));
    }
    const auto root=std::filesystem::current_path()/ ("camera-prefs-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(std::filesystem::create_directory(root));
    BattleCameraPreferences p; p.load(root); CHECK(!p.mode && p.error.empty());
    CHECK(p.save(true) && applied);
    BattleCameraPreferences q; q.load(root); CHECK(q.mode==1 && q.error.empty());
    CHECK(q.save(false) && !applied); p.load(root); CHECK(!p.mode && p.error.empty());
    { std::ofstream bad(p.path); bad<<"[Launcher]\nbounded_battle_camera=2\n"; }
    p.load(root); CHECK(!p.mode && !p.error.empty()); CHECK(!p.save(true) && !applied);
    CHECK(std::filesystem::remove(p.path));
    CHECK(std::filesystem::remove(root/"Settings"));
    CHECK(std::filesystem::remove(root));
    std::cout<<"PASS: both boundaries, edge layout/symmetry, narrow padding, native/off, shake fallback, authentication, portable preference\n";
}
