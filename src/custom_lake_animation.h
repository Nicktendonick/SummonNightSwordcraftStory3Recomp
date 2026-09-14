#pragma once

namespace swordcraft3 {
// Authenticated lake (14-tick) and village (8-tick) four-frame families.
// sub_08005560 stores timer=0 before reloading the duration, then briefly
// stores next=4 before wrapping to zero.
// A frame-start snapshot can observe either intermediate store. Visible tiles
// still have to pass the separate per-placement native-ring reconciliation.
constexpr bool lake_animation_clock_valid(unsigned frames,unsigned next,unsigned timer,unsigned ticks=14) {
    return (ticks==14 || ticks==8) && frames==4 && timer<=ticks &&
        (next<frames || (next==frames && timer==ticks));
}
}
