#include "combat_frame_renderer.h"
#include "battle_follow_camera.h"
#include "battle_scenery_edges.h"
#include <stdexcept>
#include <vector>
#include <cstdio>
static unsigned checks=0,calls=0;
static bool bounded=false;
static unsigned gameplay_anchor=8;
static void check(bool value) { ++checks; if(!value) throw std::runtime_error("combat compositor state assertion"); }
static int source(const void*,const gba::WsBgMarginContext* c,int*) {
    const auto left=bounded && c->screen_y>=19 && c->screen_y<125?gameplay_anchor:72u;
    check(c->output_width==384 && c->native_left==left && c->layer==0);
    check(c->output_x<left || c->output_x>=left+240);
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
    check(stats.edge_columns==0 && stats.edge_rows==0);
    check(calls==23040);
    bounded=true; calls=0;
    const auto framing=swordcraft3::battle_framing(384,0,384,true);
    check(swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),384,policy,stats,&framing,19));
    check(calls==23040 && stats.rows==160); // Callback coordinate contract, not pixels.
    check(stats.edge_columns==16*106 && stats.edge_rows==106);
    for(int flat:{32,72,120}) {
        const auto envelope=swordcraft3::battle_follow_envelope(flat);
        const auto follow=swordcraft3::battle_follow_framing(384,envelope.minimum,envelope.maximum,flat);
        check(follow.active);
        gameplay_anchor=follow.framing.anchor; calls=0;
        check(swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),384,policy,stats,&follow.framing,19));
        check(calls==23040 && stats.edge_columns==38*106 && stats.edge_rows==106);
        const auto cover=swordcraft3::battle_scenery_cover(follow,384,3,true);
        calls=0;
        check(swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),384,policy,stats,&cover,19));
        check(calls==23040 && stats.edge_rows==106);
        check(stats.edge_columns==(cover.begin+384-cover.end)*106);
        for(unsigned x=cover.anchor;x<cover.anchor+240;++x)
            check(swordcraft3::battle_edge_column(384,cover,x).band==swordcraft3::BattleEdgeBand::None);
    }
    const auto follow_fallback=swordcraft3::battle_follow_framing(384,-1,32,-1);
    gameplay_anchor=72; calls=0;
    check(swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),384,policy,stats,&follow_fallback.framing,19));
    check(calls==23040 && stats.edge_columns==0);
    gameplay_anchor=8;
    // No decoration in HUD rows, native width or views without side padding.
    bounded=false;
    check(swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),384,policy,stats,&framing,160));
    check(stats.edge_columns==0 && stats.edge_rows==0);
    bounded=true;
    const auto native=swordcraft3::battle_framing(240,0,384,true);
    check(swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),240,{},stats,&native,19));
    check(stats.edge_columns==0 && stats.edge_rows==0);
    const auto narrow=swordcraft3::battle_framing(284,0,384,true);
    check(swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),284,{},stats,&narrow,19));
    check(stats.edge_columns==0 && stats.edge_rows==0);
    io[8]=0;
    for(unsigned y=0;y<160;++y) capture->capture({ppu.get(),y,0x180,io.data(),vram.data(),oam.data(),pal.data()});
    check(swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),384,policy,stats,&framing,19));
    check(stats.edge_columns==0 && stats.edge_rows==0); // Forced-blank dispatch, not color matching.
    io[8]=0x40;
    for(unsigned y=0;y<160;++y) capture->capture({ppu.get(),y,0x100,io.data(),vram.data(),oam.data(),pal.data()});
    check(!swordcraft3::CombatFrameRenderer::draw(*capture,output.data(),384,policy,stats));
    check(stats.rows==0); // Unsupported mosaic rejects the entire frame first.
    capture->reset();
    check(!swordcraft3::CombatFrameRenderer::supported(*capture));
    std::printf("%u composition/source assertions passed; no framebuffer inspected\n",checks);
}
