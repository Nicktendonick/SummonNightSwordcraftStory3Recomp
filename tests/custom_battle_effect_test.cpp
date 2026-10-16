// Game-state/register/control-path assertions only; never inspect output RGB.
#include "custom_battle_scene.h"
#include <stdexcept>
#include <vector>

static void check(bool ok) { if(!ok) throw std::runtime_error("battle effect policy assertion"); }
static void put(std::uint8_t* p,unsigned value) { p[0]=value; p[1]=value>>8; }
int main() {
    using namespace swordcraft3;
    try {
        using P=BattleEffectPolicy;
        // Each register combination separates scenery eligibility from effect
        // permission, including disabled/setup/retirement and window changes.
        for(unsigned d=0;d<65536;++d) {
            const bool arena=(d&7)<=1 && !(d&0x80) && (d&0x300)==0x300;
            check(battle_arena_layout(d,0,0x450b)==arena);
            check(battle_arena_layout(d,0x470b,0x450b)==arena);
            check(!battle_arena_layout(d,1,0x450b));
            check(!battle_arena_layout(d,0x470b,0x450a));
        }
        for(unsigned c=0;c<65536;++c) {
            const bool regular=c==0x0305 || c==0x4385;
            check(battle_effect_policy(0x1740,c,false)==(regular?P::regular_single:P::native_only));
            check(battle_effect_policy(0x3740,c,true)==
                ((c==0x0305||c==0x4305)?P::signed_regular:c==0x4385?P::regular_single:P::native_only));
            const bool affine=regular || c==0x0385 || c==0x0386;
            check(battle_effect_policy(0x1741,c,false)==(affine?P::bounded_affine:P::native_only));
            check(battle_effect_policy(0x3741,c,false)==(regular?P::bounded_affine:P::native_only));
            for(unsigned d:{0x1740u,0x1340u,0x1f40u,0x1741u,0x1341u,0x3741u}) {
                const auto mask=battle_margin_layer_mask(d,c,false,false);
                check((mask&0x13)==0x13 && !(mask&8)); // Arena/OBJ preserved; BG3 native only.
                check(battle_margin_layer_mask(d,c,true,true)==0x13); // HUD band, affine included.
            }
        }
#ifdef _WIN32
        _putenv_s("SWORDCRAFT3_CUSTOM_BATTLES","1");
        _putenv_s("SWORDCRAFT3_BATTLE_WINDOW_TRACE","0");
#else
        setenv("SWORDCRAFT3_CUSTOM_BATTLES","1",1);
        setenv("SWORDCRAFT3_BATTLE_WINDOW_TRACE","0",1);
#endif
        auto raster=std::make_unique<gba::GbaRasterCapture>();
        auto ppu=std::make_unique<gba::GbaPpu>();
        std::array<std::uint8_t,0x400> io{},pal{},oam{};
        std::array<std::uint8_t,0x18000> vram{};
        std::vector<std::uint8_t> native(240*160*3),output(384*160*3);
        gbarecomp::ExtendedViewFrameInfo memory{};
        memory.io=io.data();memory.io_size=io.size();
        memory.vram=vram.data();memory.vram_size=vram.size();
        BattleState state{};
        state.enabled=true;state.arena=0;state.hud_cnt=0;state.scenery_cnt=0x470b;state.top_switch=18;
        CustomBattleScene scene;
        auto run=[&](unsigned display,unsigned descriptor,bool owned=true,int bad_row=-1) {
            scene.capture(memory,state,owned); ppu->reset();
            for(unsigned y=0;y<160;++y) {
                put(io.data()+8,(y<19 || y>=125)?0:0x470b);
                put(io.data()+10, int(y)==bad_row?0x440b:0x450b);
                // Include one-row format changes: no persistent latch may
                // promote the next row's unsupported effect.
                put(io.data()+12,y==60?0x2385:descriptor);
                check(raster->capture({ppu.get(),y,std::uint16_t(display),io.data(),vram.data(),oam.data(),pal.data()}));
            }
            return scene.draw(*raster,native.data(),output.data(),384);
        };
        for(unsigned d:{0x1740u,0x1340u,0x1f40u,0x1741u,0x1341u,0x3741u})
            for(unsigned c:{0x0305u,0x4305u,0x4385u,0x0385u,0x0386u,0x2385u,0xffffu}) {
                check(run(d,c));check(!std::strcmp(scene.decline_reason(),"none"));
            }
        check(!run(0x1740,0x0305,false));
        check(!run(0x1740,0x0305,true,71));
        check(!run(0x17c0,0x0305)); // Forced blank cannot become an arena.
        check(!run(0x1640,0x0305)); // Missing scenery remains whole-frame fallback.
        check(!run(0x1742,0x0305)); // Mode 2 removes the arena's text layers.
        state.arena=255;check(!run(0x1740,0x0305));
        std::puts("battle effect families / independent arena / immutable raster / negative scenes: PASS (state only)");
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
