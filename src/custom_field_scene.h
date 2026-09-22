#pragma once
#include "gba_raster_capture.h"
#include "gba_text_sample.h"
#include "gba_obj_sample.h"
#include "runtime.h"
#include "sha1.h"
#include "custom_lake_control.h"
#include "custom_lake_animation.h"
#include "custom_field_profiles.h"
#include "custom_field_events.h"
#include "custom_field_objects.h"
#include "custom_field_source.h"
#include "custom_field_action.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace swordcraft3 {
// Narrow, content-authenticated field scenes. No guest hooks, cached live map
// pointers, invented decoration, wrapping or camera changes. Regular objects
// are sampled from each immutable raster row; the native center is untouched.
class CustomFieldScene {
    const CustomFieldProfile* profile_=nullptr;
    FieldSourceCache sources_;
    FieldOwner owner_{};
    const std::uint8_t* checked_rom_=nullptr;
    std::size_t checked_rom_size_=0;
    bool supported_rom_=false,general_=false;
    struct Layer {
        std::vector<std::uint8_t> map;
        unsigned columns=0,rows=0,scroll_x=0,scroll_y=0,bias=0;
    };
    std::array<Layer,3> layers_{};
    std::array<std::array<Layer,3>,4> phases_{};
    struct Animation { unsigned bg,x,y,w,h; };
    std::array<Animation,32> animations_{};
    unsigned animation_count_=0;
    std::array<std::vector<unsigned char>,3> owners_{};
    bool valid_=false;
    mutable const char* decline_="not-captured";
    static unsigned u16(const std::uint8_t* p) { return gba::text_u16(p); }
    static unsigned u32(const std::uint8_t* p) { return u16(p)|(u16(p+2)<<16); }
    static const std::uint8_t* rom_bytes(const gbarecomp::ExtendedViewFrameInfo& m,
                                        std::uint64_t address,std::size_t count) {
        if(!m.rom || address<0x08000000u) return nullptr;
        const auto offset=address-0x08000000u;
        if(offset>m.rom_size || count>m.rom_size-offset) return nullptr;
        return m.rom+offset;
    }
    bool animate(Layer& l,const std::uint8_t* d,const gbarecomp::ExtendedViewFrameInfo& m,unsigned phase) {
        // csm3 sub_08005560/sub_080057E4: six-byte placement records,
        // four-byte animation header, then duration + w*h entries per frame.
        // The index is advanced before queued map copies reach visible VRAM.
        // Each placement advances separately; resolve its visible phase against
        // the captured ring, not a single phase shared by the entire scene.
        const unsigned count=u16(d+0x10),base=u16(d+0x14);
        if(count>32 || base+count>32 || u16(d)!=0x4000) return false;
        if(count && !profile_) return false; // New animation families need separate evidence.
        const std::uint64_t list=u32(d+0x28),table=u32(d+0x2C);
        for(unsigned i=0;i<count;++i) {
            const auto* p=rom_bytes(m,list+i*6,6);
            if(!p) return false;
            const auto* offset=rom_bytes(m,table+p[2]*2,2);
            if(!offset) return false;
            const auto address=table+u16(offset);
            const auto* header=rom_bytes(m,address,4);
            if(!header) return false;
            const unsigned frames=header[0],w=header[2],h=header[3];
            const unsigned next=m.iwram[0x2D50+base+i],timer=m.iwram[0x2AF0+base+i];
            if(!lake_animation_clock_valid(frames,next,timer,profile_->animation_ticks) || !w || !h || p[0]+w>l.columns || p[1]+h>l.rows) {
                if(std::getenv("SWORDCRAFT3_CUSTOM_AUDIT"))
                    std::fprintf(stderr,"[sc3:animation-state] base=%u index=%u frames=%u next=%u timer=%u x=%u y=%u w=%u h=%u\n",base,i,frames,next,timer,p[0],p[1],w,h);
                return false;
            }
            if(phase==1) {
                if(animation_count_>=animations_.size()) return false;
                const unsigned bg=unsigned((d-(m.iwram+0x2A20))/0x34);
                animations_[animation_count_]={bg,p[0],p[1],w,h};
                ++animation_count_;
                for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                    auto& owner=owners_[bg-1][(p[1]+y)*l.columns+p[0]+x];
                    if(owner) return false; // Overlapping animation needs its own model.
                    owner=static_cast<unsigned char>(animation_count_);
                }
            }
            const unsigned stride=(1+w*h)*2;
            // Known scene families have four equal-duration frames. This narrow
            // gate makes phase reconciliation explicit, not arbitrary tile reuse.
            for(unsigned f=0;f<frames;++f) {
                const auto* duration=rom_bytes(m,address+4+f*stride,2);
                if(!duration || u16(duration)!=profile_->animation_ticks) return false;
            }
            unsigned visible=(next+frames-phase)%frames;
            const auto* data=rom_bytes(m,address+4+visible*stride,stride);
            if(!data) return false;
            for(unsigned y=0;y<h;++y)
                std::memcpy(l.map.data()+((p[1]+y)*l.columns+p[0])*2,data+2+y*w*2,w*2);
        }
        return true;
    }
    static int align(unsigned descriptor,unsigned hw) {
        int delta=int(hw&511)-int(descriptor&511);
        if(delta>255) delta-=512;
        if(delta<-256) delta+=512;
        return int(descriptor)+delta;
    }
    bool entry(unsigned bg,int x,int y,std::uint16_t& value) const {
        const auto& l=layers_[bg-1];
        if(x<0 || x>=int(l.columns*8) || y<0 || y>=int(l.rows*8)) return false;
        value=std::uint16_t(u16(l.map.data()+((y/8)*l.columns+x/8)*2)+l.bias);
        return true;
    }
    bool eligible(const gba::GbaRasterCapture::Line& line,unsigned y) const {
        const auto* io=line.io.data();
        if ((line.dispcnt&0xEF87u)!=0x0F00u || u16(io+8)!=0x0500 ||
            u16(io+10)!=0x0605 || u16(io+12)!=0x0706 || u16(io+14)!=0x080B ||
            (u16(io+0x50)&0xC0u)) { decline_="display-or-effect"; return false; }
        // Scene/control authorization is from the task owner and script flags.
        // BG0 colors or visibility must never classify dialogue or menus.
        return true;
    }
public:
    bool valid() const { return valid_; }
    bool verified_ambient(const gbarecomp::ExtendedViewFrameInfo& memory,unsigned flags) const {
        if(!profile_ || std::strcmp(profile_->name,"village-chief-outdoors") ||
           !memory.iwram || !memory.ewram || memory.iwram_size<0x65c4 || memory.ewram_size<0x6d10) return false;
        const auto* vm=memory.iwram+0x6590;
        return chief_ambient_event_state(flags,u32(memory.ewram+16),u32(vm+0x28),
            vm[0],vm[0x2c],u16(vm+0x2e),u32(vm+0x30)) &&
            gba::sha1(memory.ewram+0x6cd0,64).hex()=="6f29204f532fbe80bf81ebbe22f278a7d2f2cb29";
    }
    bool objects_allowed(const gbarecomp::ExtendedViewFrameInfo& memory) const {
        FieldOwner current;
        return valid_ && read_field_owner({memory.ewram,memory.ewram_size},{memory.iwram,memory.iwram_size},current) &&
            current.pointer==owner_.pointer &&
            (!general_ || sources_.matches_key({memory.ewram,memory.ewram_size},{memory.iwram,memory.iwram_size},current)) &&
            (current.free_control() || field_tool_action({memory.ewram,memory.ewram_size},current) ||
             verified_ambient(memory,current.flags));
    }
    const char* decline_reason() const { return decline_; }
    unsigned source_decodes() const { return sources_.decodes; }
    const char* scene_name() const { return profile_ ? profile_->name : "general-field"; }
    void reset() { valid_=false; profile_=nullptr; owner_={}; sources_.reset(); }
    bool capture(const gbarecomp::ExtendedViewFrameInfo& memory) {
        valid_=false; profile_=nullptr; decline_="field-owner"; animation_count_=0;
        if(!memory.iwram || !memory.ewram || !memory.rom) return false;
        if(checked_rom_!=memory.rom || checked_rom_size_!=memory.rom_size) {
            checked_rom_=memory.rom; checked_rom_size_=memory.rom_size;
            const auto hash=gba::sha1(memory.rom,memory.rom_size).hex();
            supported_rom_=hash=="3f5253fcf57e07ce52472bd29a61d16b98a12376" || hash=="bb2eebf98deb59bb6218442c2308bb5033ae2915";
            sources_.reset();
        }
        if(!supported_rom_) { decline_="field-rom-revision"; return false; }
        if(!read_field_owner({memory.ewram,memory.ewram_size},{memory.iwram,memory.iwram_size},owner_)) {
            sources_.reset(); return false;
        }
        const char* general=std::getenv("SWORDCRAFT3_CUSTOM_GENERAL_FIELDS");
        general_=general && !std::strcmp(general,"1");
        decline_="scripted-animation";
        // The separate scripted animation list is not reconstructed in this
        // pilot. Any active entry declines rather than extending the wrong art.
        for(unsigned i=0;i<12;++i) if(memory.iwram[0x29C0+i*8]) return false;
        const auto* first=memory.iwram+0x2A20+0x34;
        profile_=custom_field_profile(u16(first+4),u16(first+6));
        if(general_) {
            if(!sources_.capture({memory.ewram,memory.ewram_size},{memory.iwram,memory.iwram_size},
                                {memory.rom,memory.rom_size},owner_)) {
                decline_=sources_.reason; return false;
            }
            if(profile_) for(unsigned bg=0;bg<3;++bg) {
                const auto& source=sources_.maps[bg];
                if(source.columns!=profile_->columns || source.rows!=profile_->rows ||
                   gba::sha1(source.entries.data(),source.entries.size()).hex()!=profile_->hashes[bg]) {
                    profile_=nullptr; break;
                }
            }
        } else if(!profile_) { decline_="map-identity"; return false; }
        const char* additional=std::getenv("SWORDCRAFT3_CUSTOM_ADDITIONAL_AREAS");
        if(profile_ && profile_->additional && additional && !std::strcmp(additional,"0")) {
            decline_="additional-area-disabled"; return false;
        }
        for(unsigned bg=1;bg<=3;++bg) {
            const auto* d=memory.iwram+0x2A20+bg*0x34;
            auto& l=layers_[bg-1];
            l.columns=general_ ? sources_.maps[bg-1].columns : profile_->columns;
            l.rows=general_ ? sources_.maps[bg-1].rows : profile_->rows;
            const unsigned map_bytes=l.columns*l.rows*2;
            const unsigned source=u32(d+0x1C);
            if(u16(d+4)!=l.columns*8 || u16(d+6)!=l.rows*8 || source<0x02000000) return false;
            const std::size_t off=source-0x02000000;
            if(off>memory.ewram_size || map_bytes>memory.ewram_size-off) return false;
            l.map.assign(memory.ewram+off,memory.ewram+off+map_bytes);
            owners_[bg-1].assign(l.columns*l.rows,0);
            if(!general_ && gba::sha1(l.map.data(),map_bytes).hex()!=profile_->hashes[bg-1]) return false;
            l.scroll_x=u16(d+8);l.scroll_y=u16(d+10);
            l.bias=u16(d+0x1A)+(unsigned(d[0x19])<<12);
            for(unsigned phase=0;phase<4;++phase) {
                phases_[phase][bg-1]=l;
                if(!animate(phases_[phase][bg-1],d,memory,phase+1)) {
                    decline_="animation-descriptor"; return false;
                }
            }
        }
        const unsigned field_flags=owner_.flags;
        const bool player_control=owner_.free_control();
        const bool tool_action=field_tool_action({memory.ewram,memory.ewram_size},owner_);
        const bool ambient_event=!player_control && !tool_action && verified_ambient(memory,field_flags);
        if(!player_control && !tool_action && !ambient_event) {
            if(std::getenv("SWORDCRAFT3_CUSTOM_AUDIT"))
                std::fprintf(stderr,"[sc3:field-control] scene=%s flags=%05x\n",scene_name(),field_flags);
            if(std::getenv("SWORDCRAFT3_CUSTOM_AUDIT_DETAIL") && memory.iwram_size>=0x65D8 && memory.ewram_size>=24) {
                const auto* vm=memory.iwram+0x6590;
                const unsigned ip=u32(vm+0x30),base=u32(vm+0x28);
                std::fprintf(stderr,"[sc3:field-vm] active=%02x state=%02x op=%04x ip=%08x base=%08x script=%u code=",
                    vm[0],vm[0x2C],u16(vm+0x2E),ip,base,u32(memory.ewram+16));
                if(ip>=0x02000008u && ip-0x02000000u<=memory.ewram_size-24)
                    for(unsigned k=0;k<16;++k) std::fprintf(stderr," %04x",u16(memory.ewram+(ip-0x02000000u)-8+k*2));
                std::fputc('\n',stderr);
            }
            decline_="lake-player-control"; return false;
        }
        if(ambient_event && std::getenv("SWORDCRAFT3_CUSTOM_AUDIT"))
            std::fprintf(stderr,"[sc3:field-ambient] verified chief sound/flag event\n");
        if(tool_action && std::getenv("SWORDCRAFT3_STATE_TRACE"))
            std::fprintf(stderr,"[sc3:field-tool] flags=%04x permission=action-owned\n",field_flags);
        valid_=true; decline_="none";
        return true;
    }
    bool draw(const gba::GbaRasterCapture& capture, std::uint8_t* output,unsigned width) {
        if(!valid_ || !capture.complete() || !output || width<=240 || width>480) return false;
        // Authorize the entire frame before touching the output. No striped
        // partial widening around dialogue, transitions or unsupported effects.
        for(unsigned y=0;y<160;++y) if(!eligible(*capture.line(y),y)) return false;
        // Authorize each placement independently. Static cells must still match
        // exactly, and all visible cells of an animation must agree on a phase.
        std::array<unsigned,32> allowed; allowed.fill(15);
        layers_=phases_[0];
        for(unsigned y=0;y<160;++y) {
            const auto& line=*capture.line(y); const auto* io=line.io.data();
            for(unsigned bg=1;bg<=3;++bg) {
                const auto& l=layers_[bg-1];
                const unsigned hx=u16(io+0x10+bg*4)&511,hy=u16(io+0x12+bg*4)&511;
                const int sx=align(l.scroll_x,hx),sy=align(l.scroll_y,hy)+int(y);
                if(sx<0 || sx+239>=int(l.columns*8) || sy<0 || sy>=int(l.rows*8)) { decline_="camera-bounds"; return false; }
                // One sample per intersecting tile on each captured raster row.
                for(unsigned x=0;x<240;) {
                    const unsigned index=(unsigned(sy)/8)*l.columns+(unsigned(sx)+x)/8;
                    std::uint16_t ring=0;
                    if(!gba::text_ring_entry(u16(io+8+bg*2),hx+x,hy+y,line.vram.data(),line.vram.size(),ring)) return false;
                    const unsigned owner=owners_[bg-1][index];
                    if(!owner) {
                        if(std::uint16_t(u16(l.map.data()+index*2)+l.bias)!=ring) {
                            decline_="static-source-ring-disagreement"; return false;
                        }
                    } else {
                        unsigned match=0;
                        for(unsigned p=0;p<4;++p)
                            if(std::uint16_t(u16(phases_[p][bg-1].map.data()+index*2)+l.bias)==ring) match|=1u<<p;
                        allowed[owner-1]&=match;
                        if(!allowed[owner-1]) {
                            decline_="animation-source-ring-disagreement";
                            if(std::getenv("SWORDCRAFT3_CUSTOM_AUDIT_DETAIL"))
                                std::fprintf(stderr,"[sc3:ring] owner=%u bg=%u x=%u y=%u ring=%04x mask=%u\n",owner,bg,x,y,ring,match);
                            return false;
                        }
                    }
                    x+=8-((unsigned(sx)+x)&7);
                }
            }
        }
        for(unsigned i=0;i<animation_count_;++i) {
            unsigned phase=0; while(!(allowed[i]&(1u<<phase))) ++phase;
            const auto& a=animations_[i];
            // Fully offscreen / visually ambiguous placements use the most
            // recent RAM-completed phase (next-1), not a cached prior picture.
            for(unsigned y=0;y<a.h;++y) {
                const unsigned offset=((a.y+y)*layers_[a.bg-1].columns+a.x)*2;
                std::memcpy(layers_[a.bg-1].map.data()+offset,phases_[phase][a.bg-1].map.data()+offset,a.w*2);
            }
        }
        decline_="none";
        const int left=int(width-240)/2;
        const char* objects=std::getenv("SWORDCRAFT3_CUSTOM_OBJECTS");
        const bool draw_objects=!objects || std::strcmp(objects,"0");
        for(unsigned y=0;y<160;++y) {
            const auto& line=*capture.line(y);
            const auto* io=line.io.data();
            struct Obj { unsigned a0,a1,a2,key; int sx,sy; };
            std::array<Obj,128> row_objects{};
            unsigned object_count=0;
            if(draw_objects && (line.dispcnt&0x1000)) for(unsigned index=0;index<128;++index) {
                const auto* obj=line.oam.data()+index*8;
                const auto a0=u16(obj),a1=u16(obj+2),a2=u16(obj+4);
                if((a0&0x1f00) || (a0>>14)>=3) continue;
                int sx=field_object_x(a1&511,width),sy=int(a0&255);
                if(sy>=160) sy-=256;
                // Conservative 64px bound prunes native-only/off-row objects.
                if(int(y)<sy || int(y)>=sy+64 || (sx>=0 && sx+64<=240)) continue;
                row_objects[object_count++]={a0,a1,a2,((a2>>10)&3)*256+index,sx,sy};
            }
            for(unsigned x=0;x<width;++x) {
                if(int(x)>=left && int(x)<left+240) continue;
                auto* pixel=output+(y*width+x)*3;
                pixel[0]=pixel[1]=pixel[2]=0;
                unsigned background_priority=4;
                std::uint16_t background_color=0;
                // Verified field priorities: BG3 ground, BG2 details, BG1 foreground.
                for(unsigned bg=3;bg>=1;--bg) {
                    const auto& l=layers_[bg-1];
                    const int wx=align(l.scroll_x,u16(io+0x10+bg*4))+int(x)-left;
                    const int wy=align(l.scroll_y,u16(io+0x12+bg*4))+int(y);
                    std::uint16_t e=0,color=0;
                    if(entry(bg,wx,wy,e) && gba::sample_text_entry(u16(io+8+bg*2),e,unsigned(wx),unsigned(wy),
                        line.vram.data(),line.vram.size(),line.pal.data(),line.pal.size(),color)) {
                        gba::text_rgb888(color,pixel);
                        background_priority=u16(io+8+bg*2)&3;
                        background_color=color;
                    }
                }
                // Keep authored black boundaries above objects. Unknown/effect
                // sprites are not extrapolated into the field-only host view.
                if(background_color && object_count) {
                    unsigned best=background_priority*256+128;
                    for(unsigned index=0;index<object_count;++index) {
                        const auto& obj=row_objects[index];
                        if(obj.key>=best) continue;
                        std::uint16_t color=0;
                        if(gba::sample_regular_obj(line.dispcnt,obj.a0,obj.a1,obj.a2,int(x)-left-obj.sx,int(y)-obj.sy,
                            line.vram.data(),line.vram.size(),line.pal.data(),line.pal.size(),color)) {
                            gba::text_rgb888(color,pixel); best=obj.key;
                        }
                    }
                }
            }
        }
        return true;
    }
};
}
