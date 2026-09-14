#pragma once
#include <cstdint>
#include <cstddef>
namespace swordcraft3 {
// Extra characters must not overrun the game's fixed graphics allocation pools.
// Native allocation behavior is unchanged; this only authorizes additional
// offscreen requests, conservatively requiring a spare palette even if a full
// pool might contain a reusable matching palette.
inline bool field_npc_resource_capacity(const std::uint8_t* e,std::size_t es,
        const std::uint8_t* rom,std::size_t rs,unsigned field,unsigned npc) {
    if(!e || !rom || es<0x2284 || field>es-0x2284 || npc<field+0xab8 ||
       npc>=field+0x1538 || (npc-field-0xab8)%0x54) return false;
    auto u16=[](const std::uint8_t* p){return unsigned(p[0])|(unsigned(p[1])<<8);};
    const unsigned resident=u16(e+npc+0x14);
    if(resident!=0xffff) return resident<16 && (u16(e+field+0x7b8+resident*4+2)&1);
    bool slot=false,palette=false;
    for(unsigned i=0;i<16;++i) if(!(u16(e+field+0x7b8+i*4+2)&1)) slot=true;
    for(unsigned i=0;i<10;++i) if(!e[field+0x211c+i*36]) palette=true;
    if(!slot || !palette) return false;
    const unsigned graphic=u16(e+npc+0x12);
    if(graphic>=32) return false;
    const unsigned type=u16(e+field+0x658+graphic*16+6);
    const std::size_t descriptor=0xbd505c+std::size_t(type)*20;
    if(descriptor>rs || rs-descriptor<20) return false;
    const unsigned bytes=u16(rom+descriptor+16),needed=(bytes+255)/256;
    const unsigned limit=u16(e+field+8)?24:64;
    if(!needed || needed>limit) return false;
    unsigned run=0;
    for(unsigned i=0;i<limit;++i) {
        run=e[field+0x618+i]?0:run+1;
        if(run>=needed) return true;
    }
    return false;
}
inline int field_object_x(unsigned raw,unsigned width) {
    const unsigned negative_start=512-64-(width-240)/2;
    return raw>=negative_start ? int(raw)-512 : int(raw);
}
// Only graphics residency and draw-list reads receive extended visibility.
// 0809FD6E submits the separate NPC shadow at +0x44. Extend visibility bit 4
// only: the game's shadow-enable bit 0x20, pose and placement remain intact.
// The shared rectangle test and stored gameplay visibility remain unmodified.
inline unsigned field_draw_visible_bit(std::uint32_t pc) {
    return (pc==0x0809FD38u || pc==0x0809FD6Eu || pc==0x0809F01Eu) ? 4u : pc==0x080A0070u ? 2u : 0u;
}
inline bool field_shadow_position_caller(std::uint32_t lr) {
    // Return addresses of the seven shadow pose/placement calls in 0809FB0C.
    switch(lr) {
    case 0x0809FDB5: case 0x0809FDD5: case 0x0809FDFD: case 0x0809FE17:
    case 0x0809FE31: case 0x0809FE4B: case 0x0809FE65: return true;
    default: return false;
    }
}
inline bool field_rect_visible(int left,int top,int right,int bottom,unsigned width) {
    if(width<=240 || width>480 || left>right || top>bottom) return false;
    const int extra=int(width-240)/2;
    return left<=240+extra && right>=-extra && top<=160 && bottom>=0;
}
}
