#pragma once
#include <algorithm>
#include <cstdint>

namespace swordcraft3 {
// Arena ownership is independent of effect storage. Called only after the
// verified battle scheduler/lifecycle and arena guards; the complete raster
// must still satisfy the HUD schedule and finite scenery source-row bounds.
// Mode 0/1 retain regular BG0/1. BG2 and scripted BG3 may change independently.
inline bool battle_arena_layout(unsigned display,unsigned bg0,unsigned bg1) {
    return (display&0x0386)==0x0300 && (display&7)<=1 &&
           bg1==0x450b && (bg0==0 || bg0==0x470b);
}

// Historical combined whitelist, retained for the existing register regression
// suite. CustomBattleScene no longer uses it to decide arena eligibility:
// battle_arena_layout and battle_effect_policy are the independent decisions.
// Spell setup/cleanup keeps the regular
// 512-wide descriptor installed across BG2 disable/re-enable. That canvas is
// native-only unless its signed script window is authenticated separately;
// it does not revoke the arena's independent scenery schedule.
inline bool battle_canvas_layout(unsigned display,unsigned bg0,unsigned bg1,unsigned bg2) {
    const bool regular_spell=(display&7)==0 && bg2==0x4305;
    // 0803BFFC (effect kinds 9..12, including Dark Hole) installs a
    // non-wrapping 128x128 affine canvas. The existing affine renderer owns
    // its transform/bounds; it must not use the text-effect repeat policy.
    // 0803BF6C removes coordinate windows. Keep this exception Mode 1 only,
    // including captured rows where BG2 is disabled during setup/retirement.
    const bool small_affine_spell=(display&0xe007)==1 && bg2==0x0385;
    return ((display&0x0b86)==0x0300 && (display&7)<=1 &&
            (bg2==0x0305 || bg2==0x4385 || regular_spell || small_affine_spell)) &&
           bg1==0x450b && (bg0==0 || bg0==0x470b);
}

enum class BattleEffectPolicy : unsigned { native_only, regular_single, signed_regular, bounded_affine };
// Role-based authorization, never a spell-name or HUD-color test. Unknown
// descriptors/windows cannot gain margin samples just because the arena is
// valid. A disabled layer remains disabled by the compositor's DISPCNT gate.
inline BattleEffectPolicy battle_effect_policy(unsigned display,unsigned bg2,bool signed_window) {
    if((display&7)==0) {
        if(signed_window && (bg2==0x0305 || bg2==0x4305))
            return BattleEffectPolicy::signed_regular;
        if(bg2==0x0305 || bg2==0x4385) return BattleEffectPolicy::regular_single;
    } else if((display&7)==1) {
        // Existing critical canvas and its setup/teardown descriptor.
        if(bg2==0x4385 || bg2==0x0305) return BattleEffectPolicy::bounded_affine;
        // 0803BB88 / 0803BFFC: 128px, priority 1; 0803AFA8: priority 2.
        // Captured transforms are authoritative. No wrapping, and unknown
        // coordinate/OBJ windows keep the effect native-only.
        if(!(display&0xe000) && (bg2==0x0385 || bg2==0x0386))
            return BattleEffectPolicy::bounded_affine;
    }
    return BattleEffectPolicy::native_only;
}
inline unsigned battle_margin_layer_mask(unsigned display,unsigned bg2,
                                         bool signed_window,bool hud) {
    // BG3's scripted placement/window lifetime is not yet authenticated.
    // Native composition remains intact; do not repeat it into the wings.
    return 0x13u | ((!hud && battle_effect_policy(display,bg2,signed_window)!=
                    BattleEffectPolicy::native_only) ? 4u : 0u);
}

// Reviewed critical-hit capture 9933: Mode 1 keeps the normal text scenery
// and HUD, but BG2 becomes a non-wrapping 256px affine impact canvas. Do not
// generalize this exception to arbitrary affine screens or wrapped effects.
inline bool battle_critical_layout(std::uint16_t dispcnt, std::uint16_t bg0,
                                   std::uint16_t bg1, std::uint16_t bg2) {
    return (dispcnt & 0xEF87u) == 0x0701u && bg0 == 0u &&
           bg1 == 0x450Bu && bg2 == 0x4385u;
}

// BG2 is a single attack canvas, not scenery. Choose the unwrapped copy with
// greatest native coverage; ties are ambiguous and must not extend margins.
inline bool battle_effect_contains(int x, unsigned hofs, unsigned span) {
    if (!span || span > 256) return false;
    const int a = (256 - static_cast<int>(hofs & 255u)) & 255;
    const int b = a - 256;
    auto overlap = [span](int origin) {
        return std::max(0, std::min(240, origin + static_cast<int>(span)) -
                           std::max(0, origin));
    };
    const int oa = overlap(a), ob = overlap(b);
    if (oa == ob) return false;
    const int origin = oa > ob ? a : b;
    return x >= origin && x < origin + static_cast<int>(span);
}

inline unsigned battle_repeat_columns(const std::uint64_t* columns, unsigned count) {
    if (!columns || !count || count > 64) return 0;
    // Require at least eight matching columns beyond the candidate period.
    for (unsigned period = 1; period + 8 <= count; ++period) {
        bool match = true;
        for (unsigned x = period; x < count; ++x)
            if (columns[x] != columns[x % period]) { match = false; break; }
        if (match) return period;
    }
    return count;
}

// Continue only the far backdrop through its verified cycle/authored strip,
// not through the unused padding of its power-of-two allocation.
inline int battle_backdrop_x(int x, unsigned hofs, unsigned span) {
    if (!span) return x;
    int texture = (x + static_cast<int>(hofs)) % static_cast<int>(span);
    if (texture < 0) texture += static_cast<int>(span);
    return texture - static_cast<int>(hofs);
}
} // namespace swordcraft3
