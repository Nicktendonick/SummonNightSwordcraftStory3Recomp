#pragma once
#include "gba_raster_capture.h"
#include "gba_text_sample.h"
#include "sha1.h"
#include "battle_hud_borders.h"
#include "battle_layer_policy.h"
#include "custom_field_objects.h"
#include "custom_battle_identity.h"
#include "runtime.h"
#include <cstring>
#include <cstdlib>

namespace swordcraft3 {
// First combat pilot: authenticated forest arena, complete immutable raster
// replay, native HUD/center oracle. No camera, collision or actor activation edits.
class CustomBattleScene {
    bool active_=false;
    unsigned backdrop_period_=0, effect_span_=0;
    const char* decline_="not-captured";
    static unsigned u16(const std::uint8_t* p) { return gba::text_u16(p); }
    static bool layout(unsigned display,unsigned bg0,unsigned bg1,unsigned bg2) {
        // BG2 turns off briefly when the critical canvas is removed. BG0/BG1
        // still contain the same arena; do not drop widescreen in that gap.
        // DISPCNT and BG2CNT are not changed atomically at effect teardown.
        // Both reviewed canvas descriptors may be present during that switch;
        // immutable replay still uses each row's actual mode and attributes.
        return ((display&0x0b86)==0x0300 && (display&7)<=1 &&
                (bg2==0x0305 || bg2==0x4385)) &&
               bg1==0x450b && (bg0==0 || bg0==0x470b);
    }
    static bool columns_equal(const std::uint8_t* vram,unsigned cnt,unsigned a,unsigned b) {
        const unsigned rows=(cnt&0x8000)?64:32;
        for(unsigned y=0;y<rows;++y) {
            std::uint16_t ea=0,eb=0;
            if(!gba::text_ring_entry(cnt,a*8,y*8,vram,0x18000,ea) ||
               !gba::text_ring_entry(cnt,b*8,y*8,vram,0x18000,eb) || ea!=eb) return false;
        }
        return true;
    }
    static unsigned span(const std::uint8_t* vram,unsigned cnt) {
        const unsigned columns=(cnt&0x4000)?64:32;
        unsigned start=columns-1;
        while(start && columns_equal(vram,cnt,start-1,columns-1)) --start;
        return (columns-start>=2?start:columns)*8;
    }
    static unsigned period(const std::uint8_t* vram,unsigned cnt,unsigned width) {
        const unsigned columns=width/8;
        for(unsigned p=1;p+8<=columns;++p) {
            bool match=true;
            for(unsigned x=p;x<columns && match;++x) match=columns_equal(vram,cnt,x,x%p);
            if(match) return p*8;
        }
        return width;
    }
    static int margin(const void* user,const gba::WsBgMarginContext* c,int* out) {
        const auto& scene=*static_cast<const CustomBattleScene*>(user);
        if(c->screen_y<19 || c->screen_y>=125 || c->layer>2) return -1;
        const int x=int(c->output_x)-int(c->native_left);
        const unsigned cnt=u16(c->io+8+c->layer*2),hofs=u16(c->io+0x10+c->layer*4)&511;
        if(c->layer==0) {
            if(cnt!=0x470b) return -1; // HUD, including expanded START pause
            if(!battle_near_row_reviewed(c->screen_y,u16(c->io+0x12))) return -1;
            const int camera=hofs>=384 ? int(hofs)-512 : int(hofs);
            if(x+camera<0 || x+camera>=512) return -1;
            *out=x; return 1; // finite near map, never mirrored or looped
        }
        if(c->layer==1) {
            if(!scene.backdrop_period_) return -1;
            *out=battle_backdrop_x(x,hofs,scene.backdrop_period_); return 1;
        }
        // The attack canvas is not a background: show one unwrapped instance.
        if(!battle_effect_contains(x,hofs,scene.effect_span_)) return -1;
        *out=x; return 1;
    }
    static int object_x(int raw,int* out) {
        *out=field_object_x(unsigned(raw),384); return 1;
    }
public:
    void reset() { active_=false; decline_="not-captured"; }
    const char* decline_reason() const { return decline_; }
    bool active() const { return active_; }
    void capture(const gbarecomp::ExtendedViewFrameInfo& m) {
        reset();
        const char* enabled=std::getenv("SWORDCRAFT3_CUSTOM_BATTLES");
        if(!enabled || std::strcmp(enabled,"1")) { decline_="disabled"; return; }
        if(!m.io || m.io_size<0x10 || !m.vram || m.vram_size<0x4800 ||
           !layout(u16(m.io),u16(m.io+8),u16(m.io+10),u16(m.io+12))) return;
        // Authenticate scenery, not shared HUD glyphs in the unused lower rows.
        active_=battle_near_identity(m.vram,m.vram_size)=="b649c3c5aa90b71aed5dae28449d9eff8ee40576" &&
            gba::sha1(m.vram+0x2800,0x1000).hex()=="c25d865a7ad8e6c20d9804f6c2aacca32ff09a70";
        decline_=active_?"none":"unknown-arena";
    }
    bool draw(const gba::GbaRasterCapture& raster,const std::uint8_t* native,
              std::uint8_t* output,unsigned width) {
        if(!active_ || !raster.complete() || width<=240 || width>384) return false;
        const auto& first=*raster.line(0);
        for(unsigned y=0;y<160;++y) {
            const auto& line=*raster.line(y);
            if(!layout(line.dispcnt,u16(line.io.data()+8),u16(line.io.data()+10),u16(line.io.data()+12)) ||
               !battle_scenery_rows_equal(first.vram.data(),line.vram.data(),line.vram.size())) {
                decline_="raster-layout-change"; return false;
            }
            // A future layout must not sample excluded HUD storage as scenery.
            if(y>=19 && y<125 && (line.dispcnt&0x100) && u16(line.io.data()+8)==0x470b &&
               !battle_near_row_reviewed(y,u16(line.io.data()+0x12))) {
                decline_="unreviewed-near-row"; return false;
            }
        }
        backdrop_period_=period(first.vram.data(),0x450b,span(first.vram.data(),0x450b));
        const auto& gameplay=*raster.line(60);
        effect_span_=(gameplay.dispcnt&7)==0 ? span(gameplay.vram.data(),u16(gameplay.io.data()+12)) : 0;
        gba::GbaReplayViewPolicy policy;
        policy.user=this; policy.bg_margin=margin; policy.obj_x=object_x;
        policy.authored_backgrounds=true; policy.clip_objects=false;
        if(!raster.draw_view(output,std::size_t(width)*160*3,width,policy)) return false;
        const unsigned left=(width-240)/2;
        for(unsigned y=0;y<160;++y) {
            if(std::memcmp(output+(y*width+left)*3,native+y*240*3,240*3)) {
                decline_="native-center-mismatch"; return false;
            }
        }
        if(!extend_battle_hud_borders(output,width,160,left,width-240-left)) {
            decline_="unrecognized-hud"; return false;
        }
        decline_="none"; return true;
    }
};
}
