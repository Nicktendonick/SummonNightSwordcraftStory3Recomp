#include "custom_lake_animation.h"
#include "custom_field_profiles.h"
#include <cstdio>
#include <initializer_list>

int main() {
    using swordcraft3::lake_animation_clock_valid;
    // Exhaustive byte-sized counters: ordinary phases, zero during reload,
    // and the one valid next=4 state; everything else fails closed.
    for(unsigned ticks : {8u,14u})
    for(unsigned next=0;next<256;++next) for(unsigned timer=0;timer<256;++timer) {
        const bool expected=(next<4 && timer<=ticks) || (next==4 && timer==ticks);
        if(lake_animation_clock_valid(4,next,timer,ticks)!=expected) return 1;
        for(unsigned frames=0;frames<8;++frames)
            if(frames!=4 && lake_animation_clock_valid(frames,next,timer,ticks)) return 2;
    }
    // Captured failing placement: next=2, timer=0. All four candidates remain
    // available, without ever indexing beyond the ROM animation's four frames.
    for(unsigned next=0;next<=4;++next) {
        unsigned mask=0;
        for(unsigned phase=1;phase<=4;++phase) mask|=1u<<((next+4-phase)%4);
        if(mask!=15) return 3;
    }
    for(unsigned ticks=0;ticks<256;++ticks)
        if(ticks!=8 && ticks!=14 && lake_animation_clock_valid(4,0,0,ticks)) return 4;
    using swordcraft3::custom_field_profile;
    const auto* lake=custom_field_profile(360,320);
    const auto* chief=custom_field_profile(512,320);
    const auto* village=custom_field_profile(512,400);
    const auto* wide_field=custom_field_profile(888,312);
    const auto* large_field=custom_field_profile(632,616);
    if(!large_field || large_field->columns!=79 || large_field->rows!=77 ||
       large_field->animation_ticks!=0 || !large_field->additional ||
       large_field->columns*large_field->rows>swordcraft3::kCustomFieldMaxCells) return 8;
    if(!wide_field || wide_field->columns!=111 || wide_field->rows!=39 ||
       wide_field->animation_ticks!=0 || wide_field->columns*wide_field->rows>swordcraft3::kCustomFieldMaxCells) return 7;
    if(!lake || !chief || !village || lake==chief || chief==village ||
       lake->animation_ticks!=14 || village->animation_ticks!=8) return 5;
    if(custom_field_profile(256,256) || custom_field_profile(512,512) ||
       custom_field_profile(0,0) || custom_field_profile(360,400)) return 6;
    std::puts("Field profiles and animation clocks: pass");
}
