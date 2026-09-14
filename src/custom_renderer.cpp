#include "custom_renderer.h"
#include "gba_raster_capture.h"
#include "runtime.h"
#include "runtime_arm.h"
#include "custom_field_scene.h"
#include "custom_field_objects.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
std::unique_ptr<gba::GbaRasterCapture> capture;
std::array<std::uint8_t,gba::GbaPpu::kFramebufferBytes> output;
unsigned matches=0, mismatches=0, incomplete=0;
gbarecomp::ExtendedViewFrameInfo memory{};
swordcraft3::CustomFieldScene lake;
std::vector<std::uint8_t> wide_output;
unsigned host_width=0, lake_frames=0, fallback_frames=0;
bool wide_ready=false;
bool field_objects_enabled() {
    const char* enabled=std::getenv("SWORDCRAFT3_CUSTOM_OBJECTS");
    return host_width>240 && (!enabled || std::strcmp(enabled,"0")) && lake.objects_allowed(memory);
}
int field_object_read(std::uint32_t pc,std::uint32_t address,std::uint32_t size,
        std::uint32_t original,std::uint32_t* value) {
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
    if(!value || !field_objects_enabled()) return 0;
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
    wide_ready=false; lake.reset();
    if(capture) capture->reset();
}
void observe(const gba::NativeRasterLineContext& line) {
    if(line.y==0 && host_width>240) lake.capture(memory);
    capture->capture(line);
}
bool draw_host(const gbarecomp::HostFrameContext& frame) {
    if(!wide_ready || frame.width!=host_width || frame.height!=160 ||
        std::memcmp(frame.native_rgb,output.data(),output.size())) return false;
    std::memcpy(frame.output_rgb,wide_output.data(),wide_output.size());
    return true;
}
void present(std::uint8_t* stock, std::size_t bytes) {
    wide_ready=false;
    if (!capture->draw_native(output.data(),bytes)) { ++incomplete; return; }
    if (std::memcmp(stock,output.data(),bytes)!=0) {
        ++mismatches; // Never replace a picture that fails the native oracle.
        std::fprintf(stderr,"[sc3:custom] native mismatch=%u; stock retained\n",mismatches);
    } else {
        ++matches;
        std::memcpy(stock,output.data(),bytes);
        if(host_width>240) {
            wide_output.assign(std::size_t(host_width)*160*3,0);
            wide_ready=lake.draw(*capture,wide_output.data(),host_width);
            if(wide_ready) ++lake_frames; else ++fallback_frames;
            if(!wide_ready && std::getenv("SWORDCRAFT3_CUSTOM_AUDIT"))
                std::fprintf(stderr,"[sc3:host-decline] completed=%u reason=%s\n",matches,lake.decline_reason());
        }
    }
    if ((matches+mismatches)%60==0)
        std::fprintf(stderr,"[sc3:custom] matches=%u mismatches=%u incomplete=%u\n",matches,mismatches,incomplete);
    if (host_width>240 && (matches+mismatches)%60==0)
        std::fprintf(stderr,"[sc3:host] width=%u lake=%u fallback=%u guest=240 reason=%s\n",host_width,lake_frames,fallback_frames,lake.decline_reason());
    capture->reset();
}
void initialize(const gbarecomp::ExtendedViewFrameInfo* frame) {
    memory=frame ? *frame : gbarecomp::ExtendedViewFrameInfo{};
    if (!capture) capture=std::make_unique<gba::GbaRasterCapture>();
    gba::g_native_raster_observer=observe;
    gba::g_native_frame_presenter=present;
    g_runtime_thumb_alu_imm_override=field_object_limit;
    g_runtime_bus_read_override=field_object_read;
}
}
void configure_swordcraft3_custom_renderer(gbarecomp::RunOptions& opts) {
    const char* mode=std::getenv("SWORDCRAFT3_CUSTOM_RENDERER");
    if (!mode || std::strcmp(mode,"1")) return;
    host_width=0;
    if(const char* width=std::getenv("SWORDCRAFT3_CUSTOM_HOST_WIDTH")) {
        char* end=nullptr; const long parsed=std::strtol(width,&end,10);
        // Wider than 384 needs explicit unwrapped OBJ coordinates: positive
        // and wrapped-negative OAM X ranges eventually overlap.
        if(end!=width && *end=='\0' && parsed>240 && parsed<=384) host_width=unsigned(parsed);
    }
    matches=mismatches=incomplete=lake_frames=fallback_frames=0;
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
}
