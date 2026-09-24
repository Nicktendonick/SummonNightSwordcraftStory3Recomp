#pragma once
#include "gba_raster_capture.h"
#include "gba_render_primitives.h"
#include "gba_window_policy.h"
#include <array>

namespace swordcraft3 {
// One owner of every combat output column, including the native center and
// HUD. Reuses hardware decoding, not GbaPpu::render_scanline/draw_view. The
// scene adapter authorizes source continuations; it never supplies a second
// image to paste over this one.
struct CombatRenderStats {
    unsigned rows=0,center_columns=0,extended_columns=0,affine_rows=0;
};
class CombatFrameRenderer {
    static unsigned u16(const std::uint8_t* p) { return gba::text_u16(p); }
public:
    static bool supported(const gba::GbaRasterCapture& capture) {
        if(!capture.complete()) return false;
        for(unsigned y=0;y<160;++y) {
            const auto& line=*capture.line(y);
            if((line.dispcnt&7)>1) return false;
            // Mosaic sampling has not been migrated yet. Reject before writing
            // any row instead of silently rendering a half-supported frame.
            for(unsigned bg=0;bg<4;++bg)
                if((line.dispcnt&(0x100u<<bg)) && (u16(line.io.data()+8+bg*2)&0x40)) return false;
            for(unsigned n=0;n<128;++n) {
                const unsigned a=u16(line.oam.data()+n*8);
                if((line.dispcnt&0x1000) && (a&0x1000) && !((a&0x300)==0x200)) return false;
            }
        }
        return true;
    }
    static bool draw(const gba::GbaRasterCapture& capture,std::uint8_t* output,unsigned width,
                     const gba::GbaReplayViewPolicy& source,CombatRenderStats& stats) {
        stats={};
        if(!output || width<240 || width>384 || !supported(capture)) return false;
        const unsigned left=(width-240)/2;
        for(unsigned y=0;y<160;++y) {
            const auto& row=*capture.line(y);
            const auto* io=row.io.data(); const auto* vram=row.vram.data(); const auto* oam=row.oam.data();
            const unsigned display=row.dispcnt,mode=display&7;
            std::array<gba::render::Stack,384> stacks{};
            std::array<gba::render::Candidate,384> objects{};
            std::array<bool,384> obj_window{};
            std::array<unsigned,384> controls{};
            // Decode each submitted OBJ once per row, for the complete view.
            // OBJ-window stencils use the same geometry, not a 240px scratch.
            if(display&0x1000) for(unsigned n=0;n<128;++n) {
                const int raw=int(u16(oam+n*8+2)&511);
                int x=raw>=256?raw-512:raw;
                if(source.obj_x) source.obj_x(raw,&x);
                const auto object=gba::render::object(oam,n,x);
                if(!object.valid || int(y)<object.y || int(y)>=object.y+object.box_height) continue;
                const int begin=std::max(0,object.x+int(left)),end=std::min(int(width),object.x+int(left)+object.box_width);
                for(int sx=begin;sx<end;++sx) {
                    const auto texel=gba::render::object_texel(object,vram,oam,display,sx-int(left),int(y));
                    if(!texel.opaque) continue;
                    if(object.mode()==2) { if(display&0x8000) obj_window[sx]=true; continue; }
                    auto& dst=objects[sx];
                    if(object.key()<dst.key) dst={object.key(),4,texel.palette,object.mode()==1,true};
                }
            }
            struct Window { bool row=false,extended=false; int left=0,right=0; } windows[2];
            for(unsigned n=0;n<2;++n) {
                auto& w=windows[n];
                const unsigned bounds=u16(io+0x44+n*2),top=bounds>>8;
                unsigned bottom=bounds&255;
                if(bottom>160 || top>bottom) bottom=160;
                w.row=(display&(0x2000u<<n)) && y>=top && y<bottom;
                if(w.row && source.window_margin_bounds)
                    w.extended=source.window_margin_bounds(source.user,n,y,io,&w.left,&w.right);
            }
            const unsigned margin_mask=source.margin_layer_mask ? source.margin_layer_mask(source.user,y,display,io) : 31;
            for(unsigned sx=0;sx<width;++sx) {
                const int x=int(sx)-int(left);
                bool inside[2]{};
                for(unsigned n=0;n<2;++n) inside[n]=windows[n].row &&
                    gba::detail::replay_window_horizontal_contains(u16(io+0x40+n*2),x,240,
                        windows[n].extended,windows[n].left,windows[n].right);
                controls[sx]=gba::detail::replay_window_control((display&0xe000)!=0,inside[0],inside[1],
                    obj_window[sx],u16(io+0x48),u16(io+0x4a));
            }
            for(unsigned layer=0;layer<4;++layer) {
                if(!(display&(0x100u<<layer)) || !(gba::g_ppu_debug_layer_mask&(1u<<layer))) continue;
                if(mode==1 && layer==3) continue;
                const unsigned cnt=u16(io+8+layer*2),scroll=0x10+layer*4;
                const bool affine=mode==1 && layer==2;
                if(affine) ++stats.affine_rows;
                // The observer captures hidden affine state before reload is
                // consumed. Reconstruct this row's origin without advancing it.
                int ref_x=0,ref_y=0,pa=0,pc=0;
                if(affine) {
                    const auto& a=row.affine[layer-2]; const unsigned base=0x20+(layer-2)*16;
                    ref_x=(y==0 || !a.valid_x || a.reload_x)?gba::render::s28(io+base+8):a.x;
                    ref_y=(y==0 || !a.valid_y || a.reload_y)?gba::render::s28(io+base+12):a.y;
                    pa=gba::render::s16(io+base); pc=gba::render::s16(io+base+4);
                }
                for(unsigned sx=0;sx<width;++sx) {
                    const int x=int(sx)-int(left); const bool margin=x<0 || x>=240;
                    if(margin && !(margin_mask&(1u<<layer))) continue;
                    const bool bypass=margin && !affine && (source.regular_bg_window_bypass_mask()&(1u<<layer));
                    if(!(controls[sx]&(1u<<layer)) && !bypass) continue;
                    gba::render::Texel texel;
                    if(affine) texel=gba::render::affine(vram,cnt,(ref_x+x*pa)>>8,(ref_y+x*pc)>>8);
                    else {
                        int sample=x;
                        const gba::WsBgMarginContext context{layer,sx,y,left,width,io,vram};
                        if(margin && source.bg_margin && source.bg_margin(source.user,&context,&sample)<0) continue;
                        std::uint16_t entry=0; int replace=0;
                        if(margin && source.text_margin_entry) replace=source.text_margin_entry(source.user,&context,&entry);
                        if(replace<0) continue;
                        texel=gba::render::text(vram,cnt,sample+int(u16(io+scroll)&511),int(y+(u16(io+scroll+2)&511)),replace>0,entry);
                    }
                    stacks[sx].submit({int((cnt&3)*256+128+layer),layer,texel.palette,false,texel.opaque});
                }
            }
            for(unsigned sx=0;sx<width;++sx) {
                const int x=int(sx)-int(left);
                if((controls[sx]&16) && (gba::g_ppu_debug_layer_mask&16) &&
                    gba::detail::replay_margin_layer_allowed(margin_mask,4,x,240) &&
                    (!source.clip_objects || (x>=0 && x<240))) stacks[sx].submit(objects[sx]);
                const auto& stack=stacks[sx];
                const auto effect=gba::render::effect(stack,u16(io+0x50),controls[sx]);
                const auto color=gba::render::color_effect(u16(row.pal.data()+stack.first.palette*2),
                    u16(row.pal.data()+stack.second.palette*2),effect,u16(io+0x52),u16(io+0x54));
                gba::render::rgb((display&0x80)?0x7fff:color,output+(y*width+sx)*3);
            }
            ++stats.rows; stats.center_columns+=240; stats.extended_columns+=width-240;
        }
        return true;
    }
};
} // namespace swordcraft3
