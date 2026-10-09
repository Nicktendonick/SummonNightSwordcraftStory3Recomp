#pragma once
#include "battle_camera.h"
#include <array>

namespace swordcraft3 {
// Follow is a host translation of all gameplay layers together. No change to
// guest camera, perspective, actors or collision. Bounds come from the owned
// completed BG0 register schedule, not next-frame RAM or sampled tile colors.
struct BattleFollowObjectRange {
    bool enabled=false;
    int left_exclusive=0, right_inclusive=0;
    unsigned wrap_begin=0;
};
inline BattleFollowObjectRange battle_follow_object_range(unsigned width) {
    // At 384, a fixed 346-column opening admits anchors 22..122.
    // All intersecting <=64px OBJ origins occupy [-166,342], 509 integers:
    // no positive X aliases a wrapped negative X in the 9-bit OAM field.
    // The Current fallback interval [-135,311] is a subset of this interval.
    if(width==384) return {true,-167,342,346};
    if(width==284) return {true,-105,280,408};
    return {};
}
inline int battle_follow_object_x(unsigned raw,unsigned width) {
    const auto range=battle_follow_object_range(width);
    raw&=511;
    return range.enabled && raw>=range.wrap_begin ? int(raw)-512 : int(raw);
}
struct BattleFollowResult {
    BattleFraming framing{};
    int h_min=0,h_max=0;
    int h_flat=-1,envelope_min=0,envelope_max=0;
    unsigned aperture=0;
    bool active=false;
};
struct BattleFollowEnvelope {
    int minimum=512,maximum=-512;
    bool valid=false;
};
inline BattleFollowEnvelope battle_follow_envelope(int flat) {
    // Authenticated sub_080352F4: rows below 96 are a flat horizontal band
    // for both d=128/136 and every ordinary vertical camera V=0..32. Its H
    // depends only on horizontal C, so it is an owned-frame, jump-independent
    // key. Integer rounding can alias C values; union ALL candidates and both
    // thresholds instead of guessing a C from next-frame RAM.
    static const auto table=[] {
        std::array<BattleFollowEnvelope,142> result{};
        const auto asr6=[](int n) { return n>=0?n/64:-((-n+63)/64); };
        for(int d:{128,136}) for(int c=0;c<=128;++c) {
            const int h=c+8+asr6((d-152)*(c-64));
            // Across scenery rows <=124 and all V, the schedule's term lies
            // between d-152 and 5. These endpoints bound every native row.
            const int extreme=c+8+asr6(5*(c-64));
            auto& e=result[h];
            e.minimum=std::min(e.minimum,std::min(h,extreme));
            e.maximum=std::max(e.maximum,std::max(h,extreme));
            e.valid=true;
        }
        // Rounding aliases from the two thresholds can make a raw bound
        // retreat by one column as C increases. Expand (never shrink) bounds
        // monotonically so ordinary walking cannot introduce a reverse nudge.
        int upper=-512,lower=512;
        for(auto& e:result) if(e.valid) {
            upper=std::max(upper,e.maximum); e.maximum=upper;
        }
        for(auto it=result.rbegin();it!=result.rend();++it) if(it->valid) {
            lower=std::min(lower,it->minimum); it->minimum=lower;
        }
        return result;
    }();
    return flat>=0 && flat<int(table.size()) ? table[flat] : BattleFollowEnvelope{};
}
inline BattleFollowResult battle_follow_framing(unsigned width,int h_min,int h_max,int h_flat,bool enabled=true) {
    BattleFollowResult out;
    out.framing=battle_framing(width,0,384,false);
    out.h_min=h_min; out.h_max=h_max; out.h_flat=h_flat;
    const auto objects=battle_follow_object_range(width);
    if(!enabled || !objects.enabled) return out;
    out.framing.object_wrap=objects.wrap_begin;
    const auto envelope=battle_follow_envelope(h_flat);
    out.envelope_min=envelope.minimum; out.envelope_max=envelope.maximum;
    // Keep actual completed rows inside the conservative stable envelope.
    // Special/unreviewed schedules fall back without changing OAM decoding.
    if(!envelope.valid || h_min>h_max || h_min<envelope.minimum ||
       h_max>envelope.maximum || h_flat<h_min || h_flat>h_max) return out;
    out.aperture=width==384 ? 346 : 284;
    const int begin=int(width-out.aperture)/2,end=begin+int(out.aperture);
    // source_x = screen_x - anchor + BG0_HOFS[y], source interval [0,384).
    // Intersect all rows and keep the original 240 columns inside the opening.
    // Use the entire possible jump envelope, not this frame's jump extrema.
    const int low=std::max(begin,end+envelope.maximum-384);
    const int high=std::min(end-240,begin+envelope.minimum);
    if(low>high) return out;
    const int anchor=std::clamp(int((width-240)/2),low,high);
    out.framing={unsigned(anchor),unsigned(begin),unsigned(end),0,true,objects.wrap_begin};
    out.active=true;
    return out;
}
inline bool battle_follow_rom_supported(const std::uint8_t* rom,std::size_t size) {
    // These exact releases were audited: all four supported near-map assets
    // declare 384x160, including forest/rocky whose VRAM allocation is 512.
    // Other ROM modifications keep their existing presentation, never guessed
    // extents. This one-time startup check does not run per frame.
    if(!battle_camera_rom_supported(rom,size)) return false;
    const auto digest=gba::sha1(rom,size).hex();
    return digest=="3f5253fcf57e07ce52472bd29a61d16b98a12376" ||
           digest=="06a9f4db52f40a7034dc1c74161a705f30edb858" ||
           digest=="6753a22a096b8adaa3a869333b99fcfe29ba1fec";
}
}
