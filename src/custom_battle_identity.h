#pragma once
#include "sha1.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace swordcraft3 {
// The forest gameplay band samples source Y 51..156 (normal VOFS=32;
// paused HUD shifts both screen band and VOFS). Authenticate enclosing tile
// rows 6..19 in each block, not the allocation's unused rows/HUD glyphs.
inline std::string battle_near_identity(const std::uint8_t* vram,std::size_t size) {
    if(!vram || size<0x4500) return {};
    std::array<std::uint8_t,0x700> rows;
    std::memcpy(rows.data(),vram+0x3980,0x380);
    std::memcpy(rows.data()+0x380,vram+0x4180,0x380);
    return gba::sha1(rows.data(),rows.size()).hex();
}
inline bool battle_scenery_rows_equal(const std::uint8_t* a,const std::uint8_t* b,
                                      std::size_t size) {
    if(!a || !b || size<0x4500) return false;
    return !std::memcmp(a+0x2800,b+0x2800,0x1000) &&
           !std::memcmp(a+0x3980,b+0x3980,0x380) &&
           !std::memcmp(a+0x4180,b+0x4180,0x380);
}
inline bool battle_near_row_reviewed(unsigned screen_y,unsigned vofs) {
    const unsigned source_y=(screen_y+vofs)&255u;
    return source_y>=48 && source_y<160;
}
}
