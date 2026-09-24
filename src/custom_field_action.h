#pragma once
#include "custom_field_source.h"

namespace swordcraft3 {
// Presentation permission, NOT player-input permission. Caller authenticates
// the ROM, current field task and map sources first. 080A4FBC tracks control
// ownership in eight actions, independently of the foreground script bit.
// Tool start sets 0x1000 and clears 1; completion reverses these masks.
inline bool field_tool_action(FieldBytes e,const FieldOwner& owner) {
    if((owner.flags&0x1005)!=0x1000 || owner.pointer<0x02000000 || (owner.pointer&3)) return false;
    const std::uint64_t root=owner.pointer-0x02000000;
    if(!field_span(e,root,0x1fbc+8*0x1c) || field16(e.data()+root)!=owner.flags) return false;
    const unsigned tool=e[root+0x1ec7];
    if(tool>6) return false;
    bool found=false;
    for(unsigned n=0;n<8;++n) {
        const auto* action=e.data()+root+0x1fbc+n*0x1c;
        const unsigned active=field16(action+0x16);
        if(!(active&1)) continue;
        const unsigned set=field16(action+0x12),clear=field16(action+0x14);
        if(!((set|clear)&0x1005)) continue; // Passive actions do not own this lock.
        if(active!=1 || set!=0x1000 || clear!=1) return false;
        switch(field32(action+0x18)) {
        case 0x0809d859: // 080A4BEC: L/R field-tool selection animation.
            // 0809D858 states 0/1 own this same temporary input lock, including
            // repeated selection changes. A tool strike hands off to its own
            // action; this does not grant permission to foreground scripts.
            if(field16(action)>1) return false;
            break;
        case 0x0809da99: // 080A4C3C: ordinary tool animation, all seven selections.
            if(field16(action)>1) return false;
            break;
        case 0x0809e3ed: // 080A4D08: bow projectile after the initial animation.
            if(tool!=6 || field16(action)>1) return false;
            break;
        case 0x080a0ad1: { // 080A06E0: hit/break/push of an existing field object.
            const unsigned target=field16(action+2);
            if(tool==6 || target>=32 || field16(action)>9) return false;
            const unsigned type=e[root+0x1538+target*0x3c+4];
            if(type<2 || type>9) return false;
            // 080A0AD0/080A1078/080A157C use different substates, up through 9.
            break;
        }
        default: return false; // Includes deferred arbitrary-event callback 0809B849.
        }
        found=true;
    }
    // 0809DA98 retires the draw action before the next update installs
    // 0809E3ED. In that handoff, 080A4724 consumes this exact request phase
    // at field+1EB8+18; bit 0x40 suspends that consumer. Authenticate the
    // pending gameplay state, not a frame-count grace period. Check it only
    // after the action scan so scripts/unknown lock owners still reject.
    const bool bow_pending=tool==6 && !(owner.flags&0x40) &&
        (field16(e.data()+root+0x1ed0)&0x700)==0x100;
    return found || bow_pending;
}
}
