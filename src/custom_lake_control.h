#pragma once
#include <cstddef>
#include <cstdint>

namespace swordcraft3 {
// Used only after an allowlisted profile's source hashes authenticate the field.
// csm3 sub_08093994 reads *gUnk_03006B54 (IWRAM 0x6B54): bit 0 enables
// player-input dispatch, bit 12 blocks it, and bit 2 runs the foreground event
// script (otherwise sub_080123E4 suspends that script). These survive gaps
// between dialogue boxes; VM call depth / current script ID are NOT input flags.
// This is presentation-only: never change these flags or simulate input.
inline bool lake_player_control(const std::uint8_t* ewram,std::size_t es,
                                const std::uint8_t* iwram,std::size_t is,
                                unsigned* observed_flags=nullptr) {
    if(observed_flags) *observed_flags=0x10000; // Invalid/unavailable, not zero.
    if(!ewram || !iwram || is<0x6B58) return false;
    const auto* p=iwram+0x6B54;
    const std::uint32_t address=std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
    if(address<0x02000000u || address>=0x02040000u || (address&3)) return false;
    const std::size_t offset=address-0x02000000u;
    if(offset>es || es-offset<2) return false;
    const unsigned flags=unsigned(ewram[offset])|(unsigned(ewram[offset+1])<<8);
    if(observed_flags) *observed_flags=flags;
    return (flags&0x1005u)==1u;
}
}
