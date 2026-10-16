#pragma once
#include "battle_scenery_repeat.h"
#include <initializer_list>

namespace swordcraft3 {
// Repeat scenery special cases (player review, 2026-10-15): the arenas whose
// scenery no single wrap period continues cleanly get a reviewed band
// continuation per side. Every other arena keeps the planner's own plan, and
// every floor keeps the planner's repeating floor. Columns refer to the
// exact-ROM arena maps (the same allowlist as Repeat scenery itself).
struct BattleSceneryQuiltSpec {
    bool enabled=false;
    unsigned run_first=0, run_last=0, overlap=0; // see scenery_repeat_apply_quilt
};
struct BattleScenerySpecial {
    SceneryRepeatSequence left{}, right{}; // length 0: the planner's plan
    BattleSceneryQuiltSpec quilt_left{}, quilt_right{};
};

namespace scenery_special_detail {
// Cells in reading order from runs: {first, last} is ascending, {first, last,
// true} the same columns mirrored and read in reverse (a reflection).
struct Run { unsigned first, last; bool flip=false; };
inline SceneryRepeatSequence sequence(unsigned inset,std::initializer_list<Run> runs) {
    SceneryRepeatSequence s; s.inset=inset;
    for(const auto& run:runs) {
        if(run.flip)
            for(unsigned c=run.last+1;c-->run.first && s.length<SceneryRepeatSequence::max_cells;)
                s.cells[s.length++]=std::uint8_t(c|SceneryRepeatSequence::flip);
        else
            for(unsigned c=run.first;c<=run.last && s.length<SceneryRepeatSequence::max_cells;++c)
                s.cells[s.length++]=std::uint8_t(c);
    }
    return s;
}
} // namespace scenery_special_detail

inline BattleScenerySpecial battle_scenery_special(unsigned arena,bool far) {
    using scenery_special_detail::sequence;
    BattleScenerySpecial out;
    if(!far && arena==6) {
        // Ruined village: art columns 3-44 between three filler columns per
        // side. The tower (3-7) and the cobblestone wall (36-44) run into the
        // edges, cut straight where the native screen edge hid them; no
        // column continues either, and any join leaves that cut showing.
        // Both are completed by their reflection instead: the tower mirrors
        // at its outer edge (column 3 is a dark recess, so it becomes one
        // ruin with a doorway in the middle, then the fence and gap), the
        // wall at column 43 (its replaced overscan column 44 is a flat side
        // that would show twice), ping-ponging through the neighbouring art.
        out.left=sequence(3,{{3,21},{3,21,true}});
        out.right=sequence(4,{{22,43,true},{22,43}});
    } else if(!far && arena==7) {
        // Withered desert: arena 3's map entries with withered foliage. Its
        // own edge scores chose a period that cuts the right tree in half;
        // use arena 3's reviewed band (left w41, right w42 from the filler).
        out.left=sequence(3,{{3,43}});
        out.right=sequence(3,{{3,44}});
    } else if(far && arena==13) {
        // Lava crystals backdrop. Right: columns 35-43 continue the orb
        // cluster as authored (they lie beyond the backdrop's own scroll
        // range but are not filler); after them the strip loops from 17.
        // Left: the lava is cut by column 0's edge, and no column or pixel
        // run elsewhere matches that edge. A quilted reflection continues it:
        // reflected for a few pixels, then lava from columns 0-17 (no orb
        // fragments) along the cheapest irregular cut within 40 pixels.
        out.quilt_left={true,0,17,40};
        out.right=sequence(4,{{17,43}});
    } else if(!far && arena==14) {
        // Storm crystals: the orb cluster runs off the map's right edge and
        // no column continues it. Its replaced overscan columns 44-47 give
        // way to a reflection of the cluster at column 43 (orbs reflected
        // through their middle stay round), ping-ponging over 36-43.
        out.right=sequence(4,{{36,43,true},{36,43}});
    }
    return out;
}
} // namespace swordcraft3
