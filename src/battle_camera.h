#pragma once
#include "sha1.h"
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace swordcraft3 {
// Presentation framing only. sub_08031810 clamps the guest camera to
// [0, descriptor.width - 256]; the union of its 240-column views is width-16.
// Do NOT change the guest camera, actor positions, collision, or parallax.
// All captured gameplay layers/OBJ/windows move together; HUD stays centered.
struct BattleFraming {
    unsigned anchor=0, begin=0, end=0;
    int origin=0;
    bool bounded=false;
    // A fixed, proven OAM submission interval for Follow. Zero retains the
    // original per-anchor decoder. Applies to gameplay rows even on fallback.
    unsigned object_wrap=0;
};
enum class BattleEdgeBand { None, Matte, Trim, Shadow };
struct BattleEdgeColumn {
    BattleEdgeBand band=BattleEdgeBand::None;
    unsigned depth=0; // Shadow distance from the scene, never a scene sample.
};
// Decoration occupies only pre-existing padding. Distance is measured inward
// from either scene boundary so both edges use the same scene-facing profile.
inline BattleEdgeColumn battle_edge_column(unsigned width,const BattleFraming& f,unsigned x) {
    if(!f.bounded || x>=width || f.begin>=f.end || f.end>width) return {};
    unsigned distance=0,pad=0;
    if(x<f.begin) { distance=f.begin-1-x; pad=f.begin; }
    else if(x>=f.end) { distance=x-f.end; pad=width-f.end; }
    else return {};
    // Retain a matte column even for tiny pads; never borrow scenery space.
    const unsigned shadow=std::min(3u,pad>2?pad-2:0u);
    if(distance<shadow) return {BattleEdgeBand::Shadow,distance};
    if(pad>=2 && distance==shadow) return {BattleEdgeBand::Trim,0};
    return {BattleEdgeBand::Matte,0};
}
inline std::uint16_t battle_edge_color(BattleEdgeColumn column) {
    // GBA 5-bit RGB, deliberately restrained beside the original HUD. The
    // dark-to-warm shadow ramp lives outside the terrain, not over sprites.
    constexpr auto rgb=[](unsigned r,unsigned g,unsigned b) {
        return std::uint16_t(r|(g<<5)|(b<<10));
    };
    if(column.band==BattleEdgeBand::Trim) return rgb(20,14,6);
    if(column.band==BattleEdgeBand::Shadow) {
        constexpr std::uint16_t ramp[]={rgb(6,4,3),rgb(4,3,2),rgb(3,2,1)};
        return ramp[std::min(column.depth,2u)];
    }
    return rgb(9,6,4);
}
inline BattleFraming battle_framing(unsigned width,int camera,unsigned arena_width,bool enabled) {
    BattleFraming f{width>=240?(width-240)/2:0,0,width,0,false};
    if(!enabled || width<=240 || width>384 || arena_width<256 || arena_width>512) return f;
    const int maximum=int(arena_width)-256;
    // Camera locks, shakes beyond the normal range, and unknown states retain
    // their original framing rather than suppressing an authored effect.
    if(camera<0 || camera>maximum) return f;
    const unsigned span=arena_width-16, shown=std::min(width,span);
    f.begin=(width-shown)/2; f.end=f.begin+shown;
    f.origin=std::clamp(camera-int((shown-240)/2),0,int(span-shown));
    f.anchor=f.begin+unsigned(camera-f.origin); f.bounded=true;
    return f;
}
inline bool battle_camera_rom_supported(const std::uint8_t* rom,std::size_t size) {
    // Checked against both Japanese and released 1.0.5.f inputs. Never infer
    // camera limits from artwork, the VRAM ring size, or a different revision.
    return rom && size>=0xb80418 &&
        gba::sha1(rom+0x31810,0x31a30-0x31810).hex()=="cc59f5d2a4bf4bbe4e47c29460ac94c30155e0c0" &&
        gba::sha1(rom+0xb801cc,0xb80418-0xb801cc).hex()=="1c5a70510935041853c326a04ead0de20ddb5704";
}
}
