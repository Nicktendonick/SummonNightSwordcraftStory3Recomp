#pragma once
#include <algorithm>
#include <cstdint>

namespace swordcraft3 {
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
