#pragma once
#include "battle_follow_camera.h"

namespace swordcraft3 {
// Optional aesthetic mask, NOT an assertion that these are opaque-art bounds.
// Cover 32 authored-source columns at either end of the rocky arena, projected
// through the owned, jump-independent upper-band scroll. Clamp the mask OUT of
// the original 240-column view: original actors/effects must remain visible.
// All layers outside the resulting opening get the existing border treatment.
inline BattleFraming battle_scenery_cover(const BattleFollowResult& follow,
        unsigned width,unsigned arena,bool enabled) {
    auto f=follow.framing;
    if(!enabled || arena!=3 || !follow.active || !f.bounded ||
       (width!=284 && width!=384) || f.begin>f.anchor ||
       f.anchor+240>f.end || f.end>width) return f;
    constexpr int inset=32,source_width=384;
    const int projected_left=int(f.anchor)+inset-follow.h_flat;
    const int projected_right=int(f.anchor)+source_width-inset-follow.h_flat;
    f.begin=unsigned(std::clamp(projected_left,int(f.begin),int(f.anchor)));
    f.end=unsigned(std::clamp(projected_right,int(f.anchor+240),int(f.end)));
    // Do not change anchor/origin/OAM unwrap or feed these bounds back into the
    // camera or submission hooks. This is only a final, reversible cover.
    return f;
}
}
