#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace swordcraft3 {
// Repeat scenery: continue an authored text-BG strip past its edges instead of
// cropping it. Decided from tile-map ENTRIES only (tile index, flips, palette
// bank), never from colors, framebuffers or screenshots.
//
// A join between two tiles is AUTHORED when that exact left/right pair, or its
// flipped image, already occurs in the map: the original art is continuous
// across it by construction. Each tile row continues with one rule:
//   wrap(P):   repeat the outer P authored columns (a P-tile period);
//   mirror(N): reflect at the edge, ping-ponging over the outer N columns.
// A reflection is pixel-continuous by construction. It is accepted freely only
// in rows whose own art already contains mirrored tiles (the perspective floors
// are drawn as mirrored halves). Rows that switch rule pay for every vertical
// tile pair that never occurs in the map, so trunks are not sheared apart.
//
// Insets: the outermost columns of some rows may never be shown by the native
// view (overscan for the perspective floor below them). Their art can end in
// a tile with no authored neighbour. A side may then start its continuation
// up to that many columns inside the edge, replacing those columns, when that
// scores better. The caller must fall back to the inset-free plan whenever the
// native view reaches a replaced column.
struct SceneryRepeatRule {
    std::uint8_t mirror=0, span=0; // span: P for wrap, N for mirror (tiles)
    std::uint8_t inset=0;          // edge columns this row replaces
};
struct SceneryRepeatSide {
    static constexpr unsigned max_rows=32;
    std::array<SceneryRepeatRule,max_rows> rows{};
    unsigned cost=0, unauthored=0; // joins inside the planning window
    unsigned inset=0;              // replaced columns in the inset rows
};
// Edge columns the native view never shows in tile rows [0, rows).
struct SceneryRepeatInsets { unsigned rows=0, left=0, right=0; };
struct SceneryRepeatPlan {
    static constexpr unsigned max_columns=64, max_inset=8;
    unsigned columns=0, rows=0, reach=0, inset_rows=0;
    SceneryRepeatSide left{}, right{};
    // The same planner restricted to the authored edge (no insets).
    SceneryRepeatSide left_edge{}, right_edge{};
    bool valid=false;
};

namespace scenery_repeat_detail {
constexpr std::uint16_t hflip=0x400, vflip=0x800;
// Tile 0 is the blank tile in every reviewed arena map; its flips and palette
// bank are not visible, so all blank entries compare equal.
inline std::uint16_t canon(std::uint16_t e) { return (e&0x3ffu) ? e : 0; }
inline std::uint32_t pair(std::uint16_t a,std::uint16_t b) {
    return (std::uint32_t(canon(a))<<16)|canon(b);
}
struct Pairs {
    std::vector<std::uint32_t> values;
    void add(std::uint32_t v) { values.push_back(v); }
    void finish() {
        std::sort(values.begin(),values.end());
        values.erase(std::unique(values.begin(),values.end()),values.end());
    }
    bool has(std::uint16_t a,std::uint16_t b) const {
        return std::binary_search(values.begin(),values.end(),pair(a,b));
    }
};
// Source column/flip of extension tile k (1-based, counted outward from the
// continuation's edge: column lo on the left, hi-1 on the right).
struct Source { unsigned column=0; bool flip=false; };
inline Source source(unsigned lo,unsigned hi,bool right,SceneryRepeatRule rule,unsigned k) {
    const unsigned span=rule.span;
    if(!rule.mirror) {
        const unsigned phase=(k-1)%span;
        return {right ? hi-span+phase : lo+span-1-phase,false};
    }
    const unsigned m=(k-1)%(2*span);
    if(right) return m<span ? Source{hi-1-m,true} : Source{hi-span+(m-span),false};
    return m<span ? Source{lo+m,true} : Source{lo+span-1-(m-span),false};
}
} // namespace scenery_repeat_detail

// Map a source pixel column back into the strip using the plan for that tile
// row. Columns inside the kept strip are returned unchanged. Pass false for a
// side whose replaced edge columns the native view shows this frame: that
// side then uses the inset-free plan.
inline int scenery_repeat_x(const SceneryRepeatPlan& plan,unsigned row,int x,
                            bool left_inset=true,bool right_inset=true) {
    if(!plan.valid || row>=plan.rows) return x;
    const auto l=(left_inset?plan.left:plan.left_edge).rows[row];
    const auto r=(right_inset?plan.right:plan.right_edge).rows[row];
    const int width=int(plan.columns)*8,lo=int(l.inset)*8,hi=width-int(r.inset)*8;
    if(x>=lo && x<hi) return x;
    const bool right=x>=hi;
    const auto rule=right?r:l;
    const int span=int(rule.span)*8;
    if(span<=0 || span>(right?hi:width-lo)) return x;
    const int distance=right ? x-hi : lo-1-x;
    if(!rule.mirror) {
        const int phase=distance%span;
        return right ? hi-span+phase : lo+span-1-phase;
    }
    const int m=distance%(2*span);
    if(right) return m<span ? hi-1-m : hi-span+(m-span);
    return m<span ? lo+m : lo+span-1-(m-span);
}

// Whether a native view of source columns [h, h+view) on tile row `row`
// leaves each side's replaced columns to the margins. A side found showing
// one must use its inset-free plan for the frame.
struct SceneryInsetView { bool left=true, right=true; };
inline void scenery_repeat_inset_view(const SceneryRepeatPlan& plan,unsigned row,int h,int view,
                                      SceneryInsetView& out) {
    if(!plan.valid || row>=plan.rows) return;
    const int left=int(plan.left.rows[row].inset)*8,right=int(plan.right.rows[row].inset)*8;
    if(left && h<left) out.left=false;
    if(right && h+view>int(plan.columns)*8-right) out.right=false;
}

// entries: row-major columns x rows. reach: extension tiles scored past the
// authored edge. insets: replaceable edge columns per side (none by default).
inline SceneryRepeatPlan plan_scenery_repeat(const std::uint16_t* entries,unsigned columns,
                                             unsigned rows,unsigned reach=10,
                                             SceneryRepeatInsets insets={}) {
    using namespace scenery_repeat_detail;
    SceneryRepeatPlan plan;
    if(!entries || columns<2 || columns>SceneryRepeatPlan::max_columns ||
       !rows || rows>SceneryRepeatSide::max_rows || !reach || reach>64) return plan;
    insets.rows=std::min(insets.rows,rows);
    if(!insets.rows) insets.left=insets.right=0;
    if(insets.left>SceneryRepeatPlan::max_inset || insets.right>SceneryRepeatPlan::max_inset ||
       insets.left+insets.right+2>columns) return plan;
    auto at=[&](unsigned y,unsigned x) { return canon(entries[y*columns+x]); };
    // Authored joins, closed under the flips that preserve each adjacency.
    Pairs across,down;
    for(unsigned y=0;y<rows;++y) for(unsigned x=0;x<columns;++x) {
        const std::uint16_t a=at(y,x);
        if(x+1<columns) {
            const std::uint16_t b=at(y,x+1);
            across.add(pair(a,b)); across.add(pair(b^hflip,a^hflip));
            across.add(pair(a^vflip,b^vflip)); across.add(pair(b^hflip^vflip,a^hflip^vflip));
        }
        if(y+1<rows) {
            const std::uint16_t b=at(y+1,x);
            down.add(pair(a,b)); down.add(pair(a^hflip,b^hflip));
            down.add(pair(b^vflip,a^vflip)); down.add(pair(b^hflip^vflip,a^hflip^vflip));
        }
    }
    across.finish(); down.finish();
    // Rows whose art already contains a tile next to its own mirror image.
    std::array<bool,SceneryRepeatSide::max_rows> mirrored{};
    for(unsigned y=0;y<rows;++y)
        for(unsigned x=0;x<columns && !mirrored[y];++x) {
            const std::uint16_t e=at(y,x);
            if(!e) continue;
            for(unsigned z=0;z<columns;++z)
                if(at(y,z)==canon(e^hflip)) { mirrored[y]=true; break; }
        }
    // Costs: authored 0; reflection in mirrored art 1; any other join 20 (a
    // wrap whose neighbouring columns still agree is a near-period: >=12);
    // each vertical tile pair that never occurs in the map 5.
    constexpr unsigned reflection=1,unauthored=20,unauthored_mirror=21,vertical=5;
    constexpr unsigned never=1u<<28; // span longer than that row's strip
    const unsigned rules=2*columns,cells=2*columns;
    auto rule_of=[&](unsigned r,unsigned inset) {
        return r<columns ? SceneryRepeatRule{0,std::uint8_t(r+1),std::uint8_t(inset)}
                         : SceneryRepeatRule{1,std::uint8_t(2*columns-r),std::uint8_t(inset)};
    };
    // A cell is a (column, flip) pair: column*2+flip.
    auto tile=[&](unsigned y,unsigned cell) { return canon(at(y,cell/2)^((cell&1)?hflip:0)); };
    // Vertical compatibility of each above/below cell pair, per row boundary.
    std::vector<std::uint8_t> fits((rows-1)*cells*cells);
    for(unsigned y=1;y<rows;++y)
        for(unsigned c1=0;c1<cells;++c1) for(unsigned c2=0;c2<cells;++c2)
            fits[((y-1)*cells+c1)*cells+c2]=down.has(tile(y-1,c1),tile(y,c2));
    for(int side=0;side<2;++side) {
        const bool right=side==1;
        auto join_cost=[&](unsigned y,std::uint16_t a,std::uint16_t b,unsigned bonus,unsigned& bad) {
            if(across.has(a,b)) return 0u;
            ++bad;
            if(b==canon(a^hflip)) {
                if(mirrored[y]) { --bad; return reflection; }
                return unauthored_mirror;
            }
            return unauthored-bonus;
        };
        // Every candidate inset is scored over the same absolute window: the
        // replaceable columns plus `reach` tiles past the edge. Window cell o
        // counts outward from the innermost replaceable column.
        const unsigned cap=right?insets.right:insets.left,window=reach+cap;
        const unsigned inner=right?columns-cap-1:cap; // kept by every candidate
        std::vector<std::uint8_t> cell(rows*rules*window);
        std::vector<unsigned> horizontal(rows*rules),horizontal_bad(rows*rules);
        std::vector<unsigned> best(rules),next(rules),order(rules);
        std::vector<std::uint16_t> back(rows*rules);
        for(unsigned inset=0;inset<=cap;++inset) {
            auto inset_of=[&](unsigned y) { return y<insets.rows ? inset : 0u; };
            // Horizontal cost of each rule in each row, over the window.
            for(unsigned y=0;y<rows;++y) {
                const unsigned row_inset=inset_of(y),kept=cap-row_inset;
                const unsigned lo=right?0:row_inset,hi=right?columns-row_inset:columns;
                for(unsigned r=0;r<rules;++r) {
                    const auto rule=rule_of(r,row_inset);
                    horizontal_bad[y*rules+r]=0;
                    if(rule.span>hi-lo) { horizontal[y*rules+r]=never; continue; }
                    std::uint8_t* c=&cell[(y*rules+r)*window];
                    for(unsigned o=0;o<window;++o) {
                        if(o<kept) { c[o]=std::uint8_t(2*(right?columns-cap+o:cap-1-o)); continue; }
                        const auto s=source(lo,hi,right,rule,o-kept+1);
                        c[o]=std::uint8_t(2*s.column+(s.flip?1:0));
                    }
                    unsigned cost=0,bad=0,bonus=0;
                    if(!rule.mirror) {
                        // Near-period evidence: the columns before the join match.
                        const unsigned p=rule.span,strip=hi-lo;
                        for(unsigned j=1;j<=4;++j) {
                            if(right ? (strip<p+1+j) : (p+j>=strip)) break;
                            const bool same=right ? at(y,hi-1-j)==at(y,hi-p-1-j)
                                                  : at(y,lo+j)==at(y,lo+p+j);
                            if(same) bonus+=2;
                        }
                    }
                    std::uint16_t previous=at(y,inner);
                    for(unsigned o=0;o<window;++o) {
                        const std::uint16_t t=tile(y,c[o]);
                        cost+=right ? join_cost(y,previous,t,bonus,bad) : join_cost(y,t,previous,bonus,bad);
                        previous=t;
                    }
                    horizontal[y*rules+r]=cost; horizontal_bad[y*rules+r]=bad;
                }
            }
            // Viterbi over rows. Ties prefer the earlier rule: short wraps,
            // then the widest reflection, keeping the choice deterministic.
            // Predecessors are visited cheapest first, so the search stops as
            // soon as no remaining one can win or tie.
            for(unsigned r=0;r<rules;++r) best[r]=horizontal[r];
            for(unsigned y=1;y<rows;++y) {
                const std::uint8_t* fit=&fits[(y-1)*cells*cells];
                for(unsigned r=0;r<rules;++r) order[r]=r;
                std::stable_sort(order.begin(),order.end(),[&](unsigned a,unsigned b) { return best[a]<best[b]; });
                for(unsigned r=0;r<rules;++r) {
                    back[y*rules+r]=0;
                    if(horizontal[y*rules+r]>=never) { next[r]=never; continue; }
                    const std::uint8_t* below=&cell[(y*rules+r)*window];
                    unsigned chosen=0,total=never;
                    for(unsigned q:order) {
                        if(best[q]>total || best[q]>=never) break;
                        const std::uint8_t* above=&cell[((y-1)*rules+q)*window];
                        unsigned cost=best[q];
                        for(unsigned o=0;o<window && cost<=total;++o)
                            if(!fit[above[o]*cells+below[o]]) cost+=vertical;
                        if(cost<total || (cost==total && q<chosen)) { total=cost; chosen=q; }
                    }
                    next[r]=total+horizontal[y*rules+r];
                    back[y*rules+r]=std::uint16_t(chosen);
                }
                best.swap(next);
            }
            unsigned last=0;
            for(unsigned r=1;r<rules;++r) if(best[r]<best[last]) last=r;
            SceneryRepeatSide out;
            out.cost=best[last]; out.inset=inset;
            for(unsigned y=rows;y-->0;) {
                out.rows[y]=rule_of(last,inset_of(y));
                out.unauthored+=horizontal_bad[y*rules+last];
                if(y) last=back[y*rules+last];
            }
            // Keep the authored edge unless an inset scores strictly better.
            auto& chosen=right?plan.right:plan.left;
            if(!inset) (right?plan.right_edge:plan.left_edge)=out;
            if(!inset || out.cost<chosen.cost) chosen=out;
        }
    }
    plan.columns=columns; plan.rows=rows; plan.reach=reach; plan.inset_rows=insets.rows;
    plan.valid=true;
    return plan;
}

// Stable identity of a planned map, so a battle only re-plans on change.
inline std::uint32_t scenery_repeat_hash(const std::uint16_t* entries,unsigned count) {
    std::uint32_t h=2166136261u;
    for(unsigned n=0;n<count;++n) { h^=entries[n]; h*=16777619u; }
    return h;
}
} // namespace swordcraft3
