#include "field_frame_renderer.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>
static unsigned checks=0;
static void check(bool b) { ++checks; if(!b) throw std::runtime_error("field compositor state/source assertion"); }
static void put(std::uint8_t* p,unsigned v) { p[0]=v; p[1]=v>>8; }
int main() {
    auto capture=std::make_unique<gba::GbaRasterCapture>();
    auto ppu=std::make_unique<gba::GbaPpu>();
    std::array<std::uint8_t,0x400> io{},oam{},pal{};
    std::array<std::uint8_t,0x18000> vram{};
    std::vector<std::uint8_t> output(384*160*3);
    for(unsigned n=0;n<128;++n) put(oam.data()+8*n,0x200);
    put(io.data()+8,0x0500); put(io.data()+10,0x0605);
    put(io.data()+12,0x0706); put(io.data()+14,0x080b);
    std::fill(vram.begin()+0x10000,vram.begin()+0x10020,0x11);
    // One native body, one negative-X body, one positive-X body.
    for(unsigned n=0;n<3;++n) {
        put(oam.data()+n*8,0); put(oam.data()+n*8+2,n==0?100:n==1?480:270);
    }
    auto record=[&](std::uint16_t display=0x1f40) {
        capture->reset();
        for(unsigned y=0;y<160;++y) check(capture->capture({ppu.get(),y,display,io.data(),vram.data(),oam.data(),pal.data()}));
    };
    swordcraft3::FieldRenderStats stats;
    unsigned center_samples=0,margin_samples=0;
    auto source=[&](unsigned bg,int x,int y,const gba::GbaRasterCapture::Line&) {
        check(bg>=1 && bg<=3 && x>=-72 && x<312 && y>=0 && y<160);
        if(x<0 || x>=240) ++margin_samples; else ++center_samples;
        return gba::render::Texel{1,0,bg==3};
    };
    check(!swordcraft3::FieldFrameRenderer::draw(*capture,output.data(),384,true,source,stats));
    record();
    check(swordcraft3::FieldFrameRenderer::draw(*capture,output.data(),384,true,source,stats));
    check(stats.rows==160 && stats.center_columns==38400 && stats.extended_columns==23040);
    check(center_samples==38400*3 && margin_samples==23040*3);
    check(stats.center_object_samples==64 && stats.extended_object_samples==128);
    check(stats.boundary_clips==0); // Palette is black, but authored coverage is real.
    check(swordcraft3::FieldFrameRenderer::draw(*capture,output.data(),384,false,source,stats));
    check(stats.center_object_samples==64 && stats.extended_object_samples==0);
    auto void_source=[](unsigned,int,int,const gba::GbaRasterCapture::Line&) { return gba::render::Texel{}; };
    check(swordcraft3::FieldFrameRenderer::draw(*capture,output.data(),384,true,void_source,stats));
    check(stats.boundary_clips==128 && stats.center_object_samples==64);
    // Unknown field effects retain native drawing, never gain margin permission.
    check(!swordcraft3::FieldFrameRenderer::margin_object(0x100,true));
    check(!swordcraft3::FieldFrameRenderer::margin_object(0x400,true));
    check(!swordcraft3::FieldFrameRenderer::margin_object(0x1000,true));
    check(swordcraft3::FieldFrameRenderer::margin_object(0x2000,true));
    check(swordcraft3::FieldFrameRenderer::allow_object(false,false));
    check(!swordcraft3::FieldFrameRenderer::allow_object(true,false));
    check(swordcraft3::FieldFrameRenderer::allow_object(true,true));
    // Mutation of live IO after snapshot must not alter eligibility.
    put(io.data()+0x50,0x80);
    check(swordcraft3::FieldFrameRenderer::supported(*capture));
    record();
    check(!swordcraft3::FieldFrameRenderer::draw(*capture,output.data(),384,true,source,stats));
    check(stats.rows==0);
    put(io.data()+0x50,0); record(0x3f40);
    check(!swordcraft3::FieldFrameRenderer::supported(*capture));
    put(oam.data(),0x1000); record();
    check(!swordcraft3::FieldFrameRenderer::supported(*capture));
    put(oam.data(),0); record();
    center_samples=margin_samples=0;
    check(swordcraft3::FieldFrameRenderer::draw(*capture,output.data(),240,true,source,stats));
    check(stats.extended_columns==0 && margin_samples==0 && center_samples==38400*3);
    capture->reset();
    check(!swordcraft3::FieldFrameRenderer::supported(*capture));
    std::printf("%u field composition/source assertions passed; no framebuffer inspected\n",checks);
}
