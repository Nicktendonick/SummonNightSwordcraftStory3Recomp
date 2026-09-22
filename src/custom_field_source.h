#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace swordcraft3 {
using FieldBytes=std::span<const std::uint8_t>;
inline unsigned field16(const std::uint8_t* p) { return p[0]|(unsigned(p[1])<<8); }
inline std::uint32_t field32(const std::uint8_t* p) { return field16(p)|(std::uint32_t(field16(p+2))<<16); }
inline bool field_span(FieldBytes b,std::uint64_t off,std::uint64_t size) {
    return off<=b.size() && size<=b.size()-off;
}
struct FieldOwner {
    std::uint32_t pointer=0;
    unsigned flags=0;
    bool free_control() const { return (flags&0x1005)==1; }
};
// 08019688 dispatches the linked 16-record pool initialized by 0801978C.
// A stale field pointer or an unlinked callback is not ownership evidence.
inline bool read_field_owner(FieldBytes e,FieldBytes i,FieldOwner& out) {
    out={};
    if(i.size()<0x6b58 || e.size()<0xa06 || field32(i.data()+0x699c)!=0x02000800) return false;
    unsigned current=field32(e.data()+0xa00),previous=0,seen=0,count=0,owners=0;
    while(current) {
        if(current<0x02000800 || current>=0x02000a00 || (current&31)) return false;
        const unsigned slot=(current-0x02000800)/32;
        if(seen&(1u<<slot)) return false;
        seen|=1u<<slot; ++count;
        const auto* task=e.data()+current-0x02000000;
        if(field32(task+0x18)!=previous) return false;
        const unsigned flags=field16(task);
        if(field32(task+8)==0x08093995 && (flags&0x88c0)==0x8000) ++owners;
        previous=current; current=field32(task+0x1c);
    }
    if(count!=field16(e.data()+0xa04) || owners!=1) return false;
    const unsigned pointer=field32(i.data()+0x6b54);
    if(pointer<0x02000000 || (pointer&3) || !field_span(e,pointer-0x02000000,0x564)) return false;
    out={pointer,field16(e.data()+pointer-0x02000000)};
    return true;
}
inline bool field_archive_member(FieldBytes rom,std::uint64_t base,unsigned index,std::size_t& result) {
    if((base&3) || index>=0xffff || !field_span(rom,base+8+index*8,4)) return false;
    const std::uint64_t offset=std::uint64_t(field32(rom.data()+base+8+index*8))*16;
    if(offset<8+(index+1)*8 || !field_span(rom,base+offset,4)) return false;
    result=std::size_t(base+offset); return true;
}
inline bool field_lz77(FieldBytes rom,std::size_t pos,std::vector<std::uint8_t>& data) {
    data.clear();
    if(!field_span(rom,pos,4) || rom[pos]!=0x10) return false;
    const unsigned size=field32(rom.data()+pos)>>8; pos+=4;
    if(size<32 || size>0x40000) return false;
    data.reserve(size);
    while(data.size()<size) {
        if(!field_span(rom,pos,1)) return false;
        unsigned flags=rom[pos++];
        for(unsigned bit=0;bit<8 && data.size()<size;++bit,flags<<=1) {
            if(flags&128) {
                if(!field_span(rom,pos,2)) return false;
                const unsigned a=rom[pos++],b=rom[pos++];
                const unsigned count=(a>>4)+3,distance=((a&15)<<8)+b+1;
                if(distance>data.size() || count>size-data.size()) return false;
                for(unsigned n=0;n<count;++n) data.push_back(data[data.size()-distance]);
            } else {
                if(!field_span(rom,pos,1)) return false;
                data.push_back(rom[pos++]);
            }
        }
    }
    return true;
}
struct FieldSourceMap {
    unsigned columns=0,rows=0;
    std::vector<std::uint8_t> entries;
};
// One scene's immutable authored data, not a history of previously seen tiles.
// ROM decompression occurs only on key changes. Loaded sources are compared
// each capture so same-address RAM mutations cannot silently reuse the cache.
class FieldSourceCache {
    std::array<std::uint32_t,5> key_{};
    const std::uint8_t* rom_=nullptr;
    std::size_t rom_size_=0;
    bool attempted_=false,decoded_=false;
public:
    std::array<FieldSourceMap,3> maps;
    unsigned decodes=0;
    const char* reason="field-source-unread";
    void reset() { attempted_=decoded_=false; key_={}; }
    bool matches_key(FieldBytes e,FieldBytes i,const FieldOwner& owner) const {
        return attempted_ && key_==key(e,i,owner);
    }
    static std::array<std::uint32_t,5> key(FieldBytes e,FieldBytes i,const FieldOwner& owner) {
        const auto* f=e.data()+owner.pointer-0x02000000;
        return {owner.pointer,field32(i.data()+0x2974),field16(f+0x4fc),
                field16(f+0x528),field16(f+0x554)};
    }
    bool capture(FieldBytes e,FieldBytes i,FieldBytes rom,const FieldOwner& owner) {
        const auto next=key(e,i,owner);
        if(!attempted_ || next!=key_ || rom.data()!=rom_ || rom.size()!=rom_size_) {
            key_=next; rom_=rom.data(); rom_size_=rom.size(); attempted_=true; decoded_=false;
            reason="field-source-format"; ++decodes;
            if(key_[1]<0x08000000) return false;
            std::size_t archive=0;
            if(!field_archive_member(rom,key_[1]-0x08000000,2,archive)) return false;
            std::array<FieldSourceMap,3> candidate;
            for(unsigned bg=0;bg<3;++bg) {
                std::size_t resource=0;
                std::vector<std::uint8_t> data;
                if(!field_archive_member(rom,archive,key_[bg+2],resource) || !field_lz77(rom,resource,data)) return false;
                const unsigned w=field16(data.data()+20),h=field16(data.data()+22);
                if(field16(data.data()+16)!=0x4000 || field16(data.data()+18)!=0 || !w || !h || (w&7) || (h&7)) return false;
                const std::uint64_t bytes=std::uint64_t(w/8)*(h/8)*2;
                const unsigned start=field32(data.data()+28)&~3u;
                if(start<32 || bytes>0x40000 || !field_span(data,start,bytes)) return false;
                candidate[bg].columns=w/8; candidate[bg].rows=h/8;
                candidate[bg].entries.assign(data.begin()+start,data.begin()+start+bytes);
            }
            maps=std::move(candidate); decoded_=true;
        }
        if(!decoded_) return false;
        reason="field-source-live-mismatch";
        for(unsigned bg=0;bg<3;++bg) {
            const auto* d=i.data()+0x2a54+bg*0x34;
            const auto& map=maps[bg];
            const unsigned source=field32(d+0x1c);
            if(field16(d)!=0x4000 || field16(d+4)!=map.columns*8 || field16(d+6)!=map.rows*8 ||
               source<0x02000000 || !field_span(e,source-0x02000000,map.entries.size()) ||
               std::memcmp(e.data()+source-0x02000000,map.entries.data(),map.entries.size())) return false;
        }
        reason="none"; return true;
    }
};
}
