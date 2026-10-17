#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
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
// Scenery rows (trees, rocks, buildings: every row above `floor_row`) repeat
// as whole columns: they share one wrap period per side, chosen on those rows
// alone, so each extension column is an exact copy of an authored column and
// no object is mirrored, sheared or left as a stray fragment. The near
// layer's perspective floor rows then choose a rule per row (as do plans
// without a band, `floor_row` 0); a floor row may reflect when its own art
// already contains mirrored tiles (floors are drawn as mirrored halves).
//
// Framed art (padding on both sides, below) loops: the seam where its right
// edge meets its left edge joins two objects that were both drawn to be cut
// by the screen edge, and is preferred over any other non-authored seam.
//
// Insets: the outermost columns of some rows may never be shown by the native
// view (overscan for the perspective floor below them). Their art can end in
// a tile with no authored neighbour. A side may then start its continuation
// up to that many columns inside the edge, replacing those columns, when that
// scores better. The caller must fall back to the inset-free plan whenever the
// native view reaches a replaced column.
//
// Padding: replaceable edge columns that hold one single entry in every inset
// row are blank filler around the art, not art (arenas 2/3/7 frame their
// scenery with three such columns per side). Continuing through filler leaves
// an empty band between copies of the art, so padding is always replaced, no
// continuation sources from it, and joins with it are not authored (the art
// edge beside it was drawn to be cut by the screen edge). Replacing filler
// needs no native-view guard: when the native view shows it, it shows the
// game's own filler. The fallback plan replaces padding only.
//
// Repeating floors: with `repeat_floor`, the floor rows also share one wrap
// period per side, chosen on the floor rows alone, so the perspective floor
// continues as one repeating pattern instead of a reflection or per-row
// periods that break its lattice between rows. The period is at least four
// tiles: two or three repeat an edge strip as stripes, and on arena 16 the
// strip holds none of the floor's ice cubes, so its pattern would stop at the
// screen edge. Arena 8's pond still repeats as water (five tiles).
//
// Reviewed sequences: for the few arenas whose art no wrap period continues
// cleanly, a side's band may instead follow a hand-reviewed list of cells
// (scenery_repeat_apply_sequence); the planner still plans everything else.
struct SceneryRepeatRule {
    std::uint8_t mirror=0, span=0; // span: P for wrap, N for mirror (tiles)
    std::uint8_t inset=0;          // edge columns this row replaces
    std::uint8_t sequence=0;       // 1: follow the side's reviewed sequence
    std::uint8_t quilt=0;          // 1: follow the side's quilted reflection
};
// Quilted reflection (texture quilting's minimum-error cut, per pixel line):
// a narrow reflection continues the edge seamlessly, and an unreflected run
// of the art takes over where the two agree, along an irregular boundary
// instead of a straight seam. For amorphous backdrops (arena 13's lava)
// whose edge no column continues and whose wide reflection would read as a
// mirror image.
struct SceneryRepeatQuilt {
    static constexpr unsigned max_lines=256;
    int source=0;                 // run pixel shown at distance 0 past the edge
    unsigned run_lo=0, run_hi=0;  // the run's source pixels [run_lo, run_hi)
    unsigned lines=0;             // source pixel lines with a cut
    std::array<std::uint8_t,max_lines> cut{}; // reflected pixels per line
};
// Cells of a reviewed sequence: column, plus `flip` for its mirror image.
struct SceneryRepeatSequence {
    static constexpr unsigned max_cells=64;
    static constexpr std::uint8_t flip=0x80;
    std::array<std::uint8_t,max_cells> cells{}; // reading order, left to right
    unsigned length=0;
    unsigned inset=0; // edge columns the sequence replaces in the band rows
};
struct SceneryRepeatSide {
    static constexpr unsigned max_rows=32;
    std::array<SceneryRepeatRule,max_rows> rows{};
    unsigned cost=0, unauthored=0; // joins inside the planning window
    unsigned inset=0;              // replaced columns in the inset rows
    unsigned padding=0;            // filler columns; the inset is at least this
    SceneryRepeatSequence sequence{}; // used by rows with `sequence` set
    SceneryRepeatQuilt quilt{};       // used by rows with `quilt` set
};
// Edge columns the native view never shows in tile rows [0, rows). With
// `overscan`, they lie beyond the layer's whole scroll range (a backdrop's
// unreachable columns): their art was never placed beside what is shown, so
// they are padding however they are drawn.
struct SceneryRepeatInsets { unsigned rows=0, left=0, right=0; bool overscan=false; };
// Optional tile graphics behind the entries: the layer's 4bpp character data
// (from its char base) and the 256-colour BG palette (RGB555). The player
// approved this one use (2026-10-15): scoring how continuous a NON-authored
// join looks, to choose continuations. Never assertions or scene detection.
struct SceneryTileArt {
    const std::uint8_t* chars=nullptr; std::size_t size=0;
    const std::uint8_t* palette=nullptr;
    bool valid() const { return chars && palette && size>=32; }
};
struct SceneryRepeatPlan {
    static constexpr unsigned max_columns=64, max_inset=32;
    unsigned columns=0, rows=0, reach=0, inset_rows=0;
    unsigned band=0; // scenery band rows [0, band); the floor rows follow
    SceneryRepeatSide left{}, right{};
    // The same planner replacing padding only (the authored edge when there
    // is none): the plan a side falls back to.
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
// The two outer pixel lines on each side of one entry as it is drawn (flips
// applied): columns top to bottom, rows left to right. [0] is the outermost
// line, [1] the one inside it.
constexpr std::uint16_t clear=0x8000; // colour index 0: transparent
using Line=std::array<std::uint16_t,8>;
struct Edges { std::array<Line,2> left{},right{},top{},bottom{}; };
inline Edges edges(const SceneryTileArt& art,std::uint16_t e) {
    Edges out;
    for(auto* side:{&out.left,&out.right,&out.top,&out.bottom}) for(auto& line:*side) line.fill(clear);
    if(!canon(e)) return out;
    const unsigned tile=e&0x3ffu,bank=e>>12;
    const bool h=e&hflip,v=e&vflip;
    auto pixel=[&](unsigned x,unsigned y)->std::uint16_t {
        const unsigned sx=h?7-x:x,sy=v?7-y:y;
        const std::size_t at=std::size_t(tile)*32+sy*4+sx/2;
        if(at>=art.size) return clear;
        const unsigned index=(art.chars[at]>>((sx&1)*4))&15u;
        if(!index) return clear;
        const unsigned c=(bank*16+index)*2;
        return std::uint16_t((art.palette[c]|(art.palette[c+1]<<8))&0x7fff);
    };
    for(unsigned d=0;d<2;++d) for(unsigned i=0;i<8;++i) {
        out.left[d][i]=pixel(d,i); out.right[d][i]=pixel(7-d,i);
        out.top[d][i]=pixel(i,d); out.bottom[d][i]=pixel(i,7-d);
    }
    return out;
}
// Difference between two neighbouring pixels: summed RGB555 channel
// differences; an opaque pixel against a transparent one is a cut edge. Two
// transparent pixels show the layer behind: no difference.
inline unsigned distance(std::uint16_t a,std::uint16_t b) {
    if(a==clear || b==clear) return a==b ? 0u : 48u;
    unsigned total=0;
    for(unsigned s=0;s<15;s+=5) {
        const int d=int((a>>s)&31u)-int((b>>s)&31u);
        total+=unsigned(d<0?-d:d);
    }
    return total;
}
// How visible a join is where side a (outer line a[0], inner a[1]) touches
// side b: per pixel, the step across the join beyond the natural step
// between neighbouring pixels on either side. Foliage meeting foliage stays
// hidden in its own texture; foliage cut against flat rock or sky shows.
inline unsigned seam(const std::array<Line,2>& a,const std::array<Line,2>& b) {
    unsigned total=0;
    for(unsigned i=0;i<8;++i) {
        const unsigned across=2*distance(a[0][i],b[0][i]);
        const unsigned inside=distance(a[1][i],a[0][i])+distance(b[0][i],b[1][i]);
        if(across>inside) total+=across-inside;
    }
    return total/2;
}
} // namespace scenery_repeat_detail

// Map a source pixel column back into the strip using the plan for that tile
// row. Columns inside the kept strip are returned unchanged. Pass false for a
// side whose replaced art columns the native view shows this frame: that
// side then uses its fallback plan. `line` is the source pixel line (needed
// by quilted rows; they use the row's first line without it).
inline int scenery_repeat_x(const SceneryRepeatPlan& plan,unsigned row,int x,
                            bool left_inset=true,bool right_inset=true,int line=-1) {
    if(!plan.valid || row>=plan.rows) return x;
    const auto& ls=left_inset?plan.left:plan.left_edge;
    const auto& rs=right_inset?plan.right:plan.right_edge;
    const auto l=ls.rows[row],r=rs.rows[row];
    const int width=int(plan.columns)*8,lo=int(l.inset)*8,hi=width-int(r.inset)*8;
    if(x>=lo && x<hi) return x;
    const bool right=x>=hi;
    const auto rule=right?r:l;
    const int distance=right ? x-hi : lo-1-x;
    if(rule.quilt) {
        // Reflected up to the line's cut, then the run (repeating past its
        // end, beyond any margin the run was chosen to cover).
        const auto& q=(right?rs:ls).quilt;
        const int y=line>=0 ? line : int(row)*8;
        if(y<0 || unsigned(y)>=q.lines || q.run_hi<=q.run_lo) return x;
        if(distance<int(q.cut[unsigned(y)])) return right ? hi-1-distance : lo+distance;
        const int length=right ? int(q.run_hi)-q.source : q.source-int(q.run_lo)+1;
        if(length<=0) return x;
        return right ? q.source+distance%length : q.source-distance%length;
    }
    if(rule.sequence) {
        // Reviewed cells, repeated outward; the left side reads them back
        // from its last cell. A flipped cell is drawn mirrored.
        const auto& s=(right?rs:ls).sequence;
        const int n=int(s.length);
        if(n<=0 || n>int(SceneryRepeatSequence::max_cells)) return x;
        const int k=(distance/8)%n,within=distance%8;
        const unsigned cell=s.cells[unsigned(right?k:n-1-k)];
        const int column=int(cell&~unsigned(SceneryRepeatSequence::flip));
        const bool flip=cell&SceneryRepeatSequence::flip;
        if(column>=int(plan.columns)) return x;
        return column*8+((right!=flip) ? within : 7-within);
    }
    const int span=int(rule.span)*8;
    if(span<=0 || span>(right?hi:width-lo)) return x;
    if(!rule.mirror) {
        const int phase=distance%span;
        return right ? hi-span+phase : lo+span-1-phase;
    }
    const int m=distance%(2*span);
    if(right) return m<span ? hi-1-m : hi-span+(m-span);
    return m<span ? lo+m : lo+span-1-(m-span);
}

// Whether a native view of source columns [h, h+view) on tile row `row`
// leaves each side's replaced art columns (beyond its padding) to the
// margins. A side found showing one must use its fallback plan for the frame.
struct SceneryInsetView { bool left=true, right=true; };
inline void scenery_repeat_inset_view(const SceneryRepeatPlan& plan,unsigned row,int h,int view,
                                      SceneryInsetView& out) {
    if(!plan.valid || row>=plan.rows) return;
    const unsigned left=plan.left.rows[row].inset,right=plan.right.rows[row].inset;
    if(left>plan.left.padding && h<int(left)*8) out.left=false;
    if(right>plan.right.padding && h+view>int(plan.columns-right)*8) out.right=false;
}

// entries: row-major columns x rows. reach: extension tiles scored past the
// authored edge. insets: replaceable edge columns per side (none by default).
// floor_row: first tile row of the perspective floor. Rows above it are the
// scenery band and share one wrap period; pass `rows` for a layer without a
// floor. The default 0 plans every row on its own. art: when given, every
// non-authored join costs its visible seam instead of a flat penalty.
// repeat_floor: the rows from floor_row down share one wrap per side too.
inline SceneryRepeatPlan plan_scenery_repeat(const std::uint16_t* entries,unsigned columns,
                                             unsigned rows,unsigned reach=10,
                                             SceneryRepeatInsets insets={},
                                             unsigned floor_row=0,
                                             const SceneryTileArt& art={},
                                             bool repeat_floor=false) {
    using namespace scenery_repeat_detail;
    SceneryRepeatPlan plan;
    if(!entries || columns<2 || columns>SceneryRepeatPlan::max_columns ||
       !rows || rows>SceneryRepeatSide::max_rows || !reach || reach>64) return plan;
    insets.rows=std::min(insets.rows,rows);
    if(!insets.rows) insets.left=insets.right=0;
    if(insets.left>SceneryRepeatPlan::max_inset || insets.right>SceneryRepeatPlan::max_inset ||
       insets.left+insets.right+2>columns) return plan;
    auto at=[&](unsigned y,unsigned x) { return canon(entries[y*columns+x]); };
    // Padding per side: outermost replaceable columns that repeat one entry in
    // every inset row (with overscan: every replaceable column). No
    // continuation ever sources from either side's. The flat band's last one
    // or two rows may already be floor art across the full width (arena 5's
    // row 16): the padded band then ends above them and they plan as floor.
    auto padding=[&](unsigned band_rows) {
        std::array<unsigned,2> out{};
        for(int side=0;side<2;++side) {
            const bool right=side==1;
            const unsigned cap=right?insets.right:insets.left;
            if(insets.overscan) { out[side]=cap; continue; }
            const std::uint16_t filler=at(0,right?columns-1:0);
            unsigned& n=out[side];
            for(;n<cap;++n) {
                const unsigned x=right?columns-1-n:n;
                bool uniform=true;
                for(unsigned y=0;y<band_rows && uniform;++y) uniform=at(y,x)==filler;
                if(!uniform) break;
            }
        }
        return out;
    };
    auto pad=padding(insets.rows);
    for(unsigned trim=1;trim<=2 && !pad[0] && !pad[1] && insets.rows>trim+1;++trim) {
        const auto shorter=padding(insets.rows-trim);
        if(shorter[0] || shorter[1]) {
            pad=shorter; insets.rows-=trim; floor_row=std::min(floor_row,insets.rows);
        }
    }
    auto padded=[&](unsigned y,unsigned x) {
        return y<insets.rows && (x<pad[0] || x>=columns-pad[1]);
    };
    auto blank=[&](std::uint16_t e) {
        return (pad[0] && e==at(0,0)) || (pad[1] && e==at(0,columns-1));
    };
    // Authored joins, closed under the flips that preserve each adjacency.
    // A join with padding is not art: the art edge beside it was drawn to be
    // cut by the native screen edge.
    Pairs across,down;
    for(unsigned y=0;y<rows;++y) for(unsigned x=0;x<columns;++x) {
        const std::uint16_t a=at(y,x);
        if(x+1<columns && !padded(y,x) && !padded(y,x+1)) {
            const std::uint16_t b=at(y,x+1);
            across.add(pair(a,b)); across.add(pair(b^hflip,a^hflip));
            across.add(pair(a^vflip,b^vflip)); across.add(pair(b^hflip^vflip,a^hflip^vflip));
        }
        if(y+1<rows && !padded(y,x) && !padded(y+1,x)) {
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
    // Costs: authored 0; reflection in mirrored art 1. With tile art, any
    // other join (across or down) costs 1 plus its visible seam. Without it:
    // framed art's loop seam 1; any other join 20 (a wrap whose neighbouring
    // columns still agree is a near-period: >=12); each vertical tile pair
    // that never occurs in the map 5.
    constexpr unsigned reflection=1,loop=1,unauthored=20,unauthored_mirror=21,vertical=5;
    constexpr unsigned min_floor_period=4;
    const bool scored=art.valid();
    constexpr unsigned never=1u<<28; // span longer than that row's strip
    const unsigned rules=2*columns,cells=2*columns,band=std::min(floor_row,rows);
    // Rule order is the tie-break: the longest wrap first (the whole strip,
    // i.e. the arena loops), then the widest reflection.
    auto rule_of=[&](unsigned r,unsigned inset) {
        return r<columns ? SceneryRepeatRule{0,std::uint8_t(columns-r),std::uint8_t(inset)}
                         : SceneryRepeatRule{1,std::uint8_t(2*columns-r),std::uint8_t(inset)};
    };
    // A cell is a (column, flip) pair: column*2+flip.
    auto tile=[&](unsigned y,unsigned cell) { return canon(at(y,cell/2)^((cell&1)?hflip:0)); };
    // Edge pixels of every (row, cell) when the tile art is given.
    std::vector<Edges> edge(scored?rows*cells:0);
    for(unsigned y=0;y<rows && scored;++y)
        for(unsigned c=0;c<cells;++c) edge[y*cells+c]=edges(art,tile(y,c));
    // Vertical cost of each above/below cell pair, per row boundary.
    std::vector<std::uint16_t> vcost((rows-1)*cells*cells);
    for(unsigned y=1;y<rows;++y)
        for(unsigned c1=0;c1<cells;++c1) for(unsigned c2=0;c2<cells;++c2) {
            unsigned v=0;
            if(!down.has(tile(y-1,c1),tile(y,c2)))
                v=scored ? 1+seam(edge[(y-1)*cells+c1].bottom,edge[y*cells+c2].top) : vertical;
            vcost[((y-1)*cells+c1)*cells+c2]=std::uint16_t(std::min(v,65535u));
        }
    for(int side=0;side<2;++side) {
        const bool right=side==1;
        // Join between the cells shown left and right of a tile boundary.
        auto join_cost=[&](unsigned y,unsigned lc,unsigned rc,unsigned bonus,unsigned& bad) {
            const std::uint16_t a=tile(y,lc),b=tile(y,rc);
            if(across.has(a,b)) return 0u;
            ++bad;
            if(b==canon(a^hflip)) {
                // Pixel-continuous by construction (only floors may reflect).
                if(mirrored[y] || scored) { --bad; return reflection; }
                return unauthored_mirror;
            }
            if(scored) return 1+seam(edge[y*cells+lc].right,edge[y*cells+rc].left);
            // Framed art loops: its right edge meeting its left edge joins two
            // objects that were both drawn to be cut by the screen edge.
            if(y<band && pad[0] && pad[1] && a==at(y,columns-1-pad[1]) && b==at(y,pad[0]))
                return loop;
            return unauthored-bonus;
        };
        // Every candidate inset is scored over the same absolute window: the
        // replaceable columns plus `reach` tiles past the edge. Window cell o
        // counts outward from the innermost replaceable column.
        const unsigned cap=right?insets.right:insets.left,window=reach+cap;
        const unsigned inner=right?columns-cap-1:cap; // kept by every candidate
        const unsigned padding=pad[side],opposite=pad[1-side];
        std::vector<std::uint8_t> cell(rows*rules*window);
        std::vector<unsigned> horizontal(rows*rules),horizontal_bad(rows*rules);
        std::vector<unsigned> best(rules),next(rules),order(rules);
        std::vector<std::uint16_t> back(rows*rules);
        for(unsigned inset=padding;inset<=cap;++inset) {
            auto inset_of=[&](unsigned y) { return y<insets.rows ? inset : 0u; };
            // Horizontal cost of each rule in each row, over the window.
            for(unsigned y=0;y<rows;++y) {
                const unsigned row_inset=inset_of(y),kept=cap-row_inset;
                const unsigned skip=y<insets.rows?opposite:0; // the far side's filler
                const unsigned lo=right?skip:row_inset,hi=right?columns-row_inset:columns-skip;
                for(unsigned r=0;r<rules;++r) {
                    const auto rule=rule_of(r,row_inset);
                    horizontal_bad[y*rules+r]=0;
                    // A repeating floor wraps at least min_floor_period tiles
                    // (32 px), so it repeats the floor's pattern, not stripes.
                    const bool short_floor=repeat_floor && y>=band && rule.span<min_floor_period &&
                                           hi-lo>=min_floor_period;
                    if(rule.span>hi-lo || short_floor || (rule.mirror && (y<band || repeat_floor))) {
                        horizontal[y*rules+r]=never; continue;
                    }
                    std::uint8_t* c=&cell[(y*rules+r)*window];
                    for(unsigned o=0;o<window;++o) {
                        if(o<kept) { c[o]=std::uint8_t(2*(right?columns-cap+o:cap-1-o)); continue; }
                        const auto s=source(lo,hi,right,rule,o-kept+1);
                        c[o]=std::uint8_t(2*s.column+(s.flip?1:0));
                    }
                    unsigned cost=0,bad=0,bonus=0;
                    if(!rule.mirror && !scored) {
                        // Near-period evidence: the columns before the join match.
                        // Blank filler matching blank filler is no evidence.
                        const unsigned p=rule.span,strip=hi-lo;
                        for(unsigned j=1;j<=4;++j) {
                            if(right ? (strip<p+1+j) : (p+j>=strip)) break;
                            const std::uint16_t a=right ? at(y,hi-1-j) : at(y,lo+j);
                            const bool same=a==(right ? at(y,hi-p-1-j) : at(y,lo+p+j));
                            if(same && !blank(a)) bonus+=2;
                        }
                    }
                    unsigned previous=2*inner;
                    for(unsigned o=0;o<window;++o) {
                        const unsigned t=c[o];
                        cost+=right ? join_cost(y,previous,t,bonus,bad) : join_cost(y,t,previous,bonus,bad);
                        previous=t;
                    }
                    horizontal[y*rules+r]=cost; horizontal_bad[y*rules+r]=bad;
                }
            }
            // Cost of rule r over rows [from, to), with the vertical joins
            // between its consecutive rows.
            auto span_cost=[&](unsigned r,unsigned from,unsigned to,unsigned total) {
                for(unsigned y=from;y<to && total<never;++y) {
                    if(horizontal[y*rules+r]>=never) return never;
                    if(y>from) {
                        const std::uint16_t* vc=&vcost[(y-1)*cells*cells];
                        const std::uint8_t* above=&cell[((y-1)*rules+r)*window];
                        const std::uint8_t* below=&cell[(y*rules+r)*window];
                        for(unsigned o=0;o<window;++o) total+=vc[above[o]*cells+below[o]];
                    }
                    total+=horizontal[y*rules+r];
                    back[y*rules+r]=std::uint16_t(r);
                }
                return total;
            };
            // The scenery band takes one rule for all its rows, decided on the
            // band alone (ties prefer the earlier rule: the longest wrap, so a
            // shorter period is used only when it joins strictly better, such
            // as the forest's authored period). Then a Viterbi pass chooses
            // each floor row's rule after it. Predecessors are visited
            // cheapest first, so the search stops as soon as none can win or tie.
            unsigned last=0,total=never;
            if(repeat_floor) {
                // Repeating floor: the band and then the floor each take one
                // wrap for all their rows, decided on their own rows (ties:
                // the longest wrap). The first floor row scrolls in
                // perspective against the band: no fixed vertical join there.
                const unsigned from=std::min(band,rows);
                unsigned keep=0,band_cost=0,floor_cost=0;
                if(from) {
                    band_cost=never;
                    for(unsigned r=0;r<rules;++r) {
                        const unsigned c=span_cost(r,0,from,0);
                        if(c<band_cost) { band_cost=c; keep=r; }
                    }
                }
                last=keep;
                if(from<rows) {
                    floor_cost=never;
                    for(unsigned r=0;r<rules;++r) {
                        const unsigned c=span_cost(r,from,rows,0);
                        if(c<floor_cost) { floor_cost=c; last=r; }
                    }
                    if(from) back[from*rules+last]=std::uint16_t(keep);
                }
                total=std::min(never,band_cost+floor_cost);
            } else if(band) {
                unsigned keep=0;
                for(unsigned r=0;r<rules;++r) {
                    best[r]=span_cost(r,0,band,0);
                    if(best[r]<best[keep]) keep=r;
                }
                for(unsigned r=0;r<rules;++r) if(r!=keep) best[r]=never;
            } else for(unsigned r=0;r<rules;++r) best[r]=horizontal[r];
            for(unsigned y=std::max(band,1u);y<rows && !repeat_floor;++y) {
                const std::uint16_t* vc=&vcost[(y-1)*cells*cells];
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
                        // The first floor row scrolls in perspective against
                        // the band above it: no fixed vertical join there.
                        for(unsigned o=0;o<window && cost<=total && y!=band;++o)
                            cost+=vc[above[o]*cells+below[o]];
                        if(cost<total || (cost==total && q<chosen)) { total=cost; chosen=q; }
                    }
                    next[r]=total+horizontal[y*rules+r];
                    back[y*rules+r]=std::uint16_t(chosen);
                }
                best.swap(next);
            }
            if(!repeat_floor) {
                for(unsigned r=1;r<rules;++r) if(best[r]<best[last]) last=r;
                total=best[last];
            }
            SceneryRepeatSide out;
            out.cost=total; out.inset=inset; out.padding=padding;
            for(unsigned y=rows;y-->0;) {
                out.rows[y]=rule_of(last,inset_of(y));
                out.unauthored+=horizontal_bad[y*rules+last];
                if(y) last=back[y*rules+last];
            }
            // Keep the edge that replaces padding only (the authored edge when
            // there is none) unless a deeper inset scores strictly better. That
            // edge is also the native-view fallback.
            auto& chosen=right?plan.right:plan.left;
            if(inset==padding) chosen=(right?plan.right_edge:plan.left_edge)=out;
            else if(out.cost<chosen.cost) chosen=out;
        }
    }
    plan.columns=columns; plan.rows=rows; plan.reach=reach; plan.inset_rows=insets.rows;
    plan.band=band; plan.valid=true;
    return plan;
}

// Replace one side's scenery band rows [0, plan.band) with a reviewed
// sequence (cells in reading order, joined outward from the edge left after
// its inset). One that replaces no more than the side's padding needs no
// native-view guard, so the fallback side takes it too; a deeper one keeps
// the planner's fallback for frames that show a replaced column. An invalid
// sequence leaves the plan unchanged and returns false.
inline bool scenery_repeat_apply_sequence(SceneryRepeatPlan& plan,bool right,
                                          const SceneryRepeatSequence& s) {
    if(!plan.valid || !plan.band || !s.length || s.length>SceneryRepeatSequence::max_cells ||
       s.inset>SceneryRepeatPlan::max_inset || s.inset+2>plan.columns) return false;
    for(unsigned i=0;i<s.length;++i)
        if(unsigned(s.cells[i]&~SceneryRepeatSequence::flip)>=plan.columns) return false;
    auto apply=[&](SceneryRepeatSide& side) {
        side.sequence=s; side.inset=s.inset;
        for(unsigned y=0;y<plan.band;++y)
            side.rows[y]=SceneryRepeatRule{0,std::uint8_t(s.length),std::uint8_t(s.inset),1};
    };
    auto& side=right?plan.right:plan.left;
    const bool guarded=s.inset>side.padding;
    apply(side);
    if(!guarded) apply(right?plan.right_edge:plan.left_edge);
    return true;
}

// Plan a quilted reflection for one side's band rows from the layer's
// entries and tile graphics (the same map the planner was given). The run is
// drawn from columns [run_first, run_last] and must cover `margin` pixels
// past the edge on its own; the cut lies within `overlap` (< margin) pixels
// of the edge.
// Each candidate run position is scored by its cheapest cut, found per pixel
// line with steps of at most one pixel between lines. The error of a pixel
// pair is the squared palette-index distance within one bank (such banks are
// brightness ramps and may cycle, so the cut does not depend on the palette
// phase when the plan is built); transparency or another bank counts as the
// largest step. Each reflected pixel also costs a little, so the reflection
// stays as narrow as the art allows. Returns false (plan unchanged) when
// nothing can be planned.
inline bool scenery_repeat_apply_quilt(SceneryRepeatPlan& plan,bool right,const std::uint16_t* entries,
                                       const SceneryTileArt& art,unsigned run_first,unsigned run_last,
                                       unsigned overlap,unsigned margin=72) {
    using namespace scenery_repeat_detail;
    if(!plan.valid || !plan.band || !entries || !art.valid() || run_first>run_last ||
       run_last>=plan.columns || !overlap || overlap>255 || overlap>=margin) return false;
    auto& side=right?plan.right:plan.left;
    const int width=int(plan.columns)*8,inset=int(side.rows[0].inset)*8;
    const int lo=inset,hi=width-inset;
    const unsigned lines=std::min(plan.band*8,SceneryRepeatQuilt::max_lines);
    const int run_lo=int(run_first)*8,run_hi=int(run_last+1)*8;
    if(int(overlap)>=hi-lo || int(margin)>run_hi-run_lo) return false;
    // Palette index of a source pixel: bank<<4 | index, 0 when transparent.
    auto index=[&](int y,int x)->unsigned {
        const std::uint16_t e=entries[unsigned(y/8)*plan.columns+unsigned(x/8)];
        const unsigned tile=e&0x3ffu,sx=(e&hflip)?7-unsigned(x%8):unsigned(x%8);
        const unsigned sy=(e&vflip)?7-unsigned(y%8):unsigned(y%8);
        const std::size_t at=std::size_t(tile)*32+sy*4+sx/2;
        if(at>=art.size) return 0;
        const unsigned i=(art.chars[at]>>((sx&1)*4))&15u;
        return i ? ((e>>12)<<4)|i : 0u;
    };
    auto error=[](unsigned a,unsigned b)->unsigned {
        if(a==b) return 0;
        if(!(a&15) || !(b&15) || (a>>4)!=(b>>4)) return 225;
        const int d=int(a&15)-int(b&15);
        return unsigned(d*d);
    };
    // B: the reflection; A: the run placed at `source`.
    auto reflected=[&](int y,int d) { return index(y,right ? hi-1-d : lo+d); };
    const int first=right ? run_lo : run_lo+int(margin)-1;
    const int last=right ? run_hi-int(margin) : run_hi-1;
    const unsigned states=overlap+1;
    std::vector<unsigned> cost(states),next(states),edge_b(lines*(states+1)),best_cut;
    std::vector<std::uint8_t> back(lines*states);
    for(unsigned y=0;y<lines;++y) for(unsigned d=0;d<=states;++d) edge_b[y*(states+1)+d]=reflected(int(y),int(d));
    unsigned best=~0u; int chosen=-1;
    for(int source=first;source<=last;++source) {
        auto run=[&](int y,int d) { return index(y,right ? source+d : source-d); };
        auto pair=[&](unsigned y,unsigned c) {
            unsigned v=20*error(run(int(y),int(c)),edge_b[y*(states+1)+c]);
            if(c) v+=20*error(run(int(y),int(c)-1),edge_b[y*(states+1)+c-1]);
            return v+c;
        };
        for(unsigned c=0;c<states;++c) cost[c]=pair(0,c);
        for(unsigned y=1;y<lines;++y) {
            for(unsigned c=0;c<states;++c) {
                unsigned from=c,low=cost[c];
                if(c && cost[c-1]<low) { low=cost[c-1]; from=c-1; }
                if(c+1<states && cost[c+1]<low) { low=cost[c+1]; from=c+1; }
                next[c]=low+pair(y,c); back[y*states+c]=std::uint8_t(from);
            }
            cost.swap(next);
        }
        unsigned end=0;
        for(unsigned c=1;c<states;++c) if(cost[c]<cost[end]) end=c;
        if(cost[end]>=best) continue;
        best=cost[end]; chosen=source;
        best_cut.assign(lines,0);
        for(unsigned y=lines;y-->0;) { best_cut[y]=end; if(y) end=back[y*states+end]; }
    }
    if(chosen<0) return false;
    auto apply=[&](SceneryRepeatSide& s) {
        s.quilt.source=chosen; s.quilt.run_lo=unsigned(run_lo); s.quilt.run_hi=unsigned(run_hi);
        s.quilt.lines=lines;
        for(unsigned y=0;y<lines;++y) s.quilt.cut[y]=std::uint8_t(best_cut[y]);
        for(unsigned y=0;y<plan.band;++y) { s.rows[y].quilt=1; s.rows[y].sequence=0; }
    };
    apply(side);
    if(side.rows[0].inset<=side.padding) apply(right?plan.right_edge:plan.left_edge);
    return true;
}

// Identity of a planned map's entries, for diagnostic traces.
inline std::uint32_t scenery_repeat_hash(const std::uint16_t* entries,unsigned count) {
    std::uint32_t h=2166136261u;
    for(unsigned n=0;n<count;++n) { h^=entries[n]; h*=16777619u; }
    return h;
}
} // namespace swordcraft3
