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
#include "battle_follow_camera.h"
#include "battle_scenery_edges.h"
#include "battle_scenery_repeat.h"
#include "battle_scenery_special.h"
#include "runtime.h"
#include <cstdio>
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
    int camera_mode_=0;
    unsigned arena_width_=0;
    bool authored_near_maps_=false;
    bool cover_edges_=false;
    // Repeat scenery (camera mode 3): Current framing, but the near and far
    // strips continue past their authored edges through tile-map plans.
    bool repeat_active_=false;
    SceneryRepeatPlan near_plan_{},far_plan_{};
    std::uint32_t near_key_=0,far_key_=0;
    unsigned plans_built_=0;
    unsigned arena_threshold_=0; // ROM arena table: perspective threshold d
    // Near insets are used only while no captured scenery row would show a
    // replaced edge column inside the native 240-column view.
    bool near_left_inset_=true,near_right_inset_=true;
    int inset_h_min_=0,inset_h_max_=0;
    unsigned inset_lines_=0; // captured scenery lines on inset rows
    mutable unsigned repeat_near_samples_=0,repeat_far_samples_=0,repeat_inset_samples_=0;
    BattleFraming framing_{};
    BattleFollowResult follow_{};
    CombatRenderStats render_stats_{};
    std::array<BattleSpellWindow,160> spell_windows_{};
    mutable std::array<unsigned,160> effect_left_samples_{},effect_right_samples_{};
    static unsigned u16(const std::uint8_t* p) { return gba::text_u16(p); }
    BattleBackdropLayout backdrop_{}; // from the ROM arena descriptor
    // Backdrop columns its scroll can never show (0 when unknown).
    unsigned far_overscan_left_=0,far_overscan_right_=0;
    std::array<std::uint8_t,0x200> target_palette_{}; // BG half of the palette module's target
    bool target_palette_valid_=false;
    bool layout(unsigned display,unsigned bg0,unsigned bg1,unsigned bg2) const {
        (void)bg2;
        return battle_arena_layout(display,bg0,bg1,backdrop_);
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
    static void trace_plan(const char* layer,unsigned arena,std::uint32_t key,std::uint32_t map,
                           const SceneryRepeatPlan& plan,bool target_palette) {
        char sides[2][SceneryRepeatSide::max_rows*5+1]{};
        for(unsigned s=0;s<2;++s) {
            const auto& side=s?plan.right:plan.left;
            unsigned used=0;
            for(unsigned y=0;y<plan.rows;++y)
                used+=unsigned(std::snprintf(sides[s]+used,sizeof(sides[s])-used,"%s%c%u",y?",":"",
                    side.rows[y].quilt?'q':side.rows[y].sequence?'s':side.rows[y].mirror?'m':'w',
                    unsigned(side.rows[y].quilt?side.quilt.source:side.rows[y].span)));
        }
        std::fprintf(stderr,"[sc3:battle-repeat] layer=%s arena=%u valid=%u palette=%s columns=%u rows=%u band=%u reach=%u key=%08x map=%08x right_cost=%u right_unauthored=%u left_cost=%u left_unauthored=%u inset_rows=%u right_inset=%u left_inset=%u right_padding=%u left_padding=%u right_edge_cost=%u right_edge_unauthored=%u left_edge_cost=%u left_edge_unauthored=%u right=%s left=%s\n",
            layer,arena,unsigned(plan.valid),target_palette?"target":"live",plan.columns,plan.rows,plan.band,plan.reach,
            unsigned(key),unsigned(map),
            plan.right.cost,plan.right.unauthored,plan.left.cost,plan.left.unauthored,
            plan.inset_rows,plan.right.inset,plan.left.inset,plan.right.padding,plan.left.padding,
            plan.right_edge.cost,plan.right_edge.unauthored,
            plan.left_edge.cost,plan.left_edge.unauthored,sides[1],sides[0]);
    }
    // Edge columns of the near map's flat rows that no ordinary camera shows
    // natively: the authenticated scheduler scrolls every row above the
    // arena's threshold d by Hflat, so [0,Hmin) and [Hmax+240,width) are only
    // ever seen in the host margins. Per-frame checks still guard each frame.
    SceneryRepeatInsets near_insets(unsigned columns) const {
        const auto band=battle_flat_band(arena_threshold_);
        if(!band.valid) return {};
        const int right=int(columns*8)-240-band.h_max;
        return {std::min(band.rows,20u),unsigned(std::max(0,band.h_min))/8,
                right>0?unsigned(right)/8:0u};
    }
    // Plans come from the authored map ENTRIES in VRAM and the ROM arena
    // threshold; joins the artist never placed are scored on how well the
    // touching tile edges match (the layer's tile graphics and BG palette,
    // read only to choose continuations, never to assert or detect scenes).
    // Floors repeat with one period per side in every arena; arenas 6, 7, 13
    // and 14 then take their reviewed band sequences (battle_scenery_special).
    // Each layer is planned once per battle. Arenas with a tile-animation
    // asset (08031420, descriptor byte 2; arena 2's near map) rewrite map
    // entries every few frames: re-planning on each change cost ~10 ms a
    // time and could switch rules between frames. The animated cells still
    // show live through the fixed column rules.
    bool update_repeat_plans(const std::uint8_t* vram,const std::uint8_t* pal) {
        const unsigned columns=near_source_width()/8,far_rows=battle_far_source_rows(state_.arena);
        if(!vram || !pal || columns!=48 || (backdrop_.enabled && !far_rows)) return false;
        std::array<std::uint16_t,SceneryRepeatPlan::max_columns*SceneryRepeatSide::max_rows> entries{};
        // Near: rows above the ROM threshold d are scenery, the rest is the
        // perspective floor. Far: the whole backdrop repeats as one band.
        auto refresh=[&](const char* name,unsigned cnt,unsigned columns,unsigned rows,SceneryRepeatInsets insets,
                         unsigned floor_row,SceneryRepeatPlan& plan,std::uint32_t& key) {
            const std::uint32_t identity=(state_.arena<<24)^(rows<<16)^
                (insets.rows<<10)^(insets.left<<5)^insets.right;
            if(plan.valid && identity==key) return true;
            for(unsigned y=0;y<rows;++y) for(unsigned x=0;x<columns;++x)
                if(!gba::text_ring_entry(cnt,x*8,y*8,vram,0x18000,entries[y*columns+x])) return false;
            const std::size_t chars=std::size_t((cnt>>2)&3u)*0x4000; // BG tiles end at 0x10000
            SceneryTileArt art; art.chars=vram+chars; art.size=0x10000-chars; art.palette=pal;
            plan=plan_scenery_repeat(entries.data(),columns,rows,10,insets,floor_row,art,true);
            const auto special=battle_scenery_special(state_.arena,std::strcmp(name,"far")==0);
            if(special.left.length) scenery_repeat_apply_sequence(plan,false,special.left);
            if(special.right.length) scenery_repeat_apply_sequence(plan,true,special.right);
            for(int s=0;s<2;++s) {
                const auto& q=s ? special.quilt_right : special.quilt_left;
                if(q.enabled) scenery_repeat_apply_quilt(plan,s==1,entries.data(),art,q.run_first,q.run_last,q.overlap);
            }
            key=identity; ++plans_built_;
            trace_plan(name,state_.arena,key,scenery_repeat_hash(entries.data(),columns*rows),plan,target_palette_valid_);
            return plan.valid;
        };
        const auto insets=near_insets(columns);
        if(!refresh("near",0x470b,columns,20,insets,insets.rows,near_plan_,near_key_)) return false;
        if(!backdrop_.enabled) { far_plan_.valid=false; return true; } // arena 15: no backdrop
        // Backdrop columns beyond its scroll range are padding (arena 16's
        // backdrop never scrolls; 11/12 never show their last two columns).
        const SceneryRepeatInsets far{far_rows,far_overscan_left_,far_overscan_right_,true};
        return refresh("far",backdrop_.cnt,battle_far_source_width(state_.arena)/8,far_rows,far,far_rows,
                       far_plan_,far_key_);
    }
    // Replaced near columns must stay outside the native view on every
    // captured scenery row that uses them; otherwise that side uses its
    // inset-free plan for the whole frame. Reads the completed raster's BG0
    // scroll registers only.
    void check_repeat_insets(const gba::GbaRasterCapture& raster) {
        SceneryInsetView view;
        inset_h_min_=512; inset_h_max_=-512;
        for(unsigned y=top_end_;y<125;++y) {
            const auto& line=*raster.line(y);
            const auto* io=line.io.data();
            if(!(line.dispcnt&0x100) || u16(io+8)!=0x470b) continue;
            const unsigned vofs=u16(io+0x12);
            if(!battle_near_row_reviewed(y,vofs)) continue;
            const unsigned row=((y+(vofs&511))&255u)>>3;
            if(row>=near_plan_.inset_rows) continue;
            const unsigned raw=u16(io+0x10)&511;
            const int h=raw>=384 ? int(raw)-512 : int(raw);
            inset_h_min_=std::min(inset_h_min_,h); inset_h_max_=std::max(inset_h_max_,h);
            ++inset_lines_;
            scenery_repeat_inset_view(near_plan_,row,h,240,view);
        }
        near_left_inset_=view.left; near_right_inset_=view.right;
        if(!inset_lines_) inset_h_min_=inset_h_max_=0;
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
            const int near_width=int(scene.near_source_width());
            const int source=x+camera;
            // Finite near map, except in Repeat scenery: continue the tile row
            // through its planned authored period or reflection, from its
            // edge or from a planned inset that only margins ever show.
            if(scene.repeat_active_) {
                const unsigned line=(c->screen_y+(u16(c->io+0x12)&511))&255u,row=line>>3;
                const int mapped=scenery_repeat_x(scene.near_plan_,row,source,
                    scene.near_left_inset_,scene.near_right_inset_,int(line));
                if(mapped<0 || mapped>=near_width) return -1;
                if(mapped!=source) {
                    ++scene.repeat_near_samples_;
                    if(source>=0 && source<near_width) ++scene.repeat_inset_samples_;
                }
                *out=mapped-camera; return 1;
            }
            if(source>=0 && source<near_width) { *out=x; return 1; }
            return -1;
        }
        if(c->layer==1) {
            if(scene.repeat_active_ && scene.far_plan_.valid) {
                const int camera=hofs>=384 ? int(hofs)-512 : int(hofs);
                const int source=x+camera,width=int(scene.far_plan_.columns)*8;
                const unsigned line=(c->screen_y+(u16(c->io+0x16)&511))&255u,row=line>>3;
                // Every margin column goes through the plan: it also replaces
                // overscan columns the backdrop's scroll never shows.
                if(row<scene.far_plan_.rows) {
                    const int mapped=scenery_repeat_x(scene.far_plan_,row,source,true,true,int(line));
                    if(mapped!=source) ++scene.repeat_far_samples_;
                    *out=mapped-camera; return 1;
                }
                // Rows past the reviewed far-map height keep the old loop.
                if(source>=0 && source<width) { *out=x; return 1; }
            }
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
    void camera_mode(int mode) { camera_mode_=mode; }
    void authored_near_maps(bool enabled) { authored_near_maps_=enabled; }
    void cover_edges(bool enabled) { cover_edges_=enabled; }
    unsigned near_source_width() const { return battle_near_source_width(state_.arena,authored_near_maps_); }
    const BattleFraming& framing() const { return framing_; }
    const BattleFollowResult& follow() const { return follow_; }
    bool repeat_active() const { return repeat_active_; }
    unsigned repeat_near_samples() const { return repeat_near_samples_; }
    unsigned repeat_far_samples() const { return repeat_far_samples_; }
    unsigned repeat_inset_samples() const { return repeat_inset_samples_; }
    const SceneryRepeatPlan& repeat_near_plan() const { return near_plan_; }
    bool repeat_left_inset() const { return near_left_inset_; }
    bool repeat_right_inset() const { return near_right_inset_; }
    int repeat_inset_h_min() const { return inset_h_min_; }
    int repeat_inset_h_max() const { return inset_h_max_; }
    unsigned repeat_inset_lines() const { return inset_lines_; }
    unsigned arena_threshold() const { return arena_threshold_; }
    std::uint32_t repeat_near_key() const { return near_key_; }
    std::uint32_t repeat_far_key() const { return far_key_; }
    unsigned repeat_plans_built() const { return plans_built_; }
    void capture(const gbarecomp::ExtendedViewFrameInfo& m,const BattleState& state,bool owned) {
        reset();
        const char* trace=std::getenv("SWORDCRAFT3_BATTLE_WINDOW_TRACE");
        window_trace_=trace && !std::strcmp(trace,"1");
        state_=state;
        arena_width_=0; arena_threshold_=0; backdrop_={}; far_overscan_left_=far_overscan_right_=0;
        target_palette_valid_=false;
        const char* enabled=std::getenv("SWORDCRAFT3_CUSTOM_BATTLES");
        if(!enabled || std::strcmp(enabled,"1")) { decline_="disabled"; return; }
        if(!owned) {
            // Battle over: the next battle plans its maps afresh.
            near_plan_.valid=far_plan_.valid=false;
            decline_="no-battle-owner"; return;
        }
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
        if(m.rom && m.rom_size>=0xb80418) {
            const std::uint8_t* d=m.rom+0xb801cc+state.arena*28;
            arena_width_=u16(d+4);
            arena_threshold_=d[3];
            backdrop_.enabled=d[0x11]!=0xff;
            backdrop_.cnt=(0x450bu&~3u)|(d[0x12]&3u);
            // 08031810's BG1 modes (descriptor byte 0x13): 1 and 9 scroll by
            // byte 0x14 * camera / 128, 2 by 1.5 * camera, 0, 5-8 and >9 stay
            // at byte 0x14. Mode 3 auto-scrolls through the whole map and 4
            // follows the perspective formula: no overscan is claimed there.
            const unsigned mode=d[0x13],k=u16(d+0x14),camera=arena_width_>256 ? arena_width_-256 : 0;
            bool known=true; unsigned h_min=0,h_max=0;
            if(mode==1 || mode==9) h_max=(k*camera)>>7;
            else if(mode==2) h_max=camera*3/2;
            else if(mode==0 || (mode>=5 && mode<=8) || mode>9) h_min=h_max=k;
            else known=false;
            far_overscan_left_=far_overscan_right_=0;
            const unsigned far_columns=battle_far_source_width(state.arena)/8,seen=(h_max+240+7)/8;
            if(known && backdrop_.enabled && seen<far_columns) {
                far_overscan_left_=std::min(h_min/8,SceneryRepeatPlan::max_inset);
                far_overscan_right_=std::min(far_columns-seen,SceneryRepeatPlan::max_inset);
            }
            // The palette module (base 03005960, JP and 1.1.0f alike) keeps
            // the tinted target palette there; 08010CD4 copies it to +0x400,
            // applies the fade (080119D0) and only that copy reaches palette
            // RAM. Battles fade in from black over ~30 frames and plans are
            // built on the first owned frame, so score tile edges with the
            // target colours. Verified by the routine's own literal.
            const std::uint8_t* lit=m.rom+0x10d08;
            target_palette_valid_=m.iwram && m.iwram_size>=0x5960+0x200 &&
                (unsigned(lit[0])|(unsigned(lit[1])<<8)|(unsigned(lit[2])<<16)|(unsigned(lit[3])<<24))==0x03005960u;
            if(target_palette_valid_) std::memcpy(target_palette_.data(),m.iwram+0x5960,target_palette_.size());
        }
    }
    bool draw(const gba::GbaRasterCapture& raster,const std::uint8_t* native,
              std::uint8_t* output,unsigned width) {
        full_frame_=false; render_stats_={}; framing_={}; follow_={};
        repeat_active_=false; repeat_near_samples_=repeat_far_samples_=repeat_inset_samples_=0;
        near_left_inset_=near_right_inset_=false; inset_h_min_=inset_h_max_=0; inset_lines_=0;
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
        // Authored arenas: sub_08031420 loads backdrops with declared widths
        // (384; arena 14: 512). Retain each row's own scroll; repeat that
        // authored strip only.
        backdrop_period_=battle_uses_authored_384(state_.arena) ? battle_far_source_width(state_.arena) :
            period(first.vram.data(),0x450b,span(first.vram.data(),0x450b));
        const auto& gameplay=*raster.line(60);
        effect_span_=(gameplay.dispcnt&7)==0 ? span(gameplay.vram.data(),u16(gameplay.io.data()+12)) : 0;
        // Repeat scenery keeps Current's centered anchor and full width; only
        // the margin source columns change. Exact-ROM arenas only.
        if(camera_mode_==3 && authored_near_maps_)
            repeat_active_=update_repeat_plans(gameplay.vram.data(),
                target_palette_valid_ ? target_palette_.data() : gameplay.pal.data());
        if(repeat_active_) check_repeat_insets(raster);
        const auto policy=view_policy();
        const char* full=std::getenv("SWORDCRAFT3_FULL_COMBAT_RENDERER");
        if(full && !std::strcmp(full,"1")) {
            framing_=battle_framing(width,state_.camera,arena_width_,camera_mode_==1 && state_.camera_mode==0);
            if(camera_mode_==2) {
                int h_min=512,h_max=-512,h_flat=-1;
                unsigned near_rows=0,flat_rows=0;
                bool coherent_flat=true;
                // Read the completed row state that the compositor consumes.
                // Do not use the next scheduled scroll buffer or guess pixels.
                for(unsigned y=top_end_;y<125;++y) {
                    const auto& row=*raster.line(y);
                    if((row.dispcnt&0x80) || !(row.dispcnt&0x100)) continue;
                    const unsigned raw=u16(row.io.data()+0x10)&511;
                    const int h=raw>=384 ? int(raw)-512 : int(raw);
                    h_min=std::min(h_min,h); h_max=std::max(h_max,h); ++near_rows;
                    if(y<96) {
                        if(flat_rows && h!=h_flat) coherent_flat=false;
                        h_flat=h; ++flat_rows;
                    }
                }
                // The follow envelope is derived for thresholds 128/136 only;
                // other arenas (1, 11-16) keep Current framing throughout
                // instead of engaging on a few aliased Hflat values.
                const bool ordinary=near_rows && arena_width_==384 &&
                    (arena_threshold_==128 || arena_threshold_==136) &&
                    state_.camera_mode==0 && state_.camera>=0 && state_.camera<=128;
                follow_=battle_follow_framing(width,h_min,h_max,
                    ordinary && flat_rows && coherent_flat ? h_flat : -1);
                framing_=battle_scenery_cover(follow_,width,state_.arena,
                    cover_edges_ && authored_near_maps_);
            }
            full_frame_=CombatFrameRenderer::draw(raster,output,width,policy,render_stats_,&framing_,top_end_);
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
