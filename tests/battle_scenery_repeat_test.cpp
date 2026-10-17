#include "battle_scenery_repeat.h"
#include "battle_scenery_special.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(x) do { if(!(x)) throw std::runtime_error("Failed: " #x " (line "+std::to_string(__LINE__)+")"); } while(0)

// Synthetic tile-map ENTRIES (and, for edge scoring, synthetic 4bpp tiles and
// palette). No ROM data, captured graphics or framebuffer pixels.
namespace {
using swordcraft3::SceneryRepeatPlan;
constexpr std::uint16_t H=0x400;
std::uint16_t canon(std::uint16_t e) { return (e&0x3ff) ? e : 0; }
struct Map {
    unsigned columns,rows;
    std::vector<std::uint16_t> e;
    std::uint16_t at(unsigned y,unsigned x) const { return canon(e[y*columns+x]); }
};
// The tile shown at absolute tile position t, derived ONLY from the per-pixel
// mapping. Every pixel of a tile position must come from one source tile.
std::uint16_t shown_tile(const Map& m,const SceneryRepeatPlan& p,unsigned y,int t,
                         bool left_inset=true,bool right_inset=true) {
    const int width=int(m.columns)*8,x0=8*t;
    const int a=swordcraft3::scenery_repeat_x(p,y,x0,left_inset,right_inset);
    const int b=swordcraft3::scenery_repeat_x(p,y,x0+7,left_inset,right_inset);
    CHECK(a>=0 && a<width && b>=0 && b<width && a/8==b/8 && (b-a==7 || a-b==7));
    return canon(m.at(y,unsigned(a/8))^(b<a?H:0));
}
// The tile shown at extension step k (1-based, outward from the map edge).
std::uint16_t extension_tile(const Map& m,const SceneryRepeatPlan& p,unsigned y,unsigned k,bool right) {
    return shown_tile(m,p,y,right ? int(m.columns+k-1) : -int(k));
}
bool authored_across(const Map& m,std::uint16_t a,std::uint16_t b) {
    for(unsigned y=0;y<m.rows;++y) for(unsigned x=0;x+1<m.columns;++x) {
        const auto l=m.at(y,x),r=m.at(y,x+1);
        if((l==a && r==b) || (canon(r^H)==a && canon(l^H)==b)) return true;
    }
    return false;
}
bool authored_down(const Map& m,std::uint16_t a,std::uint16_t b) {
    for(unsigned y=0;y+1<m.rows;++y) for(unsigned x=0;x<m.columns;++x) {
        const auto t=m.at(y,x),u=m.at(y+1,x);
        if((t==a && u==b) || (canon(t^H)==a && canon(u^H)==b)) return true;
    }
    return false;
}
struct Audit { unsigned across=0,reflections=0,down=0; };
// Independent brute-force audit of every horizontal and vertical join shown
// from `reach` tiles left of the map to `reach` tiles right of it, including
// replaced edge columns. Joins between kept columns are authored by definition.
Audit audit(const Map& m,const SceneryRepeatPlan& p,bool left_inset=true,bool right_inset=true) {
    Audit out;
    const int first=-int(p.reach),last=int(m.columns+p.reach);
    for(unsigned y=0;y<m.rows;++y) {
        for(int t=first;t<last;++t) {
            const auto here=shown_tile(m,p,y,t,left_inset,right_inset);
            if(t+1<last) {
                const auto next=shown_tile(m,p,y,t+1,left_inset,right_inset);
                if(!authored_across(m,here,next)) {
                    if(next==canon(here^H)) ++out.reflections; else ++out.across;
                }
            }
            if(y+1<m.rows && !authored_down(m,here,shown_tile(m,p,y+1,t,left_inset,right_inset))) ++out.down;
        }
    }
    return out;
}
// Continuity of the per-pixel mapping: identity inside each row's kept strip;
// outside it steps of +1, a reflection turn (0 or -1 for mirrored runs) or a
// wrap back to the start of the period.
void check_mapping(const Map& m,const SceneryRepeatPlan& p,bool left_inset=true,bool right_inset=true) {
    const int width=int(m.columns)*8;
    for(unsigned y=0;y<m.rows;++y) {
        const auto& left=(left_inset?p.left:p.left_edge).rows[y];
        const auto& right=(right_inset?p.right:p.right_edge).rows[y];
        const int lo=int(left.inset)*8,hi=width-int(right.inset)*8;
        for(int x=-3*width;x<4*width;++x) {
            const int s=swordcraft3::scenery_repeat_x(p,y,x,left_inset,right_inset);
            CHECK(s>=0 && s<width);
            if(x>=lo && x<hi) { CHECK(s==x); continue; }
            const int t=swordcraft3::scenery_repeat_x(p,y,x+1,left_inset,right_inset);
            if(x+1==lo || x+1==hi) continue; // the kept edge itself
            const auto& rule=x>=hi?right:left;
            if(rule.sequence) continue; // whole cells, checked by shown_tile
            const int span=int(rule.span)*8;
            if(rule.mirror) CHECK(t-s==1 || t-s==-1 || t==s);
            else CHECK(t-s==1 || t-s==1-span);
        }
    }
}
// One map: entries listed row by row.
Map map(unsigned columns,std::vector<std::vector<std::uint16_t>> rows) {
    Map m{columns,unsigned(rows.size()),{}};
    for(auto& row:rows) { CHECK(row.size()==columns); m.e.insert(m.e.end(),row.begin(),row.end()); }
    return m;
}
SceneryRepeatPlan plan(const Map& m,unsigned reach=10,swordcraft3::SceneryRepeatInsets insets={},
                       unsigned floor_row=0,const swordcraft3::SceneryTileArt& art={},
                       bool repeat_floor=false) {
    return swordcraft3::plan_scenery_repeat(m.e.data(),m.columns,m.rows,reach,insets,floor_row,art,repeat_floor);
}
bool same_side(const swordcraft3::SceneryRepeatSide& a,const swordcraft3::SceneryRepeatSide& b,unsigned rows) {
    if(a.cost!=b.cost || a.unauthored!=b.unauthored || a.inset!=b.inset) return false;
    for(unsigned y=0;y<rows;++y)
        if(a.rows[y].mirror!=b.rows[y].mirror || a.rows[y].span!=b.rows[y].span ||
           a.rows[y].inset!=b.rows[y].inset) return false;
    return true;
}
} // namespace

int main() {
    using namespace swordcraft3;
    unsigned cases=0;
    // Invalid inputs never produce a plan; mapping is then the identity.
    {
        const std::uint16_t one[64*33]{};
        CHECK(!plan_scenery_repeat(nullptr,48,20).valid);
        CHECK(!plan_scenery_repeat(one,1,20).valid);
        CHECK(!plan_scenery_repeat(one,65,20).valid);
        CHECK(!plan_scenery_repeat(one,48,0).valid);
        CHECK(!plan_scenery_repeat(one,48,33).valid);
        CHECK(!plan_scenery_repeat(one,48,20,0).valid);
        CHECK(!plan_scenery_repeat(one,48,20,65).valid);
        const SceneryRepeatPlan none{};
        for(int x:{-500,-1,0,383,384,900}) CHECK(scenery_repeat_x(none,0,x)==x);
        const auto blank=plan_scenery_repeat(one,48,20);
        CHECK(blank.valid && scenery_repeat_x(blank,20,-5)==-5); // row outside the plan
        // Insets must leave at least two kept columns and stay small.
        CHECK(!plan_scenery_repeat(one,10,2,10,{2,5,4}).valid);
        CHECK(!plan_scenery_repeat(one,48,20,10,{20,SceneryRepeatPlan::max_inset+1,0}).valid);
        CHECK(plan_scenery_repeat(one,10,2,10,{2,4,4}).valid);
        const auto clamped=plan_scenery_repeat(one,48,20,10,{99,4,4});
        CHECK(clamped.valid && clamped.inset_rows==20);
        const auto unused=plan_scenery_repeat(one,48,20,10,{0,4,4});
        CHECK(unused.valid && unused.inset_rows==0 && !unused.left.inset && !unused.right.inset);
        ++cases;
    }
    // A genuinely periodic strip continues through its own period both ways.
    {
        auto m=map(12,{{0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14},
                        {0x21,0x22,0x23,0x24,0x21,0x22,0x23,0x24,0x21,0x22,0x23,0x24}});
        const auto p=plan(m);
        for(unsigned y=0;y<2;++y) for(auto* side:{&p.left,&p.right})
            CHECK(!side->rows[y].mirror && side->rows[y].span%4==0);
        CHECK(p.left.cost==0 && p.right.cost==0 && p.left.unauthored==0 && p.right.unauthored==0);
        for(int x=-200;x<300;++x) {
            const int s=scenery_repeat_x(p,0,x);
            CHECK(((s-x)%32+32)%32==0); // same phase of the 32-pixel period
        }
        const auto a=audit(m,p);
        CHECK(a.across==0 && a.reflections==0 && a.down==0);
        check_mapping(m,p); ++cases;
    }
    // Open space beside a building, over a plain floor tile: equal scores keep
    // the longest wrap, so the arena loops and the building repeats instead of
    // the margin staying empty.
    {
        auto m=map(10,{{0,0,0x31,0x32,0x33,0x433,0x432,0x431,0,0},
                        {0,0,0x41,0x42,0x43,0x443,0x442,0x441,0,0},
                        {0x50,0x50,0x51,0x52,0x53,0x453,0x452,0x451,0x50,0x50}});
        const auto p=plan(m);
        for(unsigned y=0;y<3;++y) for(auto* side:{&p.left,&p.right})
            CHECK(!side->rows[y].mirror && side->rows[y].span==10);
        CHECK(p.left.cost==0 && p.right.cost==0);
        CHECK(extension_tile(m,p,2,7,true)==m.at(2,6) && extension_tile(m,p,0,3,false)==m.at(0,7));
        const auto a=audit(m,p);
        CHECK(a.across==0 && a.reflections==0 && a.down==0);
        check_mapping(m,p); ++cases;
    }
    // Forest-like: a periodic tree band over a floor drawn as mirrored halves.
    // Trees keep their period; the floor reflects at the edge exactly as its
    // own centre does; no vertical join is invented between the two.
    {
        auto m=map(12,{{0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14},
                        {0x21,0x22,0x23,0x24,0x21,0x22,0x23,0x24,0x21,0x22,0x23,0x24},
                        {0x61,0x62,0x63,0x64,0x65,0x66,0x466,0x465,0x464,0x463,0x462,0x461},
                        {0x71,0x72,0x73,0x74,0x75,0x76,0x476,0x475,0x474,0x473,0x472,0x471}});
        const auto p=plan(m);
        for(auto* side:{&p.left,&p.right}) {
            for(unsigned y=0;y<2;++y) CHECK(!side->rows[y].mirror && side->rows[y].span%4==0);
            CHECK(side->unauthored==0 && side->cost==2); // one reflection per floor row
        }
        for(unsigned y=2;y<4;++y) {
            // The first extension tile is the mirror image of the edge tile.
            CHECK(extension_tile(m,p,y,1,true)==canon(m.at(y,11)^H));
            CHECK(extension_tile(m,p,y,1,false)==canon(m.at(y,0)^H));
        }
        const auto a=audit(m,p);
        CHECK(a.across==0 && a.down==0 && a.reflections==4);
        check_mapping(m,p); ++cases;
    }
    // Rocky-like: rocks stand on part of a mirrored floor, with open columns
    // at both edges. A floor tile that only ever appears under rock must never
    // be shown under open sky, so the arena repeats as a whole.
    {
        auto m=map(10,{{0,0,0x81,0x82,0x83,0x84,0x85,0x86,0,0},
                        {0,0,0x91,0x92,0x93,0x94,0x95,0x96,0,0},
                        {0xa1,0xa2,0xa3,0xa4,0xa5,0x4a5,0x4a4,0x4a3,0x4a2,0x4a1}});
        const auto p=plan(m);
        const auto a=audit(m,p);
        CHECK(a.across==0 && a.down==0 && a.reflections==2);
        CHECK(p.left.unauthored==0 && p.right.unauthored==0 && p.left.cost==1 && p.right.cost==1);
        check_mapping(m,p); ++cases;
    }
    // A row with no authored continuation and no mirrored art keeps one
    // seam, chosen at a near-period (most matching neighbours) and counted.
    {
        auto m=map(12,{{0x11,0x12,0x13,0x14,0x15,0x16,0x11,0x12,0x13,0x14,0x15,0x17}});
        const auto p=plan(m,6);
        CHECK(!p.right.rows[0].mirror && p.right.rows[0].span==6);
        CHECK(p.right.unauthored==1 && p.right.cost==20-8); // four matching neighbours
        check_mapping(m,p); ++cases;
    }
    // Ping-pong reflection and deterministic re-planning.
    {
        SceneryRepeatPlan p; p.columns=6; p.rows=1; p.reach=10; p.valid=true;
        p.right.rows[0]={1,2}; p.left.rows[0]={1,2};
        for(int d=0;d<16;++d) CHECK(scenery_repeat_x(p,0,48+d)==47-d);
        CHECK(scenery_repeat_x(p,0,64)==32 && scenery_repeat_x(p,0,79)==47 && scenery_repeat_x(p,0,80)==47);
        CHECK(scenery_repeat_x(p,0,-1)==0 && scenery_repeat_x(p,0,-16)==15 &&
              scenery_repeat_x(p,0,-17)==15 && scenery_repeat_x(p,0,-32)==0);
        auto m=map(12,{{0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14}});
        const auto a=plan(m),b=plan(m);
        CHECK(a.right.cost==b.right.cost && a.right.rows[0].span==b.right.rows[0].span);
        const auto h=scenery_repeat_hash(m.e.data(),12);
        CHECK(h==scenery_repeat_hash(m.e.data(),12));
        m.e[5]^=1; CHECK(h!=scenery_repeat_hash(m.e.data(),12));
        ++cases;
    }
    // Overscan edge columns: each edge tile (0x10, 0x19) occurs nowhere else,
    // so no continuation can join it. One column inside, the strip repeats with
    // a 4-tile period, so replacing that column joins only authored pairs.
    {
        auto m=map(10,{{0x10,0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14,0x19},
                        {0x20,0x21,0x22,0x23,0x24,0x21,0x22,0x23,0x24,0x29},
                        {0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50}});
        // Rows 0-1 are overscan; floor row 2 is always shown natively.
        const auto edge=plan(m),p=plan(m,10,{2,1,1});
        CHECK(p.inset_rows==2 && p.left.inset==1 && p.right.inset==1);
        for(auto* side:{&p.left,&p.right}) {
            for(unsigned y=0;y<2;++y) CHECK(side->rows[y].inset==1 && !side->rows[y].mirror && side->rows[y].span%4==0);
            CHECK(side->rows[2].inset==0 && !side->rows[2].mirror);
            CHECK(side->cost==0 && side->unauthored==0);
        }
        // The inset-free result is kept for fallback and equals a plain plan.
        CHECK(same_side(p.left_edge,edge.left,m.rows) && same_side(p.right_edge,edge.right,m.rows));
        CHECK(edge.left.unauthored==2 && edge.right.unauthored==2);
        // Replaced column 0 now shows column 4's tile; column 9 shows column 5's.
        for(unsigned y=0;y<2;++y) CHECK(shown_tile(m,p,y,0)==m.at(y,4) && shown_tile(m,p,y,9)==m.at(y,5));
        CHECK(scenery_repeat_x(p,2,0)==0 && scenery_repeat_x(p,2,79)==79); // native row kept
        // A side whose replaced column is natively visible falls back alone.
        for(int t=0;t<10;++t) CHECK(shown_tile(m,p,0,t,false,true)==m.at(0,t==9?5:unsigned(t)));
        const auto a=audit(m,p),b=audit(m,edge);
        CHECK(a.across==0 && a.reflections==0 && a.down==0);
        CHECK(b.across==4 && b.down==0); // the edge tiles of both overscan rows
        CHECK(audit(m,p,false,false).across==b.across);
        check_mapping(m,p); check_mapping(m,p,false,true); check_mapping(m,p,true,false);
        ++cases;
    }
    // Equal scores keep the authored edge: a periodic strip never takes an
    // inset, so its continuation is identical with or without permission.
    {
        auto m=map(12,{{0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14},
                        {0x21,0x22,0x23,0x24,0x21,0x22,0x23,0x24,0x21,0x22,0x23,0x24}});
        const auto p=plan(m,10,{2,3,3}),q=plan(m);
        CHECK(!p.left.inset && !p.right.inset);
        CHECK(same_side(p.left,q.left,2) && same_side(p.right,q.right,2));
        for(int x=-100;x<200;++x) CHECK(scenery_repeat_x(p,1,x)==scenery_repeat_x(q,1,x));
        ++cases;
    }
    // Padding: blank filler columns frame the art in the overscan rows
    // (arenas 2/3/7). They are always replaced and never sourced, and joins
    // with them are not authored. Framed art loops: its right edge meets its
    // left edge (both drawn to be cut by the screen edge) instead of leaving
    // an empty band between copies of the art.
    {
        constexpr std::uint16_t F=0x40;
        auto m=map(12,{{F,F,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,F,F},
                        {F,F,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,F,F},
                        {0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0x4a6,0x4a5,0x4a4,0x4a3,0x4a2,0x4a1}});
        const auto edge=plan(m),p=plan(m,10,{2,3,2},2);
        CHECK(edge.left.padding==0 && edge.right.padding==0); // no insets: no padding
        CHECK(p.left.padding==2 && p.right.padding==2 && p.left.inset==2 && p.right.inset==2);
        CHECK(same_side(p.left,p.left_edge,m.rows) && same_side(p.right,p.right_edge,m.rows));
        for(unsigned y=0;y<2;++y) {
            CHECK(!p.left.rows[y].mirror && !p.right.rows[y].mirror && p.right.rows[y].span==8);
            CHECK(shown_tile(m,p,y,10)==m.at(y,2) && shown_tile(m,p,y,1)==m.at(y,9));
            for(int t=-30;t<40;++t) CHECK(shown_tile(m,p,y,t)!=F);
        }
        CHECK(p.left.cost<20 && p.right.cost<20); // two loop seams and a floor reflection
        // The plain plan continues through the filler into the next copy.
        CHECK(shown_tile(m,edge,0,12)==F || shown_tile(m,edge,0,13)==F);
        // Filler shown natively is the game's own: padding needs no fallback.
        SceneryInsetView v;
        scenery_repeat_inset_view(p,0,0,64,v); scenery_repeat_inset_view(p,1,40,56,v);
        CHECK(v.left && v.right);
        check_mapping(m,p); check_mapping(m,p,false,false); ++cases;
    }
    // Padding around art drawn as mirrored halves (arena 2): the art strip
    // wraps whole, so each edge meets the mirror image it was drawn from.
    // A wrap longer than the art strip would source the opposite filler.
    {
        constexpr std::uint16_t F=0x40;
        auto m=map(10,{{F,F,0x31,0x32,0x33,0x433,0x432,0x431,F,F},
                        {F,F,0x41,0x42,0x43,0x443,0x442,0x441,F,F},
                        {0x51,0x52,0x53,0x54,0x55,0x455,0x454,0x453,0x452,0x451}});
        const auto p=plan(m,10,{2,2,2},2);
        for(unsigned y=0;y<2;++y) {
            for(auto* side:{&p.left,&p.right}) CHECK(side->rows[y].inset==2 && side->rows[y].span<=6);
            CHECK(shown_tile(m,p,y,8)==m.at(y,2) && shown_tile(m,p,y,1)==m.at(y,7));
            for(int t=-30;t<40;++t) CHECK(shown_tile(m,p,y,t)!=F);
        }
        CHECK(p.left.unauthored==0 && p.right.unauthored==0);
        check_mapping(m,p); ++cases;
    }
    // The scenery band repeats whole columns: rows whose own best periods
    // differ (4 and 6) share one wrap, so every extension column is a copy of
    // one authored column and nothing is sheared into stray fragments.
    {
        auto m=map(12,{{0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14},
                        {0x21,0x22,0x23,0x24,0x25,0x26,0x21,0x22,0x23,0x24,0x25,0x26}});
        const auto p=plan(m,10,{},2);
        for(auto* side:{&p.left,&p.right}) {
            CHECK(!side->rows[0].mirror && side->rows[0].span==side->rows[1].span);
            CHECK(side->rows[0].span%12==0 && side->cost==0);
        }
        for(int x=-300;x<400;++x) CHECK(scenery_repeat_x(p,0,x)==scenery_repeat_x(p,1,x));
        check_mapping(m,p); ++cases;
    }
    // The band never mirrors, even art drawn as mirrored halves; a wrap whose
    // seam happens to be a reflection pair is still a repeat.
    {
        auto m=map(12,{{0x61,0x62,0x63,0x64,0x65,0x66,0x466,0x465,0x464,0x463,0x462,0x461},
                        {0x71,0x72,0x73,0x74,0x75,0x76,0x476,0x475,0x474,0x473,0x472,0x471}});
        const auto band=plan(m,10,{},2),legacy=plan(m);
        for(unsigned y=0;y<2;++y) CHECK(!band.left.rows[y].mirror && !band.right.rows[y].mirror);
        CHECK(band.right.cost==2 && legacy.right.cost==2); // one free reflection seam per row
        ++cases;
    }
    // Tile-edge scoring: with the tile graphics, a non-authored join whose
    // touching pixels match costs almost nothing, so the period that puts a
    // matching colour beside each edge wins. Entries alone cannot tell.
    {
        // Solid 8x8 tiles: 1 red, 2 green, 3 blue, 4 green, 5 white, 6 red.
        constexpr std::uint8_t colour[7]{0,1,2,3,2,4,1};
        std::vector<std::uint8_t> chars(7*32),palette(0x200);
        for(unsigned t=1;t<7;++t)
            for(unsigned b=0;b<32;++b) chars[t*32+b]=std::uint8_t(colour[t]|(colour[t]<<4));
        const std::uint16_t rgb[5]{0,0x001f,0x03e0,0x7c00,0x7fff};
        for(unsigned i=1;i<5;++i) { palette[i*2]=std::uint8_t(rgb[i]); palette[i*2+1]=std::uint8_t(rgb[i]>>8); }
        SceneryTileArt art; art.chars=chars.data(); art.size=chars.size(); art.palette=palette.data();
        auto m=map(6,{{1,4,3,6,5,2}});
        const auto scored=plan(m,10,{},1,art),plain=plan(m,10,{},1);
        CHECK(scored.right.rows[0].span==5); // green edge meets green column 1
        CHECK(scored.left.rows[0].span==4);  // red edge follows red column 3
        CHECK(plain.right.rows[0].span!=5 || plain.left.rows[0].span!=4);
        CHECK(shown_tile(m,scored,0,6)==m.at(0,1) && shown_tile(m,scored,0,-1)==m.at(0,3));
        // Seam of identical lines is zero; a cut against transparency is not.
        using scenery_repeat_detail::Line; using scenery_repeat_detail::clear;
        std::array<Line,2> green{},hole{};
        for(auto& l:green) l.fill(0x03e0);
        for(auto& l:hole) l.fill(clear);
        CHECK(scenery_repeat_detail::seam(green,green)==0 && scenery_repeat_detail::seam(green,hole)>0);
        ++cases;
    }
    // Overscan (a backdrop's columns beyond its scroll range): replaced and
    // never sourced however they are drawn, even when their joins look
    // authored. Arena 16's backdrop never scrolls past its first 240 columns.
    {
        auto m=map(12,{{0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14,0x91,0x92,0x93,0x94},
                        {0x21,0x22,0x23,0x24,0x21,0x22,0x23,0x24,0x95,0x96,0x97,0x98}});
        const auto p=plan(m,10,{2,0,4,true},2);
        CHECK(p.valid && p.right.padding==4 && p.right.inset==4 && p.left.padding==0);
        for(unsigned y=0;y<2;++y) {
            CHECK(p.right.rows[y].span%4==0 && p.right.rows[y].span<=8);
            for(int t=8;t<40;++t) CHECK(shown_tile(m,p,y,t)==m.at(y,unsigned(t%4)));
            for(int t=-30;t<0;++t) CHECK(shown_tile(m,p,y,t)<0x90); // overscan never sourced
        }
        CHECK(p.right.cost==0);
        CHECK(!plan_scenery_repeat(m.e.data(),12,2,10,{2,0,33,true}).valid); // above max_inset
        ++cases;
    }
    // The flat band's last row may already be floor art across the full
    // width (arena 5): padding is found above it and that row plans as floor.
    {
        constexpr std::uint16_t F=0x40;
        auto m=map(10,{{F,F,0x81,0x82,0x83,0x84,0x85,0x86,F,F},
                        {F,F,0x91,0x92,0x93,0x94,0x95,0x96,F,F},
                        {0xa1,0xa2,0xa3,0xa4,0xa5,0x4a5,0x4a4,0x4a3,0x4a2,0x4a1},
                        {0xb1,0xb2,0xb3,0xb4,0xb5,0x4b5,0x4b4,0x4b3,0x4b2,0x4b1}});
        const auto p=plan(m,10,{3,2,2},3);
        CHECK(p.inset_rows==2 && p.left.padding==2 && p.right.padding==2);
        CHECK(p.left.rows[2].inset==0 && p.right.rows[2].inset==0);
        for(unsigned y=0;y<2;++y) for(int t=-30;t<40;++t) CHECK(shown_tile(m,p,y,t)!=F);
        check_mapping(m,p); ++cases;
    }
    // A replaced art column beyond the padding still needs the guard.
    {
        SceneryRepeatPlan p; p.columns=48; p.rows=1; p.inset_rows=1; p.valid=true;
        p.left.rows[0]={1,42,3}; p.left.padding=3;
        p.right.rows[0]={1,40,5}; p.right.padding=3;
        auto view=[&](int h) { SceneryInsetView v; scenery_repeat_inset_view(p,0,h,240,v); return v; };
        CHECK(view(0).left && view(-8).left); // padding only on the left
        CHECK(view(104).right && view(105).right==false); // columns 43-44 are art
        ++cases;
    }
    // Native-view guard: a view [h, h+view) must not reach a replaced column.
    {
        SceneryRepeatPlan p; p.columns=48; p.rows=3; p.inset_rows=2; p.valid=true;
        for(unsigned y=0;y<2;++y) { p.left.rows[y]={0,28,4}; p.right.rows[y]={0,28,3}; }
        p.left.rows[2]={1,48,0}; p.right.rows[2]={1,48,0};
        auto view=[&](unsigned row,int h) { SceneryInsetView v; scenery_repeat_inset_view(p,row,h,240,v); return v; };
        CHECK(view(0,32).left && view(0,31).left==false && view(0,-8).left==false);
        CHECK(view(0,120).right && view(0,121).right==false);
        CHECK(view(0,32).right && view(0,120).left);
        for(int h:{-40,0,3,200}) { const auto v=view(2,h); CHECK(v.left && v.right); } // no inset
        CHECK(view(3,0).left && view(3,0).right); // outside the plan
        SceneryInsetView both; scenery_repeat_inset_view(p,0,31,240,both); scenery_repeat_inset_view(p,1,121,240,both);
        CHECK(!both.left && !both.right);
        ++cases;
    }
    // Repeating floor: a floor drawn as mirrored halves wraps instead of
    // reflecting, every floor row shares one period per side, and the band
    // (rules, insets) is exactly the plan without the option.
    {
        auto m=map(12,{{0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14},
                        {0x21,0x22,0x23,0x24,0x21,0x22,0x23,0x24,0x21,0x22,0x23,0x24},
                        {0x61,0x62,0x63,0x64,0x65,0x66,0x466,0x465,0x464,0x463,0x462,0x461},
                        {0x71,0x72,0x73,0x74,0x75,0x76,0x476,0x475,0x474,0x473,0x472,0x471},
                        {0x81,0x82,0x83,0x81,0x82,0x83,0x81,0x82,0x83,0x81,0x82,0x83}});
        const auto old=plan(m,10,{},2),p=plan(m,10,{},2,{},true);
        CHECK(p.band==2 && old.band==2);
        for(auto* side:{&p.left,&p.right}) {
            for(unsigned y=2;y<5;++y) {
                CHECK(!side->rows[y].mirror && !side->rows[y].sequence);
                CHECK(side->rows[y].span==side->rows[2].span); // one period for the floor
            }
        }
        for(unsigned y=0;y<2;++y) {
            CHECK(p.left.rows[y].span==old.left.rows[y].span && p.right.rows[y].span==old.right.rows[y].span);
            CHECK(!p.left.rows[y].mirror && !p.right.rows[y].mirror);
        }
        // The floor rows still map whole tiles; nothing in them is reflected.
        for(unsigned y=2;y<5;++y) for(int x=-300;x<400;++x) {
            const int s=scenery_repeat_x(p,y,x),t=scenery_repeat_x(p,y,x+1);
            if(x+1==0 || x+1==96) continue;
            CHECK(t-s==1 || t-s==1-int(p.right.rows[y].span)*8 || t-s==1-int(p.left.rows[y].span)*8);
        }
        check_mapping(m,p); ++cases;
    }
    // The band, its insets and padding never depend on the floor option.
    {
        constexpr std::uint16_t F=0x40;
        auto framed=map(12,{{F,F,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,F,F},
                             {F,F,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,F,F},
                             {0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0x4a6,0x4a5,0x4a4,0x4a3,0x4a2,0x4a1},
                             {0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc}});
        auto edges=map(10,{{0x10,0x11,0x12,0x13,0x14,0x11,0x12,0x13,0x14,0x19},
                            {0x20,0x21,0x22,0x23,0x24,0x21,0x22,0x23,0x24,0x29},
                            {0x50,0x51,0x52,0x50,0x51,0x52,0x50,0x51,0x52,0x53}});
        for(const auto* m:{&framed,&edges}) {
            const swordcraft3::SceneryRepeatInsets in{2,3,2};
            const auto a=plan(*m,10,in,2),b=plan(*m,10,in,2,{},true);
            CHECK(a.valid && b.valid && a.inset_rows==b.inset_rows && a.band==b.band);
            for(auto pr:{std::make_pair(&a.left,&b.left),std::make_pair(&a.right,&b.right),
                         std::make_pair(&a.left_edge,&b.left_edge),std::make_pair(&a.right_edge,&b.right_edge)}) {
                CHECK(pr.first->inset==pr.second->inset && pr.first->padding==pr.second->padding);
                for(unsigned y=0;y<a.band;++y)
                    CHECK(pr.first->rows[y].span==pr.second->rows[y].span &&
                          pr.first->rows[y].mirror==pr.second->rows[y].mirror &&
                          pr.first->rows[y].inset==pr.second->rows[y].inset);
            }
            check_mapping(*m,b); check_mapping(*m,b,false,false);
        }
        // A layer that is all band (a backdrop) plans identically either way.
        auto back=map(8,{{0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18},{0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28}});
        const auto a=plan(back,10,{},2),b=plan(back,10,{},2,{},true);
        CHECK(same_side(a.left,b.left,2) && same_side(a.right,b.right,2));
        ++cases;
    }
    // Reviewed sequences: band rows follow the cells (reading order, the left
    // side read back from its last cell, flipped cells mirrored), floor rows
    // keep the planner's rule, and the fallback takes a sequence only when it
    // replaces no more than the padding.
    {
        constexpr std::uint16_t F=0x40;
        auto m=map(12,{{F,F,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,F,F},
                        {F,F,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,F,F},
                        {0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac}});
        auto p=plan(m,10,{2,3,3},2,{},true);
        const auto floor_left=p.left.rows[2],floor_right=p.right.rows[2];
        CHECK(p.left.padding==2 && p.right.padding==2 && p.band==2);
        using swordcraft3::scenery_special_detail::sequence;
        // Right: edge 9 | 5, 3, 4, 5, 3, ...; left: ... 7, 9 | edge 2.
        CHECK(scenery_repeat_apply_sequence(p,true,sequence(2,{{5,5},{3,4}})));
        CHECK(scenery_repeat_apply_sequence(p,false,sequence(2,{{7,7},{9,9}})));
        for(unsigned y=0;y<2;++y) {
            CHECK(p.right.rows[y].sequence && p.left.rows[y].sequence);
            CHECK(p.right_edge.rows[y].sequence && p.left_edge.rows[y].sequence); // inset == padding
            const unsigned right[]{5,3,4,5,3,4},left[]{9,7,9,7};
            for(unsigned k=0;k<6;++k) CHECK(shown_tile(m,p,y,int(10+k))==m.at(y,right[k]));
            for(unsigned k=0;k<4;++k) CHECK(shown_tile(m,p,y,int(1-k))==m.at(y,left[k]));
            for(int t=2;t<10;++t) CHECK(shown_tile(m,p,y,t)==m.at(y,unsigned(t))); // kept strip
        }
        CHECK(p.left.rows[2].span==floor_left.span && !p.left.rows[2].sequence);
        CHECK(p.right.rows[2].span==floor_right.span && !p.right.rows[2].sequence);
        check_mapping(m,p); check_mapping(m,p,false,false);
        // Flipped cells: a reflection at the edge ping-ponging over columns 7-9.
        auto q=plan(m,10,{2,3,3},2,{},true);
        CHECK(scenery_repeat_apply_sequence(q,true,sequence(3,{{7,9,true},{7,9}})));
        CHECK(q.right.rows[0].inset==3 && !q.right_edge.rows[0].sequence); // deeper than padding: guarded
        const unsigned cols[]{9,8,7,7,8,9,9};
        const bool flips[]{true,true,true,false,false,false,true};
        for(unsigned k=0;k<7;++k) CHECK(shown_tile(m,q,0,int(9+k))==canon(m.at(0,cols[k])^(flips[k]?H:0)));
        for(int d=0;d<24;++d) CHECK(scenery_repeat_x(q,0,72+d)==79-d); // pixel-exact mirror
        // The guard sees the replaced art column 9 (beyond padding 2).
        SceneryInsetView v; scenery_repeat_inset_view(q,0,0,73,v);
        CHECK(!v.right);
        CHECK(shown_tile(m,q,0,9,true,false)==m.at(0,9)); // fallback keeps column 9
        // Invalid sequences change nothing.
        auto r=plan(m,10,{2,3,3},2,{},true);
        SceneryRepeatSequence empty; empty.inset=2;
        auto beyond=sequence(2,{{3,4}}); beyond.cells[1]=12;
        auto deep=sequence(11,{{3,4}});
        CHECK(!scenery_repeat_apply_sequence(r,true,empty));
        CHECK(!scenery_repeat_apply_sequence(r,true,beyond));
        CHECK(!scenery_repeat_apply_sequence(r,true,deep));
        CHECK(!r.right.rows[0].sequence && !r.right_edge.rows[0].sequence);
        SceneryRepeatPlan none;
        CHECK(!scenery_repeat_apply_sequence(none,true,sequence(0,{{1,2}})));
        auto bandless=plan(m); // floor_row 0: no band to replace
        CHECK(!scenery_repeat_apply_sequence(bandless,true,sequence(0,{{1,2}})));
        ++cases;
    }
    // Special cases: only the four reviewed arenas carry sequences, each one
    // valid for a 48-column map; every other arena and layer is planned.
    {
        unsigned special=0;
        for(unsigned arena=0;arena<21;++arena) for(bool far:{false,true}) {
            const auto s=battle_scenery_special(arena,far);
            const bool expected=(!far && (arena==6 || arena==7 || arena==14)) || (far && arena==13);
            CHECK((s.left.length || s.right.length)==expected);
            for(const auto* q:{&s.left,&s.right}) {
                if(!q->length) continue;
                ++special;
                CHECK(q->inset<=4);
                for(unsigned i=0;i<q->length;++i)
                    CHECK(unsigned(q->cells[i]&~SceneryRepeatSequence::flip)<48u);
            }
        }
        CHECK(special==6);
        for(unsigned arena=0;arena<21;++arena) for(bool far:{false,true}) {
            const auto s=battle_scenery_special(arena,far);
            CHECK(s.quilt_left.enabled==(far && arena==13) && !s.quilt_right.enabled);
        }
        // Arena 7 takes arena 3's band: left w41 and right w42 from the filler.
        const auto seven=battle_scenery_special(7,false);
        CHECK(seven.left.length==41 && seven.left.cells[0]==3 && seven.left.cells[40]==43);
        CHECK(seven.right.length==42 && seven.right.cells[0]==3 && seven.right.cells[41]==44);
        // Arena 6 completes its cut edge objects by reflection: the tower at
        // column 3 (kept) and the wall at column 43 (44 replaced).
        constexpr std::uint8_t f=SceneryRepeatSequence::flip;
        const auto six=battle_scenery_special(6,false);
        CHECK(six.left.inset==3 && six.left.length==38);
        CHECK(six.left.cells[37]==(3|f) && six.left.cells[19]==(21|f) && six.left.cells[18]==21 && six.left.cells[0]==3);
        CHECK(six.right.inset==4 && six.right.length==44);
        CHECK(six.right.cells[0]==(43|f) && six.right.cells[21]==(22|f) && six.right.cells[22]==22 && six.right.cells[43]==43);
        // Arena 14's right edge reflects at column 43 and ping-pongs.
        const auto fourteen=battle_scenery_special(14,false);
        CHECK(fourteen.right.length==16 && fourteen.right.inset==4 && !fourteen.left.length);
        CHECK(fourteen.right.cells[0]==(43|SceneryRepeatSequence::flip) &&
              fourteen.right.cells[7]==(36|SceneryRepeatSequence::flip) && fourteen.right.cells[8]==36);
        // Arena 13's backdrop keeps its authored orb columns 35-43 on the
        // right and quilts its lava on the left from columns 0-17.
        const auto thirteen=battle_scenery_special(13,true);
        CHECK(thirteen.right.inset==4 && thirteen.right.cells[0]==17 &&
              thirteen.right.cells[thirteen.right.length-1]==43 && !thirteen.left.length);
        CHECK(thirteen.quilt_left.run_first==0 && thirteen.quilt_left.run_last==17 &&
              thirteen.quilt_left.overlap==40);
        ++cases;
    }
    // Quilted reflection on synthetic solid tiles (tile t: palette index t).
    // An edge whose colour the run also holds is joined directly (cut 0);
    // otherwise the cut moves to where the reflection and the run agree,
    // paying one unit per reflected pixel.
    {
        std::vector<std::uint8_t> chars(8*32),palette(0x200,0x10);
        for(unsigned t=1;t<8;++t) for(unsigned b=0;b<32;++b) chars[t*32+b]=std::uint8_t(t|(t<<4));
        SceneryTileArt art; art.chars=chars.data(); art.size=chars.size(); art.palette=palette.data();
        // Edge 5 then 6; the run (columns 4-11) is all 6 except column 9.
        auto direct=map(12,{{5,6,6,6,6,6,6,6,6,5,6,6}});
        auto p=plan(direct,10,{},1,art,true);
        CHECK(scenery_repeat_apply_quilt(p,false,direct.e.data(),art,4,11,8,16));
        CHECK(p.left.rows[0].quilt && p.left.quilt.lines==8 && p.left.quilt.source==72);
        for(unsigned y=0;y<8;++y) {
            CHECK(p.left.quilt.cut[y]==0);
            CHECK(scenery_repeat_x(p,0,-1,true,true,int(y))==72 && scenery_repeat_x(p,0,-2,true,true,int(y))==71);
        }
        CHECK(p.right.rows[0].quilt==0); // the other side keeps its plan
        CHECK(scenery_repeat_x(p,0,40,true,true,3)==40); // kept strip
        auto deep=map(12,{{5,6,6,6,6,6,6,6,6,6,6,6}});
        auto q=plan(deep,10,{},1,art,true);
        CHECK(scenery_repeat_apply_quilt(q,false,deep.e.data(),art,4,11,12,16));
        CHECK(q.left.quilt.source==47);
        for(unsigned y=0;y<8;++y) {
            CHECK(q.left.quilt.cut[y]==9);
            for(int d=0;d<9;++d) CHECK(scenery_repeat_x(q,0,-1-d,true,true,int(y))==d);  // reflection
            for(int d=9;d<16;++d) CHECK(scenery_repeat_x(q,0,-1-d,true,true,int(y))==47-d); // the run
        }
        // Right side mirrors the same rules.
        auto flipped=map(12,{{6,6,6,6,6,6,6,6,6,6,6,5}});
        auto r=plan(flipped,10,{},1,art,true);
        CHECK(scenery_repeat_apply_quilt(r,true,flipped.e.data(),art,0,7,12,16));
        CHECK(r.right.quilt.source==0 && r.right.quilt.cut[0]==9);
        CHECK(scenery_repeat_x(r,0,96,true,true,0)==95 && scenery_repeat_x(r,0,96+9,true,true,0)==9);
        // Invalid requests change nothing.
        auto s=plan(deep,10,{},1,art,true);
        CHECK(!scenery_repeat_apply_quilt(s,false,deep.e.data(),art,4,11,16,16));  // overlap >= margin
        CHECK(!scenery_repeat_apply_quilt(s,false,deep.e.data(),art,4,12,8,16));   // beyond the map
        CHECK(!scenery_repeat_apply_quilt(s,false,deep.e.data(),{},4,11,8,16));    // no tile art
        CHECK(!scenery_repeat_apply_quilt(s,false,deep.e.data(),art,10,11,8,24));  // run too short
        CHECK(!s.left.rows[0].quilt);
        ++cases;
    }
    std::cout<<"PASS: "<<cases<<" scenery repeat cases; authored joins, reflections, insets, native-view guard, vertical coherence, repeating floors, reviewed sequences and continuous mapping\n";
}
