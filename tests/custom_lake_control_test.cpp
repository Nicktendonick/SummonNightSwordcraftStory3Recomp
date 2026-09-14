#include "custom_lake_control.h"
#include "custom_field_events.h"
#include "custom_field_objects.h"
#include <array>
#include <cstdio>
#include <vector>

int main() {
    using swordcraft3::field_rect_visible;
    using swordcraft3::field_draw_visible_bit;
    if(swordcraft3::field_object_x(377,384)!=-135 || swordcraft3::field_object_x(511,284)!=-1 ||
       swordcraft3::field_object_x(300,384)!=300) return 22;
    if(!field_rect_visible(256,40,272,72,384) || !field_rect_visible(-70,40,-54,72,384) ||
       field_rect_visible(320,40,336,72,384) || field_rect_visible(-100,40,-80,72,384) ||
       field_rect_visible(256,40,272,72,240) || field_rect_visible(10,170,20,185,384) ||
       field_rect_visible(20,40,10,72,384) || field_rect_visible(256,40,272,72,999)) return 20;
    if(field_draw_visible_bit(0x0809FD38)!=4 || field_draw_visible_bit(0x080A0070)!=2 ||
       field_draw_visible_bit(0x0809F01E)!=4 || field_draw_visible_bit(0x0809FD6E)!=4 ||
       field_draw_visible_bit(0x0809FD6C)!=0 || field_draw_visible_bit(0x0809FD70)!=0 ||
       field_draw_visible_bit(0x080A5888)!=0) return 21;
    // A hidden shadow must stay hidden; widening only supplies visibility.
    for(unsigned flags=0;flags<256;++flags) {
        const unsigned extended=flags|field_draw_visible_bit(0x0809FD6E);
        if((extended&~4u)!=(flags&~4u) || ((extended&0x24)==0x24)!=bool(flags&0x20)) return 23;
    }
    for(unsigned lr : {0x0809FDB5u,0x0809FDD5u,0x0809FDFDu,0x0809FE17u,0x0809FE31u,0x0809FE4Bu,0x0809FE65u}) {
        if(!swordcraft3::field_shadow_position_caller(lr) || swordcraft3::field_shadow_position_caller(lr-1) ||
           swordcraft3::field_shadow_position_caller(lr+2)) return 24;
    }
    if(swordcraft3::field_shadow_position_caller(0) || swordcraft3::field_shadow_position_caller(0x0809FC7B)) return 25;
    std::array<std::uint8_t,0x40000> e{};
    std::array<std::uint8_t,0x8000> i{};
    std::vector<std::uint8_t> rom(0xbd505c+20);
    rom[0xbd505c+17]=2; // 512 bytes = two contiguous allocation blocks
    e[0xab8+0x14]=e[0xab8+0x15]=255;
    auto capacity=[&] { return swordcraft3::field_npc_resource_capacity(e.data(),e.size(),rom.data(),rom.size(),0,0xab8); };
    if(!capacity()) return 30;
    for(unsigned n=0;n<64;++n) e[0x618+n]=(n%2)?0:1;
    if(capacity()) return 31; // Fragmented free space is not enough.
    e[0x618]=0; if(!capacity()) return 32;
    for(unsigned n=0;n<16;++n) e[0x7b8+n*4+2]=1;
    if(capacity()) return 33;
    e[0x7ba]=0;
    for(unsigned n=0;n<10;++n) e[0x211c+n*36]=1;
    if(capacity()) return 34;
    e[0xab8+0x14]=e[0xab8+0x15]=0;e[0x7ba]=1;
    if(!capacity()) return 35; // Retain a resident NPC even when pools are full.
    e[0xab8+0x14]=16;if(capacity()) return 36;
    e.fill(0);
    auto allowed=[&] { return swordcraft3::lake_player_control(e.data(),e.size(),i.data(),i.size()); };
    if(allowed()) return 1;
    i[0x6B55]=0xE0; i[0x6B57]=2; e[0xE000]=1; // field pointer 0x0200E000
    if(!allowed()) return 2;
    for(unsigned flags : {0u,4u,5u,0x1001u,0x1005u}) {
        e[0xE000]=flags&255; e[0xE001]=flags>>8;
        if(allowed()) return 3;
    }
    // A complete event, including box-free gaps, holds the native framing.
    for(unsigned frame=0;frame<300;++frame) {
        e[0xE000]=4; e[0xE001]=0;
        if(allowed()) return 4;
    }
    e[0xE000]=1;
    if(!allowed()) return 5;
    e[0xE000]=0x21; // Unrelated draw flags must not keep control locked.
    if(!allowed()) return 6;
    if(swordcraft3::lake_player_control(nullptr,e.size(),i.data(),i.size()) ||
       swordcraft3::lake_player_control(e.data(),0xE001,i.data(),i.size()) ||
       swordcraft3::lake_player_control(e.data(),e.size(),i.data(),0x6B57)) return 7;
    i[0x6B54]=1; if(allowed()) return 8; // Misaligned pointer.
    i[0x6B54]=0; i[0x6B56]=4; if(allowed()) return 9; // Outside EWRAM.
    using swordcraft3::chief_ambient_event_state;
    if(!chief_ambient_event_state(4,201,0x02006000,1,5,0x033c,0x02006cde) ||
       !chief_ambient_event_state(4,201,0x02006000,1,2,0x0001,0x02006cee)) return 10;
    for(unsigned flags : {0u,1u,5u,0x14u,0x1004u,0x10000u})
        if(chief_ambient_event_state(flags,201,0x02006000,1,5,0x033c,0x02006cde)) return 11;
    if(chief_ambient_event_state(4,205,0x02006000,1,5,0x033c,0x02006cde) ||
       chief_ambient_event_state(4,201,0x02006002,1,5,0x033c,0x02006cde) ||
       chief_ambient_event_state(4,201,0x02006000,3,5,0x033c,0x02006cde) ||
       chief_ambient_event_state(4,201,0x02006000,1,2,0x033c,0x02006cde) ||
       chief_ambient_event_state(4,201,0x02006000,1,5,0x041d,0x02006cde) ||
       chief_ambient_event_state(4,201,0x02006000,1,5,0x033c,0x02006cdc)) return 12;
    std::puts("Lake control gate: pass");
}
