#include "custom_field_source.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>

using namespace swordcraft3;
void put16(std::vector<std::uint8_t>& b,unsigned off,unsigned n) { b[off]=n; b[off+1]=n>>8; }
void put32(std::vector<std::uint8_t>& b,unsigned off,unsigned n) { put16(b,off,n); put16(b,off+2,n>>16); }
std::vector<std::uint8_t> literal(const std::vector<std::uint8_t>& data) {
    std::vector<std::uint8_t> out(4); put32(out,0,0x10|(data.size()<<8));
    for(unsigned n=0;n<data.size();++n) { if(!(n%8)) out.push_back(0); out.push_back(data[n]); }
    return out;
}
int main() {
    std::vector<std::uint8_t> e(0x40000),i(0x8000),rom(4096);
    put32(i,0x699c,0x02000800); put32(i,0x6b54,0x0200e000);
    put32(e,0xa00,0x02000800); put16(e,0xa04,1); put16(e,0x800,0x8000);
    put32(e,0x808,0x08093995); put16(e,0xe000,1);
    FieldOwner state;
    assert(read_field_owner(e,i,state) && state.free_control());
    for(unsigned flags:{0u,4u,5u,0x1001u}) {
        put16(e,0xe000,flags); assert(read_field_owner(e,i,state) && !state.free_control());
    }
    put16(e,0xe000,1);
    for(unsigned flags:{0u,0x8800u,0x8040u,0x8080u}) {
        put16(e,0x800,flags); assert(!read_field_owner(e,i,state));
    }
    put16(e,0x800,0x8000);
    put32(e,0x81c,0x02000800); assert(!read_field_owner(e,i,state)); put32(e,0x81c,0);
    put16(e,0xa04,2); assert(!read_field_owner(e,i,state)); put16(e,0xa04,1);
    put32(e,0x808,0x0802b95d); assert(!read_field_owner(e,i,state)); put32(e,0x808,0x08093995);
    assert(read_field_owner(e,i,state));
    put32(i,0x2974,0x08000000); put32(rom,24,4); put32(rom,72,4);
    std::vector<std::uint8_t> data(34);
    put16(data,16,0x4000); put16(data,20,8); put16(data,22,8); put32(data,28,32); put16(data,32,123);
    auto packed=literal(data); std::copy(packed.begin(),packed.end(),rom.begin()+128);
    for(unsigned bg=0;bg<3;++bg) {
        unsigned d=0x2a54+bg*0x34;
        put16(i,d,0x4000); put16(i,d+4,8); put16(i,d+6,8); put32(i,d+0x1c,0x02010000);
    }
    put16(e,0x10000,123);
    FieldSourceCache cache;
    assert(cache.capture(e,i,rom,state) && cache.decodes==1);
    assert(cache.capture(e,i,rom,state) && cache.decodes==1);
    // Same dimensions and pointers, different RAM: never authorize cached art.
    put16(e,0x10000,124); assert(!cache.capture(e,i,rom,state) && cache.decodes==1);
    put16(e,0x10000,123); assert(cache.capture(e,i,rom,state));
    cache.reset(); assert(cache.capture(e,i,rom,state) && cache.decodes==2);
    // Absent resource fails and is not repeatedly decompressed each frame.
    put16(e,0xe4fc,0xffff); assert(!cache.capture(e,i,rom,state) && cache.decodes==3);
    assert(!cache.capture(e,i,rom,state) && cache.decodes==3);
    put16(e,0xe4fc,0); assert(cache.capture(e,i,rom,state) && cache.decodes==4);
    std::vector<std::uint8_t> out;
    assert(field_lz77(rom,128,out) && out==data);
    std::vector<std::uint8_t> bad{0x10,32,0,0,128,0,0};
    assert(!field_lz77(bad,0,out));
    assert(!field_lz77(FieldBytes{},0,out));
    // No named room profile: independent layer sizes, including more cells
    // than the former 79*77 ceiling. All data here is synthetic, not game art.
    rom.assign(0x20000,0); put32(rom,24,4);
    unsigned packed_at=128,live_at=0x10000;
    for(unsigned bg=0;bg<3;++bg) {
        const unsigned columns=bg==0 ? 128 : 32+bg,rows=bg==0 ? 64 : 25+bg;
        data.assign(32+columns*rows*2,0);
        put16(data,16,0x4000); put16(data,20,columns*8); put16(data,22,rows*8); put32(data,28,32);
        for(unsigned n=32;n<data.size();++n) data[n]=std::uint8_t(n+bg);
        packed=literal(data);
        put32(rom,72+bg*8,(packed_at-64)/16);
        std::copy(packed.begin(),packed.end(),rom.begin()+packed_at);
        std::copy(data.begin()+32,data.end(),e.begin()+live_at);
        put16(e,0xe4fc+bg*0x2c,bg);
        const unsigned d=0x2a54+bg*0x34;
        put16(i,d+4,columns*8); put16(i,d+6,rows*8); put32(i,d+0x1c,0x02000000+live_at);
        packed_at=(packed_at+packed.size()+15)&~15u; live_at+=columns*rows*2;
    }
    cache.reset(); assert(cache.capture(e,i,rom,state));
    assert(cache.maps[0].entries.size()==128*64*2 && cache.maps[1].columns==33 && cache.maps[2].rows==27);
    put16(i,0x2a54+4,240); assert(!cache.capture(e,i,rom,state));
    std::puts("Field owner, bounded decoder, source cache, mutation and reset checks: PASS");
}
