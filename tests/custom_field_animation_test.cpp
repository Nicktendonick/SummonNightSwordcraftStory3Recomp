#include "custom_field_animation.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

using namespace swordcraft3;

namespace {
void put16(std::vector<std::uint8_t>& bytes,unsigned at,unsigned value) {
    assert(at+2<=bytes.size());
    bytes[at]=std::uint8_t(value); bytes[at+1]=std::uint8_t(value>>8);
}
void put32(std::vector<std::uint8_t>& bytes,unsigned at,std::uint32_t value) {
    put16(bytes,at,value); put16(bytes,at+2,value>>16);
}

// Entirely synthetic authored entries and guest descriptors, not game art.
// The two-level archive's member lengths are measured in 16-byte units.
struct Fixture {
    static constexpr unsigned root=0x1000,archive_root=0x100,archive=0x200;
    static constexpr unsigned resource_bytes=0x1000,list=0x20,table=0x200,family=0x220;
    std::vector<std::uint8_t> e=std::vector<std::uint8_t>(0x40000);
    std::vector<std::uint8_t> i=std::vector<std::uint8_t>(0x8000);
    std::vector<std::uint8_t> rom=std::vector<std::uint8_t>(0x6000);
    FieldOwner owner{0x02000000+root,1};
    std::array<FieldSourceMap,3> maps;

    static unsigned resource(unsigned asset) { return archive+0x100+asset*resource_bytes; }
    static unsigned descriptor(unsigned bg) { return 0x2a54+bg*0x34; }
    static unsigned asset_field(unsigned bg) { return root + 0x4fe + bg*0x2c; }

    Fixture() {
        put32(i,0x2974,0x08000000+archive_root);
        put32(rom,archive_root+8+2*8,(archive-archive_root)/16);
        put32(rom,archive_root+12+2*8,0x5000/16);
        for(unsigned bg=0;bg<3;++bg) {
            maps[bg].columns=16; maps[bg].rows=8;
            maps[bg].entries.resize(16*8*2);
            put16(e,root+0x4fc+bg*0x2c,10+bg);
        }
        for(unsigned asset=0;asset<4;++asset) make_resource(asset,1,3,1,1,{8,14,5});
        set_layer(0,0,0); set_layer(1,1,1); set_layer(2,0xffff,2);
    }

    void make_resource(unsigned asset,unsigned count,unsigned frames,unsigned w,unsigned h,
                       std::initializer_list<unsigned> durations) {
        const unsigned r=resource(asset);
        std::fill(rom.begin()+r,rom.begin()+r+resource_bytes,0);
        put32(rom,archive+8+asset*8,(r-archive)/16);
        put32(rom,archive+12+asset*8,resource_bytes/16);
        put16(rom,r+0x12,resource_bytes);
        put16(rom,r+0x14,count); put16(rom,r+0x16,1);
        put32(rom,r+0x18,list); put32(rom,r+0x1c,table);
        put16(rom,r+table,family-table);
        rom[r+family]=std::uint8_t(frames);
        rom[r+family+2]=std::uint8_t(w); rom[r+family+3]=std::uint8_t(h);
        for(unsigned n=0;n<count;++n) {
            rom[r+list+n*6]=std::uint8_t(n%16);
            rom[r+list+n*6+1]=std::uint8_t(n/16);
            rom[r+list+n*6+2]=0;
            rom[r+list+n*6+3]=0;
        }
        const std::vector<unsigned> periods(durations);
        assert(!periods.empty());
        for(unsigned frame=0;frame<frames;++frame) {
            const unsigned at=r+family+4+frame*(1+w*h)*2;
            put16(rom,at,periods[frame%periods.size()]);
            for(unsigned cell=0;cell<w*h;++cell)
                put16(rom,at+2+cell*2,0x1000+asset*0x100+frame*3+cell);
        }
    }

    void set_layer(unsigned bg,unsigned asset,unsigned base) {
        put16(e,asset_field(bg),asset);
        const unsigned d=descriptor(bg);
        put16(i,d+0x14,base);
        const unsigned count=asset==0xffff ? 0 : field16(rom.data()+resource(asset)+0x14);
        put16(i,d+0x10,count); put16(i,d+0x16,count);
        unsigned allocated=field16(i.data()+0x2a20+0x16);
        for(unsigned layer=0;layer<3;++layer) allocated+=field16(i.data()+descriptor(layer)+0x16);
        put32(i,0x29b0,allocated);
        if(asset==0xffff) return;
        const unsigned r=resource(asset),frames=rom[r+family];
        put16(i,d+0x12,field16(rom.data()+r+0x16));
        put32(i,d+0x28,0x08000000+r+list);
        put32(i,d+0x2c,0x08000000+r+table);
        for(unsigned n=0;n<count && base+n<64;++n) {
            i[0x2d50+base+n]=std::uint8_t(frames>1 ? 1 : 0);
            i[0x2af0+base+n]=rom[r+family+4];
        }
    }

    bool capture(FieldAnimationCache& cache) const { return cache.capture(e,i,rom,owner,maps); }
};

template<class Edit> void rejected(Edit edit,const char* reason="animation-source-format") {
    Fixture f; edit(f); FieldAnimationCache cache;
    assert(!f.capture(cache)); assert(std::strcmp(cache.reason,reason)==0);
}

void clock_tests() {
    const std::vector<std::uint8_t> periods{3,17,6,22,1};
    for(unsigned next=0;next<periods.size();++next) {
        const unsigned maximum=std::max(periods[next],periods[(next+periods.size()-1)%periods.size()]);
        for(unsigned timer=0;timer<=255;++timer)
            assert(field_animation_clock_valid(periods,next,timer)==(timer<=maximum));
    }
    for(unsigned timer=0;timer<=255;++timer)
        assert(field_animation_clock_valid(periods,periods.size(),timer)==(timer==periods.back()));
    assert(!field_animation_clock_valid(periods,periods.size()+1,0));
    assert(!field_animation_clock_valid(periods,0,256));
    assert(!field_animation_clock_valid({},0,0));
    assert(!field_animation_clock_valid(std::vector<std::uint8_t>(256,8),0,8));
    const std::vector<std::uint8_t> single{7};
    for(unsigned timer=0;timer<=7;++timer) assert(field_animation_clock_valid(single,0,timer));
    assert(!field_animation_clock_valid(single,0,8));
    assert(field_animation_clock_valid(single,1,7));
    assert(!field_animation_clock_valid(single,1,0));
    // Zero duration means a wrapping byte countdown: 0,255,...,1, not zero ticks.
    const std::vector<std::uint8_t> zero{4,0};
    for(unsigned next=0;next<2;++next) for(unsigned timer=0;timer<=255;++timer)
        assert(field_animation_clock_valid(zero,next,timer));
    assert(field_animation_clock_valid(zero,2,0));
    assert(!field_animation_clock_valid(zero,2,1));
    assert(field_animation_clock_valid(std::vector<std::uint8_t>(255,9),255,9));
}

void phase_tests() {
    for(unsigned count:{1u,3u,5u,255u}) for(unsigned next=0;next<=count;++next) {
        FieldAnimationPhase phase; phase.reset(count,next);
        assert(phase.allowed.count()==count);
        // No observed ring entry: the preferred phase is explicitly extrapolated.
        assert(phase.selected()==(next+count-1)%count);
        for(unsigned candidate=0;candidate<count;++candidate)
            assert(phase.frame(candidate)==(next+count-1-candidate)%count);
        const unsigned target=(next+2)%count;
        assert(phase.constrain(std::uint16_t(0x100+target),[](unsigned frame) {
            return std::uint16_t(0x100+frame);
        }));
        assert(phase.allowed.count()==1 && phase.selected()==target);
    }
    // Intersect ordered entries, rather than accepting an unordered tile set.
    // Frames 0 and 1 agree at cell 0 but differ at cell 1.
    const std::uint16_t entries[3][2]={{0x10,0x20},{0x10,0x30},{0x20,0x10}};
    FieldAnimationPhase a,b; a.reset(3,1); b.reset(3,1);
    assert(a.constrain(0x10,[&](unsigned f){return entries[f][0];}));
    assert(a.allowed.count()==2 && a.selected()==0);
    assert(a.constrain(0x30,[&](unsigned f){return entries[f][1];}));
    assert(a.selected()==1);
    assert(b.constrain(0x20,[&](unsigned f){return entries[f][0];}));
    assert(b.selected()==2 && a.selected()==1); // Independent placement clocks.
    assert(!a.constrain(0x20,[&](unsigned f){return entries[f][1];}));
    assert(a.allowed.none() && a.selected()==3 && b.selected()==2);
    a.reset(3,1);
    assert(a.allowed.count()==3 && a.selected()==0); // No phase history survives reset.
    // Every possible legacy four-frame candidate mask keeps the same ordered
    // preference, including the next==4 increment-before-wrap transient.
    for(unsigned next=0;next<=4;++next) for(unsigned mask=0;mask<16;++mask) {
        FieldAnimationPhase phase; phase.reset(4,next);
        unsigned expected=4;
        for(unsigned candidate=0;candidate<4;++candidate) if(mask&(1u<<candidate)) {
            expected=(next+4-(candidate+1))%4; break;
        }
        const bool valid=phase.constrain(1,[&](unsigned frame) {
            for(unsigned candidate=0;candidate<4;++candidate)
                if(frame==(next+4-(candidate+1))%4) return std::uint16_t((mask>>candidate)&1u);
            return std::uint16_t(0);
        });
        assert(valid==(mask!=0)); assert(phase.selected()==expected);
    }
}

void valid_resources_and_cache_tests() {
    Fixture f; FieldAnimationCache cache;
    assert(f.capture(cache) && cache.decodes==1);
    assert(cache.placements.size()==2 && std::strcmp(cache.reason,"none")==0);
    assert(cache.placements[0].bg==1 && cache.placements[1].bg==2);
    assert(cache.owners[0][0]==1 && cache.owners[1][0]==2 && cache.owners[2][0]==0);
    assert(cache.entry(cache.placements[0],2,0)==0x1006);
    assert(f.capture(cache) && cache.decodes==1);
    // Cached immutable metadata never bypasses live pointer/counter checks.
    put32(f.i,Fixture::descriptor(0)+0x28,0x08000000+Fixture::resource(0)+Fixture::list+6);
    assert(!f.capture(cache) && cache.decodes==1);
    f.set_layer(0,0,0); assert(f.capture(cache) && cache.decodes==1);
    f.i[0x2d50]=4; assert(!f.capture(cache) && std::strcmp(cache.reason,"animation-clock")==0);
    f.i[0x2d50]=3; f.i[0x2af0]=5; assert(f.capture(cache)); // Increment-before-wrap transient.
    f.i[0x2af0]=4; assert(!f.capture(cache));
    f.i[0x2d50]=1; f.i[0x2af0]=0; assert(f.capture(cache));
    // Same field address and map IDs, different animation asset: force decode.
    f.set_layer(0,3,0); assert(f.capture(cache) && cache.decodes==2);
    assert(cache.entry(cache.placements[0],2,0)==0x1306);
    cache.reset(); assert(f.capture(cache) && cache.decodes==3);
    auto other_rom=f.rom;
    assert(cache.capture(f.e,f.i,other_rom,f.owner,f.maps) && cache.decodes==4);
    assert(cache.capture(f.e,f.i,FieldBytes(other_rom).first(other_rom.size()-1),f.owner,f.maps));
    assert(cache.decodes==5);
    // A rejected key is cached, and a new asset key can recover without history.
    put16(f.e,Fixture::asset_field(0),500); assert(!f.capture(cache));
    const unsigned rejected_decodes=cache.decodes;
    assert(!f.capture(cache) && cache.decodes==rejected_decodes);
    f.set_layer(0,0,0); assert(f.capture(cache) && cache.decodes==rejected_decodes+1);
    f.set_layer(2,2,2); assert(f.capture(cache) && cache.placements.size()==3);
    assert(cache.placements[2].bg==3 && cache.owners[2][0]==3);

    for(unsigned frames:{1u,5u,255u}) {
        Fixture many; many.make_resource(0,1,frames,2,1,{0x100,0x10e,3,27,1});
        many.set_layer(0,0,0); FieldAnimationCache c;
        assert(many.capture(c) && c.placements[0].frames==frames);
        assert(c.placements[0].durations[0]==0); // Only the low byte is stored in RAM.
        if(frames>1) assert(c.placements[0].durations[1]==14);
        assert(c.entry(c.placements[0],frames-1,1)==0x1000+(frames-1)*3+1);
        many.i[0x2af0]=255; assert(many.capture(c));
    }
    // The physical allocation is 64 counters, including slot 63.
    Fixture full; full.make_resource(0,64,1,1,1,{8});
    full.set_layer(0,0,0); full.set_layer(1,0xffff,64); full.set_layer(2,0xffff,64);
    FieldAnimationCache all; assert(full.capture(all));
    assert(all.placements.size()==64 && all.owners[0][63]==64);
    assert(all.phases[63].selected()==0);
    // Zero-placement resources still have an authored family table.
    Fixture idle; idle.make_resource(0,0,5,1,1,{8}); idle.set_layer(0,0,0);
    idle.set_layer(1,1,0); idle.set_layer(2,0xffff,1);
    FieldAnimationCache unused; assert(idle.capture(unused) && unused.placements.size()==1);
    // Aligned-down resource-relative pointers match the guest header decoder.
    Fixture aligned;
    put32(aligned.rom,Fixture::resource(0)+0x18,Fixture::list+3);
    put32(aligned.rom,Fixture::resource(0)+0x1c,Fixture::table+2);
    FieldAnimationCache rounded; assert(aligned.capture(rounded));
}

void malformed_resource_tests() {
    rejected([](Fixture& f){f.rom.resize(Fixture::archive+16);});
    rejected([](Fixture& f){put32(f.rom,Fixture::archive_root+28,0);});
    rejected([](Fixture& f){put32(f.rom,Fixture::archive_root+28,0x10000000);});
    rejected([](Fixture& f){put32(f.i,0x2974,0x07000000);});
    rejected([](Fixture& f){put32(f.i,0x2974,0x08000101);});
    rejected([](Fixture& f){put32(f.rom,Fixture::archive+12,0);});
    rejected([](Fixture& f){put32(f.rom,Fixture::archive+12,1);});
    rejected([](Fixture& f){put32(f.rom,Fixture::archive+8,1);});
    rejected([](Fixture& f){put32(f.rom,Fixture::archive+8,0x4fff/16);});
    // These pointers remain within the entire ROM but escape their own resource.
    rejected([](Fixture& f){put32(f.rom,Fixture::resource(0)+0x18,0xffc);});
    rejected([](Fixture& f){put32(f.rom,Fixture::resource(0)+0x1c,0x1000);});
    rejected([](Fixture& f){put32(f.rom,Fixture::resource(0)+0x18,0x1c);});
    rejected([](Fixture& f){put32(f.rom,Fixture::resource(0)+0x1c,0x1c);});
    rejected([](Fixture& f){put16(f.rom,Fixture::resource(0)+Fixture::table,0xdfd);});
    rejected([](Fixture& f){put16(f.rom,Fixture::resource(0)+Fixture::table,0xffff);});
    rejected([](Fixture& f){put16(f.rom,Fixture::resource(0)+Fixture::table,0xdfe);});
    rejected([](Fixture& f){put16(f.rom,Fixture::resource(0)+0x12,Fixture::family+4);});
    rejected([](Fixture& f){put16(f.rom,Fixture::resource(0)+0x12,31);});
    rejected([](Fixture& f){put16(f.rom,Fixture::resource(0)+0x12,Fixture::resource_bytes+1);});
    rejected([](Fixture& f){put16(f.rom,Fixture::resource(0)+0x16,0);});
    rejected([](Fixture& f){put16(f.rom,Fixture::resource(0)+0x16,257);});
    rejected([](Fixture& f){f.rom[Fixture::resource(0)+Fixture::list+2]=1;});
    rejected([](Fixture& f){put16(f.rom,Fixture::resource(0)+Fixture::table,0);});
    rejected([](Fixture& f){put32(f.rom,Fixture::resource(0)+0x1c,Fixture::list);});
    rejected([](Fixture& f){f.rom[Fixture::resource(0)+Fixture::family]=0;});
    rejected([](Fixture& f){f.rom[Fixture::resource(0)+Fixture::family+2]=0;});
    rejected([](Fixture& f){f.rom[Fixture::resource(0)+Fixture::family+3]=0;});
    rejected([](Fixture& f){f.rom[Fixture::resource(0)+Fixture::family+2]=255;});
    rejected([](Fixture& f){f.rom[Fixture::resource(0)+Fixture::list+3]=3;});
    rejected([](Fixture& f){f.rom[Fixture::resource(0)+Fixture::list]=16;});
    rejected([](Fixture& f){f.rom[Fixture::resource(0)+Fixture::list+1]=8;});
    rejected([](Fixture& f){put16(f.rom,Fixture::resource(0)+0x14,65);});
    rejected([](Fixture& f){
        f.make_resource(0,2,3,1,1,{8});
        f.rom[Fixture::resource(0)+Fixture::list+6]=0;
        f.set_layer(0,0,0);
    }); // Overlapping placement footprints have ordered guest writes not modeled here.
    rejected([](Fixture& f){
        f.make_resource(0,64,1,1,1,{8}); f.set_layer(0,0,0);
    }); // Total placement count across layers exceeds 64.
}

void live_descriptor_tests() {
    const char* live="animation-live-descriptor";
    rejected([](Fixture& f){put16(f.i,Fixture::descriptor(0)+0x10,0);},live);
    rejected([](Fixture& f){put16(f.i,Fixture::descriptor(0)+0x16,0);},live);
    rejected([](Fixture& f){put16(f.i,Fixture::descriptor(0)+0x12,2);},live);
    rejected([](Fixture& f){put16(f.i,Fixture::descriptor(0)+0x14,64);},live);
    rejected([](Fixture& f){put16(f.i,Fixture::descriptor(2)+0x14,65);},live);
    rejected([](Fixture& f){put16(f.i,Fixture::descriptor(1)+0x14,0);},live);
    rejected([](Fixture& f){put32(f.i,0x29b0,65);},live);
    rejected([](Fixture& f){put32(f.i,0x29b0,1);},live);
    rejected([](Fixture& f){put32(f.i,0x29b0,3);},live);
    rejected([](Fixture& f){
        put16(f.i,0x2a20+0x10,1); put16(f.i,0x2a20+0x16,1);
        put16(f.i,0x2a20+0x14,0); put32(f.i,0x29b0,3);
    },live); // BG0 may not claim the field's counter even with a larger pool.
    rejected([](Fixture& f){put32(f.i,Fixture::descriptor(0)+0x28,0x08000000+Fixture::resource(1)+Fixture::list);},live);
    rejected([](Fixture& f){put32(f.i,Fixture::descriptor(0)+0x2c,0x08000000+Fixture::resource(1)+Fixture::table);},live);
    rejected([](Fixture& f){put16(f.i,Fixture::descriptor(2)+0x10,1); put16(f.i,Fixture::descriptor(2)+0x16,1);},live);
    rejected([](Fixture& f){f.i[0x2d50]=255;},"animation-clock");
    rejected([](Fixture& f){f.i[0x2af0]=15;},"animation-clock");
    rejected([](Fixture& f){f.owner.pointer=0x01000000;},"animation-memory-span");
    rejected([](Fixture& f){f.owner.pointer=0x0203fab0;},"animation-memory-span");
    rejected([](Fixture& f){f.i.resize(0x2d8f);},"animation-memory-span");
    rejected([](Fixture& f){f.e.resize(Fixture::root+0x557);},"animation-memory-span");
}
}

int main() {
    clock_tests(); phase_tests(); valid_resources_and_cache_tests();
    malformed_resource_tests(); live_descriptor_tests();
    std::puts("Field animation source bounds, live ownership, byte clocks and per-placement phases: PASS");
}
