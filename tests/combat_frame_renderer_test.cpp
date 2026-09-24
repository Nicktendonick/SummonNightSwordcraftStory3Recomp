#include "combat_frame_renderer.h"
#include <stdexcept>
#include <vector>
#include <cstdio>
static unsigned checks=0,calls=0;
static void check(bool value) { ++checks; if(!value) throw std::runtime_error("combat compositor state assertion"); }
static int source(const void*,const gba::WsBgMarginContext* c,int*) {
    check(c->output_width==384 && c->native_left==72 && c->layer==0);
    check(c->output_x<72 || c->output_x>=312);
    ++calls; return -1;
}
int main() {
    auto capture=std::make_unique<gba::GbaRasterCapture>();
    auto ppu=std::make_unique<gba::GbaPpu>();
    std::array<std::uint8_t,0x400> io{},oam{},pal{};
    std::array<std::uint8_t,0x18000> vram{};
    std::vector<std::uint8_t> output(384*160*3);
    swordcraft3::CombatRenderStats stats;
    gba::GbaReplayViewPolicy policy; policy.bg_margin=source;
    check(!swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),384,policy,stats));
    check(stats.rows==0);
    for(unsigned y=0;y<160;++y) check(capture->capture({ppu.get(),y,0x100,io.data(),vram.data(),oam.data(),pal.data()}));
    // Mutation after capture must not change input ownership or source calls.
    io[8]=0x40;
    check(swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),384,policy,stats));
    check(stats.rows==160 && stats.center_columns==38400 && stats.extended_columns==23040);
    check(calls==23040);
    for(unsigned y=0;y<160;++y) capture->capture({ppu.get(),y,0x100,io.data(),vram.data(),oam.data(),pal.data()});
    check(!swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),384,policy,stats));
    check(stats.rows==0); // Unsupported mosaic rejects the entire frame first.
    capture->reset();
    check(!swordcraft3::CombatFrameRenderer::supported(*capture));
    std::printf("%u composition/source assertions passed; no framebuffer inspected\n",checks);
}
