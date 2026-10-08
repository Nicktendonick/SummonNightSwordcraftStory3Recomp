#include "custom_renderer.h"
#include "gba_raster_capture.h"
#include "runtime.h"
#include "runtime_arm.h"
#include "custom_field_scene.h"
#include "custom_field_objects.h"
#include "custom_battle_scene.h"
#include "guard_experiment.h"
#include "ram_write_override.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
std::unique_ptr<gba::GbaRasterCapture> capture;
std::array<std::uint8_t,gba::GbaPpu::kFramebufferBytes> output;
unsigned incomplete=0;
unsigned native_reused=0;
unsigned completed_frames=0,complete_frames=0;
gbarecomp::ExtendedViewFrameInfo memory{};
swordcraft3::CustomFieldScene lake;
swordcraft3::CustomBattleScene battle;
swordcraft3::BattleStateTracker battle_state;
swordcraft3::BattleSpellDisplayEpoch spell_display_epoch;
void (*previous_entry_hook)(std::uint32_t)=nullptr;
bool battle_hook_supported=false, state_trace=false;
bool battle_spell_window_supported=false;
bool guard_enabled=false, guard_supported=false, guard_trace=false;
int battle_camera_mode=0;
bool battle_edge_cover=false;
bool bounded_camera_supported=false,follow_camera_supported=false;
swordcraft3::GuardExperiment guard_context() {
    return {guard_enabled && guard_supported, memory.iwram, memory.iwram_size};
}
bool guard_write(std::uint32_t pc,std::uint32_t address,unsigned size,
        std::uint32_t original,std::uint32_t* value) {
    const bool changed=guard_context().write(pc,address,size,original,g_cpu.R[4],value);
    if(changed && guard_trace) std::fprintf(stderr,"[sc3:guard] preserve-slot=%u\n",*value);
    return changed;
}
unsigned battle_hook_calls=0;
void battle_entry(std::uint32_t pc) {
    if(previous_entry_hook) previous_entry_hook(pc);
    if(pc==0x080044e4 && battle_spell_window_supported) {
        spell_display_epoch.publish(memory.iwram,memory.iwram_size,memory.ewram,memory.ewram_size);
        return;
    }
    if(pc==0x0805e780 && battle_hook_supported) {
        // Results-panel initialization in sub_0805BC10 retires the arena's
        // VCOUNT schedule before the outer lifecycle reaches teardown.
        battle_state.reset(); battle.reset();
        if(state_trace) std::fprintf(stderr,"[sc3:state-exit] pc=0805e780 reason=result-panel\n");
        return;
    }
    if(pc!=0x08031bc8 || !battle_hook_supported) return;
    swordcraft3::BattleState state;
    if(!swordcraft3::read_battle_state(memory.iwram,memory.iwram_size,g_cpu.R[0],g_cpu.R[1],state)) return;
    battle_state.observe(state); ++battle_hook_calls;
    if(state_trace) std::fprintf(stderr,
        "[sc3:state-hook] call=%u pc=08031bc8 phase=%u mode=%u pause=%u arena=%u variant=%u planned=%u enabled=%u\n",
        battle_hook_calls,state.phase,state.mode,state.pause,state.arena,state.variant,state.top_switch,unsigned(state.enabled));
}
std::vector<std::uint8_t> wide_output;
unsigned host_width=0, lake_frames=0, battle_frames=0, fallback_frames=0;
bool wide_ready=false;
bool complete_frame_ready=false;
bool field_objects_enabled() {
    const char* enabled=std::getenv("SWORDCRAFT3_CUSTOM_OBJECTS");
    return host_width>240 && (!enabled || std::strcmp(enabled,"0")) && lake.objects_allowed(memory);
}
int field_object_read(std::uint32_t pc,std::uint32_t address,std::uint32_t size,
        std::uint32_t original,std::uint32_t* value) {
    if(guard_enabled && guard_context().read(pc,address,size,original,g_cpu.R[4],value)) {
        if(guard_trace && original!=*value)
            std::fprintf(stderr,"[sc3:guard] read pc=%08x original=%08x value=%08x\n",pc,original,*value);
        return 1;
    }
    const unsigned bit=swordcraft3::field_draw_visible_bit(pc);
    if(!bit || size!=2 || !value || (original&bit) || !field_objects_enabled()) return 0;
    const bool resources=pc==0x0809F01Eu;
    const unsigned member=pc==0x080A0070u ? 8 : 14;
    if(address!=g_cpu.R[resources?4:5]+member || address<0x02000000u || address>=0x02040000u) return 0;
    // The draw-list check must agree with the graphics/palette residency check.
    // Otherwise sub_0809F008 frees this NPC's tiles and palette while its OAM
    // still gets submitted, making another object's graphics appear on it.
    if(resources && g_cpu.R[14]!=0x0809FC7Bu) return 0;
    // sub_0809FB0C retains the actor rectangle at sp+0x10 throughout these
    // draw blocks. Validate both stack rectangles before using them.
    const unsigned sp=g_cpu.R[13]+(resources?20:0);
    if(sp<0x03000000u || sp>0x03007fe8u || !memory.iwram) return 0;
    const auto* stack=memory.iwram+(sp-0x03000000u);
    auto read=[&](unsigned off){return int(static_cast<std::int16_t>(gba::text_u16(stack+off)));};
    if(read(8)!=0 || read(10)!=0 || read(12)!=240 || read(14)!=160) return 0;
    if(!swordcraft3::field_rect_visible(read(16),read(18),read(20),read(22),host_width)) return 0;
    if(pc!=0x080A0070u) {
        const unsigned field=unsigned(memory.iwram[0x6b54])|(unsigned(memory.iwram[0x6b55])<<8)|
            (unsigned(memory.iwram[0x6b56])<<16)|(unsigned(memory.iwram[0x6b57])<<24);
        const unsigned npc=address-member;
        if(field<0x02000000u || !swordcraft3::field_npc_resource_capacity(memory.ewram,memory.ewram_size,
            memory.rom,memory.rom_size,field-0x02000000u,npc-0x02000000u)) return 0;
        // A failed native allocator must not authorize stale draw attributes.
        if(!resources && gba::text_u16(memory.ewram+(npc-0x02000000u)+0x14)==0xffff) return 0;
    }
    *value=original|bit; return 1;
}
int field_object_limit(std::uint32_t pc,std::uint32_t original,std::uint32_t* value) {
    if(!value) return 0;
    if(battle.active() && ((pc==0x08009B9E && original==239) || (pc==0x08009BB4 && original==64))) {
        const char* objects=std::getenv("SWORDCRAFT3_CUSTOM_OBJECTS");
        if(objects && !std::strcmp(objects,"0")) return 0;
        const char* full_combat=std::getenv("SWORDCRAFT3_FULL_COMBAT_RENDERER");
        if(battle_camera_mode==2 && follow_camera_supported && full_combat && !std::strcmp(full_combat,"1")) {
            // Constant union of every admissible Follow view, also containing
            // Current fallback. No next-frame camera prediction/epoch needed.
            const auto range=swordcraft3::battle_follow_object_range(host_width);
            if(range.enabled) {
                *value=pc==0x08009B9E ? unsigned(range.right_inclusive) : unsigned(-range.left_exclusive);
                return 1;
            }
        }
        // Keep the submitted coordinate interval asymmetric with the view.
        // Extending BOTH sides by the maximum shift admits >512 coordinates:
        // then positive and wrapped-negative OAM X become indistinguishable.
        unsigned anchor=(host_width-240)/2;
        if(battle_camera_mode==1 && bounded_camera_supported && memory.iwram &&
           memory.iwram_size>=0x1aa3 && memory.rom && memory.rom_size>=0xb80418) {
            const auto arena=memory.iwram[0x1a94];
            if(arena==battle.state().arena && memory.iwram[0x1aa2]==0) {
                const auto f=swordcraft3::battle_framing(host_width,
                    static_cast<std::int16_t>(gba::text_u16(memory.iwram+0x1a98)),
                    gba::text_u16(memory.rom+0xb801cc+arena*28+4),true);
                if(f.bounded) anchor=f.anchor;
            }
        }
        *value=pc==0x08009B9E ? host_width-anchor-1 : 64+anchor;
        return 1;
    }
    if(!field_objects_enabled()) return 0;
    if((pc==0x080091CE && original==239) || (pc==0x080091E0 && original==64)) {
        if(!swordcraft3::field_shadow_position_caller(g_cpu.R[14]) || !memory.iwram || memory.iwram_size<0x6b58) return 0;
        const unsigned field=unsigned(memory.iwram[0x6b54])|(unsigned(memory.iwram[0x6b55])<<8)|
            (unsigned(memory.iwram[0x6b56])<<16)|(unsigned(memory.iwram[0x6b57])<<24);
        // R3 is the shadow record at NPC + 0x44. The helper has saved R5, so
        // authenticate the record against the field's NPC pool, not live R5.
        const unsigned shadow=g_cpu.R[3];
        if(field<0x02000000u || shadow<0x02000044u ||
            !swordcraft3::field_npc_resource_capacity(memory.ewram,memory.ewram_size,
                memory.rom,memory.rom_size,field-0x02000000u,shadow-0x02000044u)) return 0;
        const unsigned npc=shadow-0x02000044u;
        if(gba::text_u16(memory.ewram+npc+0x14)==0xffff || !(gba::text_u16(memory.ewram+npc+0xe)&0x20)) return 0;
        *value=original+(host_width-240)/2; return 1;
    }
    // Only the two established OBJ draw-list cutoffs; no actor activation,
    // collision, camera or event checks are changed.
    if((pc==0x08009B9E && original==239) || (pc==0x08009BB4 && original==64)) {
        *value=original+(host_width-240)/2; return 1;
    }
    return 0;
}
void reset_host() {
    complete_frame_ready=false;
    wide_ready=false; lake.reset(); battle.reset(); battle_state.reset();
    spell_display_epoch.reset();
    if(capture) capture->reset();
}
void change_host_width(std::uint16_t width) {
    if (width != 240 && width != 284 && width != 384) return;
    host_width = width;
    reset_host();
}
void observe(const gba::NativeRasterLineContext& line) {
    if(line.y==0 && host_width>240) {
        swordcraft3::BattleState state;
        const bool owned=battle_state.latch(memory.iwram,memory.iwram_size,state);
        lake.capture(memory);
        battle.camera_mode(battle_camera_mode==2 ? (follow_camera_supported?2:0) :
                           (battle_camera_mode==1 && bounded_camera_supported?1:0));
        battle.authored_near_maps(follow_camera_supported);
        battle.cover_edges(battle_edge_cover);
        battle.capture(memory,state,owned);
    }
    battle.capture_spell_window(line,memory,battle_spell_window_supported,spell_display_epoch);
    capture->capture(line);
}
bool draw_host(const gbarecomp::HostFrameContext& frame) {
    if(!wide_ready || frame.width!=host_width || frame.height!=160) return false;
    std::memcpy(frame.output_rgb,wide_output.data(),wide_output.size());
    if(complete_frame_ready) {
        if(!frame.ownership) return false;
        *frame.ownership=gbarecomp::HostFrameOwnership::complete_frame;
        return true;
    }
    // Native scanout is authoritative. Reset invalidates retained margins on
    // load/rewind; no framebuffer comparison is used to recognize the scene.
    for(unsigned y=0;y<160;++y)
        std::memcpy(frame.output_rgb+(y*frame.width+frame.native_left)*3,frame.native_rgb+y*240*3,240*3);
    return true;
}
void present(std::uint8_t* stock, std::size_t bytes) {
    complete_frame_ready=false;
    wide_ready=false;
    if (!capture->complete() || bytes!=output.size()) { ++incomplete; return; }
    std::memcpy(output.data(),stock,bytes);
    ++completed_frames;
    if(host_width>240) {
        wide_output.assign(std::size_t(host_width)*160*3,0);
        wide_ready=lake.draw(*capture,wide_output.data(),host_width);
        if(state_trace) std::fprintf(stderr,"[sc3:field-frame] completed=%u valid=%u wide=%u decodes=%u animations=%u scene=%s reason=%s\n",
            completed_frames,unsigned(lake.valid()),unsigned(wide_ready),lake.source_decodes(),lake.animation_decodes(),lake.scene_name(),lake.decline_reason());
        if(wide_ready) {
            ++lake_frames;
            complete_frame_ready=lake.full_frame();
            if(state_trace) {
                const auto& stats=lake.render_stats();
                std::fprintf(stderr,"[sc3:field-composition] completed=%u complete_owner=%u rows=%u center=%u extended=%u center_objects=%u extended_objects=%u boundary_clips=%u\n",
                    completed_frames,unsigned(complete_frame_ready),stats.rows,stats.center_columns,stats.extended_columns,
                    stats.center_object_samples,stats.extended_object_samples,stats.boundary_clips);
            }
        }
        else {
            wide_ready=battle.draw(*capture,output.data(),wide_output.data(),host_width);
            complete_frame_ready=wide_ready && battle.full_frame();
            if(state_trace) {
                const auto& stats=battle.render_stats();
                std::fprintf(stderr,"[sc3:composition] completed=%u complete_owner=%u rows=%u center=%u extended=%u affine_rows=%u edge_columns=%u edge_rows=%u\n",
                    completed_frames,unsigned(complete_frame_ready),stats.rows,stats.center_columns,stats.extended_columns,stats.affine_rows,stats.edge_columns,stats.edge_rows);
                const auto& f=battle.framing();
                const auto& follow=battle.follow();
                std::fprintf(stderr,"[sc3:battle-camera] completed=%u enabled=%u supported=%u active=%u camera=%d anchor=%u begin=%u end=%u origin=%d width=%u camera_setting=%d h_min=%d h_max=%d source_width=%u object_wrap=%u h_flat=%d envelope_min=%d envelope_max=%d\n",
                    completed_frames,unsigned(battle_camera_mode!=0),unsigned(battle_camera_mode==2?follow_camera_supported:bounded_camera_supported),
                    unsigned(complete_frame_ready && f.bounded),battle.state().camera,f.anchor,f.begin,f.end,f.origin,host_width,
                    battle_camera_mode,follow.h_min,follow.h_max,battle.near_source_width(),f.object_wrap,
                    follow.h_flat,follow.envelope_min,follow.envelope_max);
                std::fprintf(stderr,"[sc3:battle-cover] completed=%u enabled=%u active=%u arena=%u anchor=%u base_begin=%u base_end=%u begin=%u end=%u flat=%d\n",
                    completed_frames,unsigned(battle_edge_cover),
                    unsigned(complete_frame_ready && follow.active &&
                        (f.begin!=follow.framing.begin || f.end!=follow.framing.end)),
                    battle.state().arena,f.anchor,follow.framing.begin,follow.framing.end,
                    f.begin,f.end,follow.h_flat);
            }
            battle.trace_raster(*capture,completed_frames);
            if(wide_ready) ++battle_frames; else ++fallback_frames;
        }
        if(!wide_ready && std::getenv("SWORDCRAFT3_CUSTOM_AUDIT"))
            std::fprintf(stderr,"[sc3:host-decline] completed=%u reason=%s battle=%s\n",completed_frames,lake.decline_reason(),battle.decline_reason());
    }
    if(state_trace) {
        const auto& s=battle.state();
        std::fprintf(stderr,"[sc3:state-frame] completed=%u hooks=%u arena=%u mode=%u pause=%u top=%u active=%u wide=%u reason=%s\n",
            completed_frames,battle_hook_calls,s.arena,s.mode,s.pause,battle.top_end(),unsigned(battle.active()),unsigned(wide_ready),battle.decline_reason());
    }
    if(complete_frame_ready) ++complete_frames; else ++native_reused;
    if (completed_frames%60==0)
        std::fprintf(stderr,"[sc3:custom] incomplete=%u native_reused=%u complete_frames=%u\n",incomplete,native_reused,complete_frames);
    if (host_width>240 && completed_frames%60==0)
        std::fprintf(stderr,"[sc3:host] width=%u lake=%u fallback=%u guest=240 reason=%s\n",host_width,lake_frames,fallback_frames,lake.decline_reason());
    if(host_width>240 && completed_frames%60==0)
        std::fprintf(stderr,"[sc3:battle] frames=%u reason=%s\n",battle_frames,battle.decline_reason());
    capture->reset();
}
void initialize(const gbarecomp::ExtendedViewFrameInfo* frame) {
    memory=frame ? *frame : gbarecomp::ExtendedViewFrameInfo{};
    if(g_runtime_fn_entry_hook!=battle_entry) {
        previous_entry_hook=g_runtime_fn_entry_hook;
        battle_hook_supported=swordcraft3::battle_hook_rom_supported(memory.rom,memory.rom_size);
        bounded_camera_supported=swordcraft3::battle_camera_rom_supported(memory.rom,memory.rom_size);
        follow_camera_supported=swordcraft3::battle_follow_rom_supported(memory.rom,memory.rom_size);
        battle_spell_window_supported=swordcraft3::battle_spell_window_rom_supported(memory.rom,memory.rom_size);
        guard_supported=guard_enabled && swordcraft3::guard_rom_supported(memory.rom,memory.rom_size);
        if(guard_enabled) std::fprintf(stderr,"[sc3:guard] Hold Select to Guard; authenticated=%u\n",unsigned(guard_supported));
        g_runtime_fn_entry_hook=battle_entry;
        if(state_trace) std::fprintf(stderr,"[sc3:state-hook] installed supported=%u\n",unsigned(battle_hook_supported));
    }
    if (!capture) capture=std::make_unique<gba::GbaRasterCapture>();
    gba::g_native_raster_observer=observe;
    gba::g_native_frame_presenter=present;
    g_runtime_thumb_alu_imm_override=field_object_limit;
    g_runtime_bus_read_override=field_object_read;
    gbarecomp::ram_write_override=guard_enabled && guard_supported ? guard_write : nullptr;
}
}
void set_swordcraft3_select_guard(bool enabled) {
    guard_enabled = enabled;
    std::fprintf(stderr, "[sc3:guard] preference=%s\n", enabled ? "on" : "off");
}
void set_swordcraft3_battle_camera_mode(int mode) {
    battle_camera_mode=mode>=0 && mode<=2?mode:0;
    // A live switch invalidates the old retained presentation, including rewind.
    reset_host();
    std::fprintf(stderr,"[sc3:battle-camera] preference=%s\n",battle_camera_mode==2?"follow-edge":battle_camera_mode==1?"bounded":"current");
}
void set_swordcraft3_battle_edge_cover(bool enabled) {
    battle_edge_cover=enabled;
    reset_host();
    std::fprintf(stderr,"[sc3:battle-cover] preference=%s\n",enabled?"on":"off");
}
void configure_swordcraft3_custom_renderer(gbarecomp::RunOptions& opts) {
    const char* mode=std::getenv("SWORDCRAFT3_CUSTOM_RENDERER");
    if (!mode || std::strcmp(mode,"1")) return;
    const char* guard=std::getenv("SWORDCRAFT3_SELECT_GUARD");
    guard_enabled=guard && !std::strcmp(guard,"1");
    const char* guard_log=std::getenv("SWORDCRAFT3_GUARD_TRACE");
    guard_trace=guard_log && !std::strcmp(guard_log,"1");
    host_width=0;
    if(const char* width=std::getenv("SWORDCRAFT3_CUSTOM_HOST_WIDTH")) {
        char* end=nullptr; const long parsed=std::strtol(width,&end,10);
        // Wider than 384 needs explicit unwrapped OBJ coordinates: positive
        // and wrapped-negative OAM X ranges eventually overlap.
        if(end!=width && *end=='\0' && parsed>240 && parsed<=384) host_width=unsigned(parsed);
    }
    incomplete=lake_frames=battle_frames=fallback_frames=native_reused=battle_hook_calls=0;
    completed_frames=complete_frames=0;
    const char* trace=std::getenv("SWORDCRAFT3_STATE_TRACE");
    state_trace=trace && !std::strcmp(trace,"1");
    std::fprintf(stderr,"[sc3:custom] game-state battle hook; native replay/visual checks removed\n");
    reset_host();
    // Guest scanout remains native; only field draw-list visibility is widened.
    // No legacy BG hooks, camera changes, interpolation or generated scenery.
    opts.max_view_width=opts.max_resize_view_width=240;
    opts.resize_driven_view=false;
    opts.launcher_expose_widescreen=opts.launcher_expose_adaptive_view=false;
    opts.launcher_num_aspects=0;
    opts.extended_view_init=nullptr;
    opts.extended_view_frame=initialize;
    opts.host_presentation_width=std::uint16_t(host_width);
    opts.host_frame_renderer=host_width>240 ? draw_host : nullptr;
    opts.host_frame_reset=reset_host;
    // Reuse the established 284-column 16:9 approximation at 160 rows. The
    // wider scanout is still host-only; native guest PPU stays 240x160.
    if (const char* portable = std::getenv("SWORDCRAFT3_BETA_LAUNCHER"); portable && !std::strcmp(portable,"1")) {
        static const char* const labels[] = {"Original GBA (3:2)", "Widescreen (16:9)", "Ultrawide (12:5)"};
        static const std::uint16_t widths[] = {240, 284, 384};
        opts.launcher_aspect_labels = labels;
        opts.launcher_aspect_view_widths = widths;
        opts.launcher_num_aspects = 3;
        opts.launcher_default_aspect = host_width == 384 ? 2 : host_width == 284 ? 1 : 0;
        opts.launcher_aspects_host_only = true;
        opts.launcher_expose_widescreen = true;
        opts.host_frame_renderer = draw_host;
        opts.host_frame_set_width = change_host_width;
    }
}
