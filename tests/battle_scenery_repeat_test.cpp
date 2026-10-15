#include "battle_scenery_repeat.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(x) do { if(!(x)) throw std::runtime_error("Failed: " #x " (line "+std::to_string(__LINE__)+")"); } while(0)

// Synthetic tile-map ENTRIES only. No ROM data, tile graphics or pixels.
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
SceneryRepeatPlan plan(const Map& m,unsigned reach=10,swordcraft3::SceneryRepeatInsets insets={}) {
    return swordcraft3::plan_scenery_repeat(m.e.data(),m.columns,m.rows,reach,insets);
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
            CHECK(!side->rows[y].mirror && side->rows[y].span==4);
        CHECK(p.left.cost==0 && p.right.cost==0 && p.left.unauthored==0 && p.right.unauthored==0);
        for(int x=-200;x<300;++x) {
            const int s=scenery_repeat_x(p,0,x);
            CHECK(((s-x)%32+32)%32==0); // same phase of the 32-pixel period
        }
        const auto a=audit(m,p);
        CHECK(a.across==0 && a.reflections==0 && a.down==0);
        check_mapping(m,p); ++cases;
    }
    // Open space beside a building, over a plain floor tile: continue the open
    // column instead of repeating the building (shortest authored period).
    {
        auto m=map(10,{{0,0,0x31,0x32,0x33,0x433,0x432,0x431,0,0},
                        {0,0,0x41,0x42,0x43,0x443,0x442,0x441,0,0},
                        {0x50,0x50,0x51,0x52,0x53,0x453,0x452,0x451,0x50,0x50}});
        const auto p=plan(m);
        for(unsigned y=0;y<3;++y) for(auto* side:{&p.left,&p.right})
            CHECK(!side->rows[y].mirror && side->rows[y].span==1);
        CHECK(p.left.cost==0 && p.right.cost==0);
        CHECK(extension_tile(m,p,2,7,true)==0x50 && extension_tile(m,p,0,3,false)==0);
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
            for(unsigned y=0;y<2;++y) CHECK(!side->rows[y].mirror && side->rows[y].span==4);
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
            for(unsigned y=0;y<2;++y) CHECK(side->rows[y].inset==1 && !side->rows[y].mirror && side->rows[y].span==4);
            CHECK(side->rows[2].inset==0 && side->rows[2].span==1);
            CHECK(side->cost==0 && side->unauthored==0);
        }
        // The inset-free result is kept for fallback and equals a plain plan.
        CHECK(same_side(p.left_edge,edge.left,m.rows) && same_side(p.right_edge,edge.right,m.rows));
        CHECK(edge.left.unauthored==2 && edge.right.unauthored==2 && edge.left.cost==40);
        // Replaced column 0 now shows column 4; column 9 shows column 5.
        for(int x=0;x<8;++x) CHECK(scenery_repeat_x(p,0,x)==32+x && scenery_repeat_x(p,0,72+x)==40+x);
        CHECK(scenery_repeat_x(p,2,0)==0 && scenery_repeat_x(p,2,79)==79); // native row kept
        // A side whose replaced column is natively visible falls back alone.
        for(int x=0;x<80;++x) CHECK(scenery_repeat_x(p,0,x,false,true)==(x>=72?x-32:x));
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
    std::cout<<"PASS: "<<cases<<" scenery repeat cases; authored joins, reflections, insets, native-view guard, vertical coherence and continuous mapping\n";
}
