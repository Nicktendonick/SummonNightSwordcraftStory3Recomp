#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace swordcraft3 {
struct BattleSpellWindow {
    bool valid=false;
    int left=0,right=0,top=0,bottom=0;
    unsigned kind=0;
};
namespace battle_spell_detail {
inline unsigned u16(const std::uint8_t* p) {
    return unsigned(p[0])|(unsigned(p[1])<<8);
}
inline std::uint32_t u32(const std::uint8_t* p) {
    return u16(p)|(std::uint32_t(u16(p+2))<<16);
}
inline int s16(const std::uint8_t* p) {
    const unsigned v=u16(p);
    return v<0x8000 ? int(v) : int(v)-0x10000;
}
inline std::uint64_t code_hash(const std::uint8_t* p,std::size_t size) {
    std::uint64_t h=14695981039346656037ull;
    for(std::size_t i=0;i<size;++i) h=(h^p[i])*1099511628211ull;
    return h;
}
inline unsigned clamped_window(int a,int b) {
    return (unsigned(std::clamp(a,0,255))<<8)|unsigned(std::clamp(b,0,255));
}
inline bool spell_window_permissions(const std::uint8_t* io) {
    // Both callers require WIN0 alone in DISPCNT. WININ's high byte belongs
    // to disabled WIN1, and WINOUT's high byte to disabled OBJ-window. Their
    // stale settings must not decide whether this live WIN0 can be extended.
    return (u16(io+0x48)&0x3f)==0x37 && (u16(io+0x4a)&0x3f)==0x13;
}
}

// Verify the exact guest routines from which the signed-window interpretation
// was derived. These ranges match both the owned JP ROM and English beta.
// This is a bounded code-layout fingerprint, not whole-ROM authentication.
inline bool battle_spell_window_rom_supported(const std::uint8_t* rom,std::size_t size) {
    struct Range { std::size_t offset,size; std::uint64_t hash; };
    constexpr Range ranges[]{
        {0x044e4,0x60,0x500878fdcd64a447ull}, // Display-shadow publication boundary.
        {0x3d5a4,0xe8,0xb07d99708b188df2ull}, // Signed script window equations.
        {0x0493c,0x7c,0x71ff69191085bb4eull}, // WIN0 signed16 -> [0,255].
        {0x062c0,0x20,0x1dcad6af0ec332b7ull}, // BG descriptor dimensions.
        {0x492e0,0x2c,0xb9459c901e9313b7ull}, // Actor-facing query.
        {0x0a7dc,0x30,0xd4e687027db616c1ull}, // Facing flag bit extraction.
        {0x26a44,0xdc,0x7571f713752956d9ull}, // Effect-owner allocation.
        {0x3ce14,0x154,0x236aa2e223ebbc74ull}, // Mode0/BG2 owner dispatch.
        {0x5a51c,0x6c,0x66d5f60ad8fd512bull}, // Script BG retirement/reset.
        {0x06210,0x60,0xa983e7e450424f88ull}, // BG resource retirement.
        {0x05b5c,0x40,0x32d9669b3dcdfbe6ull}, // Clear live BG metadata.
        {0x3b4c4,0x12c,0xf3512b952a15e4a6ull}, // Casting owner/position latch.
        {0x3b5f0,0x32c,0xb0710eeac0f686ccull}, // Casting lifecycle/retirement.
        {0x3b91c,0xac,0x6e6d0e66d58ff4d8ull}, // Kind 3/4 actor WIN0 rectangle.
        {0x3b9c8,0x1c0,0xca3e511272601884ull}, // Regular casting BG2 placement.
        {0x3d68c,0x88,0x426de01f756945d5ull}, // Kind dispatch to window producer.
    };
    if(!rom) return false;
    for(const auto& r:ranges)
        if(r.offset>size || r.size>size-r.offset ||
           battle_spell_detail::code_hash(rom+r.offset,r.size)!=r.hash) return false;
    return true;
}

// Read from state owned by the same captured raster row, never retained guest
// pointers. Caller must first authenticate the ROM and the battle lifecycle.
// Supports the traced type-2 script and kind-3/4 casting window producers.
// Unknown kinds/affine states retain the original hardware window.
inline BattleSpellWindow read_battle_spell_window(
        const std::uint8_t* iwram,std::size_t iwram_size,
        const std::uint8_t* ewram,std::size_t ewram_size,
        const std::uint8_t* io,std::size_t io_size) {
    using namespace battle_spell_detail;
    BattleSpellWindow out;
    if(!iwram || iwram_size<0x6ac4 || !ewram || ewram_size<0x3274 ||
       !io || io_size<0x56) return out;
    if(u32(iwram+0x6ac0)!=0x03000000 || u32(iwram+0x1a90)!=1) return out;
    if(iwram[0x44e]==3 || iwram[0x44e]==4) {
        // 0803B91C publishes WIN0 from the current casting actor, NOT the
        // inactive/stale type-2 script descriptor. It keeps publishing through
        // the BG2-off part of casting; WININ still owns effect/OBJ eligibility.
        const unsigned display=u16(io),cnt=u16(io+0x0c);
        if((display&0xe087)!=0x2000 || ((display&0x400) && cnt!=0x0305) ||
           !spell_window_permissions(io) ||
           u16(io+0x50)!=0x1344) return out;
        const unsigned alpha=u16(io+0x52);
        if((alpha&0xe0e0) || (alpha&31)>16 || ((alpha>>8)&31)>16) return out;
        const auto actor=u32(iwram+0x454);
        if(actor<0x030008c0 || actor>0x03001700 ||
           (actor-0x030008c0)%0x390!=0 || iwram[0x0e]>4 ||
           (actor-0x030008c0)/0x390>iwram[0x0e]) return out;
        const unsigned offset=actor-0x03000000;
        out.left=s16(iwram+offset+0x19a)-56; out.right=out.left+120;
        out.top=s16(iwram+offset+0x19c)-80; out.bottom=out.top+120;
        for(int v:{out.left,out.right,out.top,out.bottom})
            if(v<-32768 || v>32767) return {};
        // 0803B4C4 latches the casting origin, then 0803B9C8 places its BG2
        // canvas. Preserve that captured scroll independently of actor motion.
        if((u16(io+0x18)&511)!=(unsigned(184-s16(iwram+0x450))&511) ||
           (u16(io+0x1a)&511)!=(unsigned(344-s16(iwram+0x452))&511) ||
           clamped_window(out.left,out.right)!=u16(io+0x40) ||
           clamped_window(out.top,out.bottom)!=u16(io+0x44)) return {};
        // Live BG metadata may already be retired while this row still shows
        // the old canvas. Its captured CNT is sufficient here (fixed 256x256).
        // This callback never enables BG2: the captured DISPCNT still owns it.
        out.valid=true; out.kind=iwram[0x44e];
        return out;
    }
    if(iwram[0x44e]!=2 || iwram[0x44f]!=1) return out;
    // sub_08026A44 installs this fixed effect allocation; sub_0803CE14 takes
    // its inline descriptor at +3C and calls sub_0803D5A4 for BG2 in Mode0.
    if(u32(iwram+0x1e44)!=0x02003200) return out;
    const auto* effect=ewram+0x3200;
    const auto* d=effect+0x3c;
    const unsigned display=u16(io),cnt=u16(io+0x0c);
    if((display&0xe487)!=0x2400 || (cnt!=0x0305 && cnt!=0x4305) ||
       !spell_window_permissions(io) ||
       u16(io+0x50)!=0x1344) return out;
    const unsigned alpha=u16(io+0x52);
    if((alpha&0xe0e0) || (alpha&31)>16 || ((alpha>>8)&31)>16) return out;
    if((d[0]&~3u)!=0 || d[1]>2) return out;

    const unsigned map_width=cnt==0x4305 ? 512u : 256u;
    constexpr unsigned map_height=256;
    if((d[0]&1)==0) {
        // 0805A51C retires this script BG before the shadow DISPCNT is applied
        // to the display. It preserves orientation/position/extents, but clears
        // its flag and (through 08006210 -> 08005B5C) live BG metadata. The
        // captured row still owns the old hardware dimensions and WIN0 here.
        // Accept only this exact pending-disable/reset state, not an inactive
        // descriptor generally. All scroll/window equations below must still
        // reproduce the captured registers; no previous-frame state is used.
        if(d[0]!=0 || u16(iwram+0x2990)!=(display&~0x2400u) ||
           u16(d+0x12)!=0x100 || u16(d+0x14)!=0x100 ||
           u16(d+0x16) || u16(d+0x18) || u16(d+0x1a) ||
           u16(d+0x28) || u16(d+0x2a)) return out;
        // 08005B5C clears 0x32 bytes, leaving the final padding halfword alone.
        for(unsigned j=0;j<0x32;++j)
            if(iwram[0x2a88+j]) return out;
    } else if(u16(iwram+0x2a88+4)!=map_width ||
              u16(iwram+0x2a88+6)!=map_height) return out;
    const unsigned w=u16(d+0x0e),h=u16(d+0x10);
    if(!w || w>map_width || !h || h>map_height) return out;
    const int x=s16(d+2),y=s16(d+4);
    if((u16(io+0x18)&511)!=(unsigned(x)&511) ||
       (u16(io+0x1a)&511)!=(unsigned(y)&511)) return out;

    const auto actor=u32(effect+0x38);
    if(actor<0x030008c0 || actor>0x03001700 ||
       (actor-0x030008c0)%0x390!=0) return out;
    const unsigned actor_index=(actor-0x030008c0)/0x390;
    if(iwram[0x0e]>4 || actor_index>iwram[0x0e]) return out;
    const unsigned actor_offset=actor-0x03000000;
    // sub_080492E0 -> sub_0800A7DC(actor+30C): flag bit2 at +318.
    const bool facing=(iwram[actor_offset+0x318]&4)!=0;
    if(d[1]==0) {
        out.left=255-x-int(w); out.right=256-x;
        out.top=-y; out.bottom=int(h)-y;
    } else {
        out.right=int(map_width)-x;
        out.left=facing ? out.right : out.right-int(w)-1;
        if(facing) out.right=out.left+int(w);
        out.top=256-y; out.bottom=out.top+int(h);
    }
    // Guest arguments are truncated to signed16 before clamping. Decline the
    // overflow cases instead of extrapolating a wrapped coordinate to the host.
    for(int v:{out.left,out.right,out.top,out.bottom})
        if(v<-32768 || v>32767) return {};
    if(out.left>=out.right || out.top>=out.bottom ||
       clamped_window(out.left,out.right)!=u16(io+0x40) ||
       clamped_window(out.top,out.bottom)!=u16(io+0x44)) return {};
    out.valid=true; out.kind=2;
    return out;
}

// sub_08001BC0 publishes display/window shadows through 080044E4, then
// scroll shadows through 08004B30, BEFORE advancing input/actors/effects.
// Own the source metadata at that publication boundary. Live RAM observed
// during scanout may already describe the following update. Each raster row
// still has to satisfy every exact register/window check in the decoder.
// This is not a last-good-window cache: every publication replaces it, even
// with an unsupported/retired owner, and reset/restore invalidates it.
class BattleSpellDisplayEpoch {
    std::array<std::uint8_t,0x6ac4> iwram_{};
    std::array<std::uint8_t,0x3274> ewram_{};
    bool published_=false;
public:
    void reset() { published_=false; }
    void publish(const std::uint8_t* iwram,std::size_t iwram_size,
                 const std::uint8_t* ewram,std::size_t ewram_size) {
        published_=iwram && iwram_size>=iwram_.size() &&
                   ewram && ewram_size>=ewram_.size();
        if(!published_) return;
        std::memcpy(iwram_.data(),iwram,iwram_.size());
        std::memcpy(ewram_.data(),ewram,ewram_.size());
    }
    BattleSpellWindow read(const std::uint8_t* iwram,std::size_t iwram_size,
                           const std::uint8_t* ewram,std::size_t ewram_size,
                           const std::uint8_t* io,std::size_t io_size) const {
        // The first partial frame after restoration has no publication yet.
        // Permit only the existing strict, history-free decoder in that case.
        return published_ ? read_battle_spell_window(iwram_.data(),iwram_.size(),
            ewram_.data(),ewram_.size(),io,io_size) :
            read_battle_spell_window(iwram,iwram_size,ewram,ewram_size,io,io_size);
    }
};
}
