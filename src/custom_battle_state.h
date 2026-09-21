#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace swordcraft3 {
// Decoded from the game's battle owner, not from HUD colors or tile images.
// See docs/BATTLE_STATE_HOOKS.md for the disassembly and TCP trace evidence.
struct BattleState {
    unsigned phase=0, mode=0, pause=0, arena=0, variant=0;
    unsigned top_switch=0, hud_cnt=0, scenery_cnt=0;
    bool enabled=false;
};
inline unsigned battle_read16(const std::uint8_t* p) {
    return unsigned(p[0]) | (unsigned(p[1])<<8);
}
inline std::uint32_t battle_read32(const std::uint8_t* p) {
    return battle_read16(p) | (std::uint32_t(battle_read16(p+2))<<16);
}
inline bool battle_hook_rom_supported(const std::uint8_t* rom,std::size_t size) {
    constexpr std::uint8_t signature[]{0xf0,0xb5,0x06,0x1c,0x32,0x79,0xd0,0x00};
    constexpr std::uint8_t result_signature[]{0x30,0xb5,0x8e,0xb0,0x01,0x20,0xa7,0xf7};
    return rom && size>=0x5e780+sizeof(result_signature) &&
        std::memcmp(rom+0x31bc8,signature,sizeof(signature))==0 &&
        std::memcmp(rom+0x5e780,result_signature,sizeof(result_signature))==0;
}
inline bool read_battle_state(const std::uint8_t* ram,std::size_t size,
        std::uint32_t scene,unsigned variant,BattleState& out) {
    if(!ram || size<0x6ac4 || scene!=0x03001a90 ||
       battle_read32(ram+0x6ac0)!=0x03000000) return false;
    out.phase=battle_read32(ram+0x6ab4);
    out.mode=ram[0xc]; out.pause=ram[0xf]; out.arena=ram[0x1a94];
    out.variant=variant; out.enabled=battle_read32(ram+0x1a90)==1;
    out.hud_cnt=battle_read16(ram+0x1d2c);
    out.scenery_cnt=battle_read16(ram+0x1d2e);
    out.top_switch=battle_read16(ram+0x1d30);
    return true;
}
inline bool battle_state_supported(const BattleState& s) {
    // Do not key eligibility to normal/airborne/ability/pause submodes. The
    // call of the battle raster scheduler is the scene-ownership proof.
    return s.enabled && s.variant!=3 && (s.arena==0 || s.arena==3) &&
        s.hud_cnt==0 && s.scenery_cnt==0x470b &&
        s.top_switch>=18 && s.top_switch<=58 && (s.top_switch-18)%8==0;
}
inline bool battle_lifecycle_owns_scene(const std::uint8_t* ram,std::size_t size,
        const BattleState& observed) {
    if(!ram || size<0x6ac4 || battle_read32(ram+0x6ac0)!=0x03000000) return false;
    // sub_0802B95C: 0..2 setup, 3 intro, 4 combat, 5 result preparation,
    // 6 escape, 7 result, 8 teardown. Variant 11 is scripted combat.
    const unsigned phase=battle_read32(ram+0x6ab4);
    return ((phase>=3 && phase<=7) || phase==11) &&
        ram[0x1a94]==observed.arena && battle_read32(ram+0x1a90)==1;
}
class BattleStateTracker {
    BattleState pending_{};
    bool observed_=false;
public:
    void reset() { pending_={}; observed_=false; }
    void observe(const BattleState& s) { pending_=s; observed_=true; }
    bool latch(const std::uint8_t* ram,std::size_t size,BattleState& out) {
        if(!battle_lifecycle_owns_scene(ram,size,pending_)) reset();
        out=pending_; return observed_;
    }
};
// HUD authored by sub_080331C4: palette bank 4, blank tile 0x80 and
// separator tile 0xA6. Top is vertically flipped; never repeat HUD contents.
inline std::uint16_t battle_hud_margin_entry(unsigned y,unsigned top_end) {
    if(y<top_end && y+3>=top_end) return 0x48a6;
    if(y>=125 && y<128) return 0x40a6;
    return 0x4080;
}
}
