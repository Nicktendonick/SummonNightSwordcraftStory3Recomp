#pragma once
#include "custom_field_source.h"
#include <algorithm>
#include <bitset>

namespace swordcraft3 {
// 08005560 chooses the upload BEFORE updating its byte counters. The timer
// can belong to either neighbouring frame during initialization/reload, and
// next==frames is observable between increment and wrap. A zero duration is
// a 256-tick byte countdown, not a malformed family.
inline bool field_animation_clock_valid(std::span<const std::uint8_t> durations,
                                         unsigned next,unsigned timer) {
    if(durations.empty() || durations.size()>255 || timer>255 || next>durations.size()) return false;
    if(next==durations.size()) return timer==durations.back();
    const unsigned previous=(next+durations.size()-1)%durations.size();
    const unsigned a=durations[previous] ? durations[previous] : 256;
    const unsigned b=durations[next] ? durations[next] : 256;
    return timer<=std::max(a,b);
}

// Ordered source-entry constraints, never framebuffer/image comparisons.
// Candidate zero is next-1; the others walk backwards like the original lake
// implementation. An unconstrained/offscreen placement is an extrapolation
// from that RAM clock, NOT a claim that its upload was independently observed.
struct FieldAnimationPhase {
    std::bitset<256> allowed;
    unsigned frames=0,next=0;
    void reset(unsigned count,unsigned counter) {
        frames=count; next=counter; allowed.reset();
        for(unsigned n=0;n<count && n<255;++n) allowed.set(n);
    }
    unsigned frame(unsigned candidate) const { return (next+frames-1-candidate)%frames; }
    template<class Entry> bool constrain(std::uint16_t ring,Entry entry) {
        for(unsigned n=0;n<frames;++n) if(allowed[n] && entry(frame(n))!=ring) allowed.reset(n);
        return allowed.any();
    }
    unsigned selected() const {
        for(unsigned n=0;n<frames;++n) if(allowed[n]) return frame(n);
        return frames;
    }
};

inline bool field_animation_member(FieldBytes rom,std::uint64_t base,unsigned index,
                                    std::size_t& start,std::size_t& size) {
    if(!field_archive_member(rom,base,index,start) || !field_span(rom,base+8+index*8,8)) return false;
    const std::uint64_t bytes=std::uint64_t(field32(rom.data()+base+12+index*8))*16;
    if(!bytes || !field_span(rom,start,bytes)) return false;
    size=std::size_t(bytes); return true;
}

struct FieldAnimationPlacement {
    unsigned bg=0,x=0,y=0,w=0,h=0,index=0,frames=0,stride=0;
    std::size_t data=0; // First duration word; offset in immutable ROM.
    std::vector<std::uint8_t> durations;
};

// Cache only immutable authored metadata. No historical tiles or counter
// state is retained. Live descriptors and counters are rechecked each frame.
class FieldAnimationCache {
    struct Descriptor {
        unsigned count=0,table_word=0;
        std::uint32_t list=0,table=0;
        bool present=false;
    };
    std::array<Descriptor,3> descriptors_{};
    std::array<std::uint32_t,8> key_{};
    FieldBytes rom_{};
    bool attempted_=false,decoded_=false;
    bool decode(FieldBytes rom,const std::array<FieldSourceMap,3>& maps) {
        placements.clear(); descriptors_={};
        for(unsigned bg=0;bg<3;++bg) owners[bg].assign(maps[bg].columns*maps[bg].rows,0);
        if(key_[1]<0x08000000) return false;
        std::size_t archive=0,archive_size=0;
        if(!field_animation_member(rom,key_[1]-0x08000000,2,archive,archive_size)) return false;
        const auto archive_bytes=rom.subspan(archive,archive_size);
        for(unsigned bg=0;bg<3;++bg) {
            if(key_[5+bg]==0xffff) continue; // Loader explicitly skips absent assets.
            std::size_t relative=0,bytes=0;
            if(!field_animation_member(archive_bytes,0,key_[5+bg],relative,bytes) || bytes<32) return false;
            const std::size_t resource=archive+relative;
            const unsigned logical=field16(rom.data()+resource+0x12);
            if(logical<32 || logical>bytes) return false;
            const auto data=rom.subspan(resource,logical);
            auto& d=descriptors_[bg]; d.present=true;
            d.count=field16(data.data()+0x14); d.table_word=field16(data.data()+0x16);
            const unsigned list=field32(data.data()+0x18)&~3u,table=field32(data.data()+0x1c)&~3u;
            if(d.count>64 || placements.size()+d.count>64 || !d.table_word || d.table_word>256 ||
               list<32 || table<32 || !field_span(data,list,d.count*6) ||
               !field_span(data,table,d.table_word*2) ||
               (list<table+d.table_word*2 && table<list+d.count*6)) return false;
            d.list=std::uint32_t(0x08000000+resource+list); d.table=std::uint32_t(0x08000000+resource+table);
            for(unsigned n=0;n<d.count;++n) {
                const auto* p=data.data()+list+n*6;
                if(p[2]>=d.table_word) return false;
                const std::uint64_t table_entry=std::uint64_t(table)+p[2]*2;
                if(!field_span(data,table_entry,2)) return false;
                const unsigned family_offset=field16(data.data()+table_entry);
                if((family_offset&1) || family_offset<d.table_word*2) return false;
                const std::uint64_t family=std::uint64_t(table)+family_offset;
                if(!field_span(data,family,4)) return false;
                const auto* header=data.data()+family;
                const unsigned frames=header[0],w=header[2],h=header[3],stride=(1+w*h)*2;
                if(!frames || !w || !h || p[3]>=frames ||
                   p[0]+w>maps[bg].columns || p[1]+h>maps[bg].rows ||
                   !field_span(data,family+4,std::uint64_t(stride)*frames) ||
                   (family<list+d.count*6 && list<family+4+std::uint64_t(stride)*frames)) return false;
                FieldAnimationPlacement a{bg+1,p[0],p[1],w,h,n,frames,stride,resource+family+4,{}};
                for(unsigned f=0;f<frames;++f) a.durations.push_back(data[family+4+f*stride]);
                for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                    auto& owner=owners[bg][(p[1]+y)*maps[bg].columns+p[0]+x];
                    if(owner) return false; // Ordered overlapping updates are not modeled.
                    owner=static_cast<std::uint8_t>(placements.size()+1);
                }
                placements.push_back(std::move(a));
            }
        }
        return true;
    }
public:
    std::vector<FieldAnimationPlacement> placements;
    std::array<std::vector<std::uint8_t>,3> owners;
    std::array<FieldAnimationPhase,64> phases;
    unsigned decodes=0;
    const char* reason="animation-source-unread";
    void reset() { attempted_=decoded_=false; placements.clear(); }
    std::uint16_t entry(const FieldAnimationPlacement& a,unsigned frame,unsigned cell) const {
        return std::uint16_t(field16(rom_.data()+a.data+frame*a.stride+2+cell*2));
    }
    bool capture(FieldBytes e,FieldBytes i,FieldBytes rom,const FieldOwner& owner,
                 const std::array<FieldSourceMap,3>& maps) {
        if(owner.pointer<0x02000000 || !field_span(e,owner.pointer-0x02000000,0x558) || i.size()<0x2d90) {
            reason="animation-memory-span"; return false;
        }
        const auto key=FieldSourceCache::key(e,i,owner);
        if(!attempted_ || key!=key_ || rom.data()!=rom_.data() || rom.size()!=rom_.size()) {
            key_=key; rom_=rom; attempted_=true; decoded_=false; ++decodes;
            reason="animation-source-format";
            if(!decode(rom,maps)) return false;
            decoded_=true;
        }
        if(!decoded_) return false;
        reason="animation-live-descriptor";
        std::bitset<64> counters;
        const unsigned allocated=field32(i.data()+0x29b0);
        if(allocated>64) return false;
        // 08005D6C allocates counters for all four BGs from one 64-byte pool.
        // A BG0 allocation must not alias one of the field-layer counters.
        for(unsigned bg=0;bg<4;++bg) {
            const auto* live=i.data()+0x2a20+bg*0x34;
            const unsigned count=field16(live+0x10),base=field16(live+0x14);
            if(count!=field16(live+0x16) || base>allocated || count>allocated-base) return false;
            for(unsigned n=0;n<count;++n) {
                if(counters[base+n]) return false;
                counters.set(base+n);
            }
        }
        if(counters.count()!=allocated) return false;
        for(unsigned bg=0;bg<3;++bg) {
            const auto* live=i.data()+0x2a54+bg*0x34;
            const auto& d=descriptors_[bg];
            const unsigned count=field16(live+0x10),base=field16(live+0x14);
            if(count!=d.count || field16(live+0x16)!=count || base>64 || count>64-base) return false;
            if(d.present && (field16(live+0x12)!=d.table_word ||
                field32(live+0x28)!=d.list || field32(live+0x2c)!=d.table)) return false;
        }
        reason="animation-clock";
        for(unsigned n=0;n<placements.size();++n) {
            const auto& a=placements[n];
            const auto* d=i.data()+0x2a20+a.bg*0x34;
            const unsigned counter=field16(d+0x14)+a.index;
            const unsigned next=i[0x2d50+counter],timer=i[0x2af0+counter];
            if(!field_animation_clock_valid(a.durations,next,timer)) return false;
            phases[n].reset(a.frames,next);
        }
        reason="none"; return true;
    }
};
}
