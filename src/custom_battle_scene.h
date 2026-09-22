#pragma once
#include "gba_raster_capture.h"
#include "gba_text_sample.h"
#include "sha1.h"
#include "custom_battle_state.h"
#include "battle_layer_policy.h"
#include "custom_field_objects.h"
#include "custom_battle_identity.h"
#include "runtime.h"
#include <cstring>
#include <cstdlib>

namespace swordcraft3 {
// Scoped combat pilot: game-owned battle state, immutable raster replay.
// No framebuffer recognition, camera, collision or actor activation edits.
class CustomBattleScene {
    bool active_=false;
    unsigned backdrop_period_=0, effect_span_=0;
    const char* decline_="not-captured";
    BattleState state_{};
    unsigned top_end_=19;
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
        if(c->layer>2) return -1;
        const bool hud=c->screen_y<scene.top_end_ || c->screen_y>=125;
        if(hud) {
            if(c->layer!=0) return -1;
            *out=0; return 1;
        }
        const int x=int(c->output_x)-int(c->native_left);
        const unsigned cnt=u16(c->io+8+c->layer*2),hofs=u16(c->io+0x10+c->layer*4)&511;
        if(c->layer==0) {
            if(cnt!=0x470b) return -1; // HUD, including expanded START pause
            if(!battle_near_row_reviewed(c->screen_y,u16(c->io+0x12))) return -1;
            const int camera=hofs>=384 ? int(hofs)-512 : int(hofs);
            // Reviewed new arena headers declare 384x160; the 512-wide VRAM
            // allocation includes padding, not additional authored terrain.
            const int near_width=battle_uses_authored_384(scene.state_.arena) ? 384 : 512;
            if(x+camera<0 || x+camera>=near_width) return -1;
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
    static int hud_entry(const void* user,const gba::WsBgMarginContext* c,std::uint16_t* out) {
        const auto& scene=*static_cast<const CustomBattleScene*>(user);
        if(c->layer!=0 || (c->screen_y>=scene.top_end_ && c->screen_y<125)) return 0;
        *out=battle_hud_margin_entry(c->screen_y,scene.top_end_); return 1;
    }
public:
    void reset() { active_=false; decline_="not-captured"; }
    const char* decline_reason() const { return decline_; }
    bool active() const { return active_; }
    const BattleState& state() const { return state_; }
    unsigned top_end() const { return top_end_; }
    void capture(const gbarecomp::ExtendedViewFrameInfo& m,const BattleState& state,bool owned) {
        reset();
        state_=state;
        const char* enabled=std::getenv("SWORDCRAFT3_CUSTOM_BATTLES");
        if(!enabled || std::strcmp(enabled,"1")) { decline_="disabled"; return; }
        if(!owned) { decline_="no-battle-owner"; return; }
        if(!battle_state_supported(state)) { decline_="unsupported-battle-state"; return; }
        const char* additional=std::getenv("SWORDCRAFT3_CUSTOM_ADDITIONAL_AREAS");
        if(battle_uses_authored_384(state.arena) && additional && !std::strcmp(additional,"0")) {
            decline_="additional-area-disabled"; return;
        }
        if(!m.io || m.io_size<0x10 || !m.vram || m.vram_size<0x4800) return;
        const char* rocky=std::getenv("SWORDCRAFT3_CUSTOM_ROCKY");
        if(state.arena==3 && rocky && !std::strcmp(rocky,"0")) {
            decline_="rocky-disabled"; return;
        }
        active_=true; decline_="none";
    }
    bool draw(const gba::GbaRasterCapture& raster,const std::uint8_t* native,
              std::uint8_t* output,unsigned width) {
        if(!active_ || !raster.complete() || width<=240 || width>384) return false;
        const auto& first=*raster.line(0);
        // Read the actual raster register transition, not the next frame's
        // planned boundary or the colors of the pause/ability menu.
        top_end_=125;
        for(unsigned y=0;y<160;++y) {
            const auto& line=*raster.line(y);
            const unsigned bg0=u16(line.io.data()+8);
            if(!layout(line.dispcnt,bg0,u16(line.io.data()+10),u16(line.io.data()+12))) {
                decline_="raster-layout-change"; return false;
            }
            if(y<125 && bg0==state_.scenery_cnt && top_end_==125) top_end_=y;
            // A future layout must not sample excluded HUD storage as scenery.
            if(y>=19 && y<125 && (line.dispcnt&0x100) && u16(line.io.data()+8)==0x470b &&
               !battle_near_row_reviewed(y,u16(line.io.data()+0x12))) {
                decline_="unreviewed-near-row"; return false;
            }
        }
        if(top_end_<19 || top_end_>59 || (top_end_-19)%8) {
            decline_="unsupported-hud-schedule"; return false;
        }
        for(unsigned y=0;y<160;++y) {
            const unsigned expected=(y<top_end_ || y>=125)?state_.hud_cnt:state_.scenery_cnt;
            if(u16(raster.line(y)->io.data()+8)!=expected) {
                decline_="incomplete-hud-schedule"; return false;
            }
        }
        // Arenas 2/7: sub_08031420 loads assets 34/39 with 384x160 headers.
        // Retain each row's own scroll; repeat that authored strip only.
        backdrop_period_=battle_uses_authored_384(state_.arena) ? 384 : period(first.vram.data(),0x450b,span(first.vram.data(),0x450b));
        const auto& gameplay=*raster.line(60);
        effect_span_=(gameplay.dispcnt&7)==0 ? span(gameplay.vram.data(),u16(gameplay.io.data()+12)) : 0;
        gba::GbaReplayViewPolicy policy;
        policy.user=this; policy.bg_margin=margin; policy.obj_x=object_x;
        policy.text_margin_entry=hud_entry;
        policy.authored_backgrounds=true; policy.clip_objects=false;
        if(!raster.draw_view(output,std::size_t(width)*160*3,width,policy)) return false;
        const unsigned left=(width-240)/2;
        for(unsigned y=0;y<160;++y) {
            // Preserve the stock center by construction, not a visual oracle.
            std::memcpy(output+(y*width+left)*3,native+y*240*3,240*3);
        }
        decline_="none"; return true;
    }
};
}
