#include "custom_battle_state.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
static void check(bool ok) { if(!ok) { std::fputs("battle state test failed\n",stderr); std::exit(1); } }
int main() {
    using namespace swordcraft3;
    std::array<std::uint8_t,0x8000> ram{};
    auto put16=[&](unsigned a,unsigned n){ram[a]=n;ram[a+1]=n>>8;};
    auto put32=[&](unsigned a,unsigned n){put16(a,n);put16(a+2,n>>16);};
    put32(0x6ac0,0x03000000); put32(0x6ab4,4); put32(0x1a90,1);
    put16(0x1d2e,0x470b); put16(0x1d30,18); ram[0xc]=2;
    BattleState s;
    check(read_battle_state(ram.data(),ram.size(),0x03001a90,0,s));
    check(battle_state_supported(s));
    check(s.phase==4 && s.mode==2 && s.arena==0 && s.top_switch==18);
    check(!read_battle_state(nullptr,ram.size(),0x03001a90,0,s));
    check(!read_battle_state(ram.data(),0x6ac3,0x03001a90,0,s));
    check(!read_battle_state(ram.data(),ram.size(),0x03001a94,0,s));
    put32(0x6ac0,0x02000000);
    check(!read_battle_state(ram.data(),ram.size(),0x03001a90,0,s));
    put32(0x6ac0,0x03000000);
    // All menu substates and all six traced pause extents retain ownership.
    for(unsigned arena:{0u,3u}) for(unsigned mode=0;mode<8;++mode)
        for(unsigned pause=0;pause<4;++pause) for(unsigned top=18;top<=58;top+=8) {
            ram[0x1a94]=arena;ram[0xc]=mode;ram[0xf]=pause;put16(0x1d30,top);
            check(read_battle_state(ram.data(),ram.size(),0x03001a90,0,s));
            check(battle_state_supported(s));
        }
    s.arena=1; check(!battle_state_supported(s)); s.arena=0;
    s.enabled=false; check(!battle_state_supported(s)); s.enabled=true;
    s.variant=3; check(!battle_state_supported(s)); s.variant=0;
    s.top_switch=19; check(!battle_state_supported(s)); s.top_switch=18;
    s.hud_cnt=1; check(!battle_state_supported(s)); s.hud_cnt=0;
    BattleStateTracker tracker; BattleState latched;
    ram[0x1a94]=0;
    auto latch=[&](){return tracker.latch(ram.data(),ram.size(),latched);};
    check(!latch()); tracker.observe(s); check(latch());
    check(latched.top_switch==18); check(latch()); // a held video frame is still battle
    put32(0x6ab4,8); check(!latch()); // explicit teardown cancels ownership
    put32(0x6ab4,4); check(!latch()); // stale RAM alone cannot re-enter
    tracker.observe(s); check(latch()); ram[0x1a94]=3; check(!latch());
    ram[0x1a94]=0; tracker.observe(s); tracker.reset(); check(!latch());
    check(!battle_lifecycle_owns_scene(nullptr,ram.size(),s));
    check(!battle_lifecycle_owns_scene(ram.data(),0x6ac3,s));
    // Assertions concern authored map entries/schedule, never rendered pixels.
    for(unsigned end=19;end<=59;end+=8) {
        check(battle_hud_margin_entry(end-4,end)==0x4080);
        for(unsigned y=end-3;y<end;++y) check(battle_hud_margin_entry(y,end)==0x48a6);
        for(unsigned y=125;y<128;++y) check(battle_hud_margin_entry(y,end)==0x40a6);
        check(battle_hud_margin_entry(128,end)==0x4080);
    }
    std::vector<std::uint8_t> rom(0x5e788);
    constexpr std::uint8_t sig[]{0xf0,0xb5,0x06,0x1c,0x32,0x79,0xd0,0x00};
    check(!battle_hook_rom_supported(rom.data(),rom.size()));
    std::memcpy(rom.data()+0x31bc8,sig,sizeof(sig));
    check(!battle_hook_rom_supported(rom.data(),rom.size()));
    constexpr std::uint8_t result_sig[]{0x30,0xb5,0x8e,0xb0,0x01,0x20,0xa7,0xf7};
    std::memcpy(rom.data()+0x5e780,result_sig,sizeof(result_sig));
    check(battle_hook_rom_supported(rom.data(),rom.size()));
    check(!battle_hook_rom_supported(rom.data(),rom.size()-1));
    check(!battle_hook_rom_supported(nullptr,rom.size()));
    std::puts("battle ownership, menu-state independence, reset and HUD schedule: PASS");
}
