#pragma once
#include "sha1.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace swordcraft3 {
enum class BattleArena { unknown, forest, manig_rocks };
// Paired identities are game-owned. Never authenticate a near/far mixture or
// broaden support based only on the shared hardware layout.
inline BattleArena battle_arena_identity(std::string_view near, std::string_view far) {
    if(near=="7f0309c30b95187d7b409e0a5ba7f73b2431694b" &&
       far=="c25d865a7ad8e6c20d9804f6c2aacca32ff09a70") return BattleArena::forest;
    if(near=="a8ce0c8aa57c09f164966847722e2d9c627ac29d" &&
       far=="71979aeb549c54f05620f4fda638614f28684d60") return BattleArena::manig_rocks;
    return BattleArena::unknown;
}
// Grounded gameplay samples Y 51..156, but the airborne camera exposes upper
// rows (observed VOFS=14). Authenticate the complete 0..159 scenery height,
// not just the grounded footprint. HUD glyphs still begin at 0x4500, excluded.
inline std::string battle_near_identity(const std::uint8_t* vram,std::size_t size) {
    if(!vram || size<0x4500) return {};
    std::array<std::uint8_t,0xa00> rows;
    std::memcpy(rows.data(),vram+0x3800,0x500);
    std::memcpy(rows.data()+0x500,vram+0x4000,0x500);
    return gba::sha1(rows.data(),rows.size()).hex();
}
inline bool battle_scenery_rows_equal(const std::uint8_t* a,const std::uint8_t* b,
                                      std::size_t size) {
    if(!a || !b || size<0x4500) return false;
    return !std::memcmp(a+0x2800,b+0x2800,0x1000) &&
           !std::memcmp(a+0x3800,b+0x3800,0x500) &&
           !std::memcmp(a+0x4000,b+0x4000,0x500);
}
inline bool battle_near_row_reviewed(unsigned screen_y,unsigned vofs) {
    const unsigned source_y=(screen_y+vofs)&255u;
    return source_y<160;
}
}
