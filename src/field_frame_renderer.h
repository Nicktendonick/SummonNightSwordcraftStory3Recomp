#pragma once
#include "gba_raster_capture.h"
#include "gba_render_primitives.h"
#include "custom_field_objects.h"
#include <array>

namespace swordcraft3 {
struct FieldRenderStats {
    unsigned rows=0,center_columns=0,extended_columns=0;
    unsigned center_object_samples=0,extended_object_samples=0,boundary_clips=0;
};

// This game's field compositor. Source authorisation and animation reconciliation
// happen in CustomFieldScene before drawing; stateless hardware decoding remains
// in gbarecomp. No native-image paste, guest step, or replay PPU is used here.
class FieldFrameRenderer {
    static unsigned u16(const std::uint8_t* p) { return gba::text_u16(p); }
public:
    static bool supported(const gba::GbaRasterCapture& capture) {
        if(!capture.complete()) return false;
        for(unsigned y=0;y<160;++y) {
            const auto& row=*capture.line(y); const auto* io=row.io.data();
            // Same reviewed field register schedule as the source adapter.
            // Windows, fades, affine backgrounds and mosaic remain unsupported.
            if((row.dispcnt&0xEF87u)!=0x0F00u || u16(io+8)!=0x0500 ||
               u16(io+10)!=0x0605 || u16(io+12)!=0x0706 || u16(io+14)!=0x080B ||
               (u16(io+0x50)&0xC0u)) return false;
            if(row.dispcnt&0x1000) for(unsigned n=0;n<128;++n) {
                const auto a=u16(row.oam.data()+n*8);
                if((a&0x1000) && ((a&0x300)!=0x200) && (a>>14)!=3) return false;
            }
        }
        return true;
    }
    static bool margin_object(unsigned attr0,bool enabled) {
        // Retain the accepted extension policy: only regular field bodies and
        // shadows. Affine/semitransparent effects still render in the center.
        return enabled && !(attr0&0x1f00);
    }
    static bool allow_object(bool margin,bool authored_coverage) {
        // This is source opacity/coverage, NOT an RGB or black-pixel test.
        // Native composition is not subject to the margin void mask.
        return !margin || authored_coverage;
    }
    // sample(bg,x,y,row) supplies authenticated BG1..3 texels across the entire
    // view. It must return transparent outside each finite source's bounds.
    template<class Sample>
    static bool draw(const gba::GbaRasterCapture& capture,std::uint8_t* output,unsigned width,
                     bool extended_objects,Sample sample,FieldRenderStats& stats) {
        stats={};
        if(!output || width<240 || width>384 || !supported(capture)) return false;
        const int left=int(width-240)/2;
        for(unsigned y=0;y<160;++y) {
            const auto& row=*capture.line(y); const auto* io=row.io.data();
            std::array<gba::render::Candidate,384> objects{};
            if((row.dispcnt&0x1000) && (gba::g_ppu_debug_layer_mask&16)) for(unsigned n=0;n<128;++n) {
                const unsigned raw=u16(row.oam.data()+n*8+2)&511;
                const auto object=gba::render::object(row.oam.data(),n,field_object_x(raw,width));
                if(!object.valid || object.mode()==2 || int(y)<object.y || int(y)>=object.y+object.box_height) continue;
                const int begin=std::max(0,object.x+left),end=std::min(int(width),object.x+left+object.box_width);
                for(int sx=begin;sx<end;++sx) {
                    const int x=sx-left; const bool margin=x<0 || x>=240;
                    if(margin && !margin_object(object.attr0,extended_objects)) continue;
                    const auto texel=gba::render::object_texel(object,row.vram.data(),row.oam.data(),row.dispcnt,x,int(y));
                    if(!texel.opaque) continue;
                    if(margin) ++stats.extended_object_samples; else ++stats.center_object_samples;
                    if(object.key()<objects[sx].key)
                        objects[sx]={object.key(),4,texel.palette,object.mode()==1,true};
                }
            }
            for(unsigned sx=0;sx<width;++sx) {
                const int x=int(sx)-left; const bool margin=x<0 || x>=240;
                gba::render::Stack stack;
                bool coverage=false;
                for(unsigned bg=1;bg<=3;++bg) {
                    const auto texel=sample(bg,x,int(y),row);
                    coverage|=texel.opaque;
                    if(gba::g_ppu_debug_layer_mask&(1u<<bg))
                        stack.submit({int((u16(io+8+bg*2)&3)*256+128+bg),bg,texel.palette,false,texel.opaque});
                }
                // BG0 contains field UI. Compose it normally in the native
                // interval, without repeating UI into the extended scenery.
                if(!margin && (gba::g_ppu_debug_layer_mask&1)) {
                    const unsigned cnt=u16(io+8);
                    const auto texel=gba::render::text(row.vram.data(),cnt,x+int(u16(io+0x10)&511),int(y+(u16(io+0x12)&511)));
                    stack.submit({int((cnt&3)*256+128),0,texel.palette,false,texel.opaque});
                }
                if(allow_object(margin,coverage)) stack.submit(objects[sx]);
                else if(objects[sx].valid) ++stats.boundary_clips;
                const auto effect=gba::render::effect(stack,u16(io+0x50),63);
                auto color=gba::render::color_effect(u16(row.pal.data()+stack.first.palette*2),
                    u16(row.pal.data()+stack.second.palette*2),effect,u16(io+0x52),u16(io+0x54));
                // Finite source void stays black. Palette changes cannot turn
                // valid dark scenery into a void or hide its actors.
                if(margin && !coverage) color=0;
                gba::render::rgb(color,output+(y*width+sx)*3);
            }
            ++stats.rows; stats.center_columns+=240; stats.extended_columns+=(width-240);
        }
        return true;
    }
};
}
