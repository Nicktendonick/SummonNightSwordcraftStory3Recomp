#pragma once
#include "gba_raster_capture.h"
#include "gba_text_sample.h"
#include "sha1.h"
#include "custom_battle_state.h"
#include "battle_layer_policy.h"
#include "custom_field_objects.h"
#include "custom_battle_identity.h"
#include "custom_battle_spell_window.h"
#include "combat_frame_renderer.h"
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
    bool window_trace_=false;
    bool full_frame_=false;
    CombatRenderStats render_stats_{};
    std::array<BattleSpellWindow,160> spell_windows_{};
    mutable std::array<unsigned,160> effect_left_samples_{},effect_right_samples_{};
    static unsigned u16(const std::uint8_t* p) { return gba::text_u16(p); }
    static bool layout(unsigned display,unsigned bg0,unsigned bg1,unsigned bg2) {
        (void)bg2;
        return battle_arena_layout(display,bg0,bg1);
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
        // The guest's signed spell rectangle identifies the one actual
        // instance before its hardware window coordinates are clamped.
        const auto& spell=scene.spell_windows_[c->screen_y];
        if(spell.valid) {
            if(x<spell.left || x>=spell.right || int(c->screen_y)<spell.top ||
               int(c->screen_y)>=spell.bottom) return -1;
        } else {
            // Unknown 512-wide effects stay native-only; never apply the
            // 256-wide nearest-copy heuristic to their texture allocation.
            if(cnt==0x4305 || !battle_effect_contains(x,hofs,scene.effect_span_)) return -1;
        }
        // Accepted source candidates are a state-only diagnostic of permission,
        // not evidence that a tile produced a visible/nontransparent pixel.
        if(scene.window_trace_) {
            if(x<0) ++scene.effect_left_samples_[c->screen_y];
            else if(x>=240) ++scene.effect_right_samples_[c->screen_y];
        }
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
    static bool window_bounds(const void* user,unsigned window,unsigned y,
                              const std::uint8_t*,int* left,int* right) {
        const auto& scene=*static_cast<const CustomBattleScene*>(user);
        if(window!=0 || y>=160 || y<scene.top_end_ || y>=125) return false;
        const auto& spell=scene.spell_windows_[y];
        if(!spell.valid) return false;
        *left=spell.left; *right=spell.right; return true;
    }
    static unsigned layer_mask(const void* user,unsigned y,std::uint16_t display,
                                const std::uint8_t* io) {
        const auto& scene=*static_cast<const CustomBattleScene*>(user);
        if(y>=160 || !io) return 0;
        return battle_margin_layer_mask(display,u16(io+12),scene.spell_windows_[y].valid,
                                        y<scene.top_end_ || y>=125);
    }
    gba::GbaReplayViewPolicy view_policy() const {
        gba::GbaReplayViewPolicy policy;
        policy.user=this; policy.bg_margin=margin; policy.obj_x=object_x;
        policy.text_margin_entry=hud_entry; policy.clip_objects=false;
        // Only arena scenery/HUD wings may bypass a native background window.
        // BG2 spells keep WIN0/WIN1/WINOUT and blend-enable semantics.
        policy.regular_bg_window_bypass_layers=0x03;
        policy.window_margin_bounds=window_bounds;
        policy.margin_layer_mask=layer_mask;
        return policy;
    }
public:
    // Opt-in metadata only: the exact completed raster consumed by draw(),
    // including frames whose layout is rejected. No graphics/pixel inspection.
    void trace_raster(const gba::GbaRasterCapture& raster,unsigned completed) const {
        if(!window_trace_ || !active_ || !raster.complete()) return;
        const auto& gameplay=*raster.line(60);
        const unsigned effect_span=(gameplay.dispcnt&7)==0 ? span(gameplay.vram.data(),u16(gameplay.io.data()+12)) : 0;
        for(unsigned y=0;y<160;++y) {
            const auto& row=*raster.line(y); const auto* io=row.io.data();
            const auto& spell=spell_windows_[y];
            std::fprintf(stderr,"[sc3:battle-window] completed=%u y=%u arena=%u mode=%u display=%04x bg0=%04x bg1=%04x bg2=%04x hofs=%u vofs=%u win0h=%04x win1h=%04x win0v=%04x win1v=%04x winin=%04x winout=%04x blend=%04x alpha=%04x span=%u authored=%u layout=%u left_samples=%u right_samples=%u spell_window=%u spell_left=%d spell_right=%d spell_top=%d spell_bottom=%d spell_kind=%u margin_layers=%u effect_policy=%u\n",
                completed,y,state_.arena,state_.mode,row.dispcnt,u16(io+8),u16(io+10),u16(io+12),
                u16(io+0x18)&511,u16(io+0x1a)&511,u16(io+0x40),u16(io+0x42),u16(io+0x44),u16(io+0x46),
                u16(io+0x48),u16(io+0x4a),u16(io+0x50),u16(io+0x52),effect_span,
                view_policy().regular_bg_window_bypass_mask(),
                unsigned(layout(row.dispcnt,u16(io+8),u16(io+10),u16(io+12))),
                effect_left_samples_[y],effect_right_samples_[y],unsigned(spell.valid),
                spell.left,spell.right,spell.top,spell.bottom,spell.kind,
                layer_mask(this,y,row.dispcnt,io),
                unsigned(battle_effect_policy(row.dispcnt,u16(io+12),spell.valid)));
        }
    }
    void reset() { active_=false; decline_="not-captured"; spell_windows_.fill({}); }
    void capture_spell_window(const gba::NativeRasterLineContext& line,
                              const gbarecomp::ExtendedViewFrameInfo& m,bool supported,
                              const BattleSpellDisplayEpoch& epoch) {
        if(line.y>=160) return;
        spell_windows_[line.y]={};
        if(active_ && supported && line.io)
            spell_windows_[line.y]=epoch.read(
                m.iwram,m.iwram_size,m.ewram,m.ewram_size,line.io,0x400);
    }
    const char* decline_reason() const { return decline_; }
    bool active() const { return active_; }
    const BattleState& state() const { return state_; }
    unsigned top_end() const { return top_end_; }
    bool full_frame() const { return full_frame_; }
    const CombatRenderStats& render_stats() const { return render_stats_; }
    void capture(const gbarecomp::ExtendedViewFrameInfo& m,const BattleState& state,bool owned) {
        reset();
        const char* trace=std::getenv("SWORDCRAFT3_BATTLE_WINDOW_TRACE");
        window_trace_=trace && !std::strcmp(trace,"1");
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
        full_frame_=false; render_stats_={};
        if(window_trace_) { effect_left_samples_.fill(0); effect_right_samples_.fill(0); }
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
        const auto policy=view_policy();
        const char* full=std::getenv("SWORDCRAFT3_FULL_COMBAT_RENDERER");
        if(full && !std::strcmp(full,"1")) {
            full_frame_=CombatFrameRenderer::draw(raster,output,width,policy,render_stats_);
            decline_=full_frame_?"none":"full-compositor-unsupported";
            return full_frame_;
        }
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
