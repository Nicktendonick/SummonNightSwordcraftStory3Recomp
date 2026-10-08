#include "battle_follow_camera.h"
#include "battle_scenery_edges.h"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#define CHECK(x) do { if(!(x)) throw std::runtime_error("Failed: " #x); } while(0)

int main(int argc,char** argv) {
    using namespace swordcraft3;
    unsigned cases=0;
    for(unsigned width:{284u,384u}) {
        const auto objects=battle_follow_object_range(width);
        CHECK(objects.enabled && objects.right_inclusive-objects.left_exclusive<=512);
        std::array<bool,512> seen{};
        for(int x=objects.left_exclusive+1;x<=objects.right_inclusive;++x) {
            CHECK(!seen[x&511]); seen[x&511]=true;
            CHECK(battle_follow_object_x(unsigned(x),width)==x);
        }
        // Every admitted flat-band key and every possible captured extremum
        // inside its envelope. Coordinates only, never framebuffer checks.
        for(int flat=0;flat<=141;++flat) {
          const auto e=battle_follow_envelope(flat);
          if(!e.valid) { CHECK(!battle_follow_framing(width,flat,flat,flat).active); continue; }
          CHECK(e.minimum>=3 && e.maximum<=141 && e.maximum-e.minimum<=29);
          unsigned stable_anchor=512;
          for(int lo=e.minimum;lo<=flat;++lo) for(int hi=flat;hi<=e.maximum;++hi) {
            const auto result=battle_follow_framing(width,lo,hi,flat);
            const auto& f=result.framing;
            CHECK(result.active && f.bounded && f.object_wrap==objects.wrap_begin);
            CHECK(f.begin==(width==384?19u:0u));
            CHECK(f.end==width-f.begin);
            CHECK(f.anchor>=f.begin && f.anchor+240<=f.end);
            CHECK(int(f.begin)-int(f.anchor)+lo>=0);
            CHECK(int(f.end)-1-int(f.anchor)+hi<384);
            const int low=std::max(int(f.begin),int(f.end)+e.maximum-384);
            const int high=std::min(int(f.end)-240,int(f.begin)+e.minimum);
            CHECK(int(f.anchor)==std::clamp(int((width-240)/2),low,high));
            if(stable_anchor==512) stable_anchor=f.anchor;
            CHECK(f.anchor==stable_anchor); // jump extrema must never move it
            const auto cover=battle_scenery_cover(result,width,3,true);
            CHECK(cover.anchor==f.anchor && cover.origin==f.origin && cover.object_wrap==f.object_wrap);
            CHECK(cover.begin>=f.begin && cover.begin<=f.anchor);
            CHECK(cover.end<=f.end && cover.end>=f.anchor+240);
            CHECK(cover.end-cover.begin>=240);
            const auto stable_cover=battle_scenery_cover(
                battle_follow_framing(width,e.minimum,e.maximum,flat),width,3,true);
            CHECK(cover.begin==stable_cover.begin && cover.end==stable_cover.end);
            for(unsigned arena:{0u,1u,2u,4u,7u,255u}) {
                const auto other=battle_scenery_cover(result,width,arena,true);
                CHECK(other.begin==f.begin && other.end==f.end && other.anchor==f.anchor);
            }
            const auto off_cover=battle_scenery_cover(result,width,3,false);
            CHECK(off_cover.begin==f.begin && off_cover.end==f.end);
            // Entire visible origin interval of each guest-cull-sized OBJ fits
            // the constant submitted/decoded interval, including both edges.
            CHECK(int(f.begin)-int(f.anchor)-63>objects.left_exclusive);
            CHECK(int(f.end)-int(f.anchor)-1<=objects.right_inclusive);
            ++cases;
          }
          for(const auto pair:{std::pair{e.minimum-1,e.maximum},
                               std::pair{e.minimum,e.maximum+1},std::pair{flat+1,flat}}) {
            const auto r=battle_follow_framing(width,pair.first,pair.second,flat);
            CHECK(!r.active && !r.framing.bounded);
            const auto rejected=battle_scenery_cover(r,width,3,true);
            CHECK(!rejected.bounded && rejected.begin==r.framing.begin && rejected.end==r.framing.end);
            CHECK(r.framing.object_wrap==objects.wrap_begin);
          }
        }
        // Current fallback and native 240 remain available in the same OAM
        // interval; no stale prior-frame anchor is used to decode coordinates.
        const int centered=int((width-240)/2);
        CHECK(-centered-63>objects.left_exclusive);
        CHECK(int(width)-centered-1<=objects.right_inclusive);
        for(int flat:{-512,-1,0,23,121,142,512}) {
            const auto r=battle_follow_framing(width,72,72,flat);
            CHECK(!r.active && !r.framing.bounded && int(r.framing.anchor)==centered);
            CHECK(r.framing.object_wrap==objects.wrap_begin);
        }
        const auto off=battle_follow_framing(width,72,72,72,false);
        CHECK(!off.active && off.framing.object_wrap==0);
    }
    // Independent arithmetic transcription of authenticated sub_080352F4's
    // normal schedule; include the final table row as a conservative envelope.
    // Independent full-row enumeration checks the production endpoint envelope.
    // Production reads owned row IO; it never writes reconstructed row offsets.
    const auto asr6=[](int x) { return x>=0?x/64:-((-x+63)/64); };
    for(int d:{128,136}) for(int c=0;c<=128;++c) for(int v=0;v<=32;++v) {
        int lo=512,hi=-512;
        for(int r=18;r<=124;++r) {
            const int term=r<d-v?d-152:r-(151-v);
            const int h=c+8+asr6(term*(c-64));
            lo=std::min(lo,h); hi=std::max(hi,h);
        }
        CHECK(lo>=3 && hi<=141 && hi-lo<=29);
        const int flat=c+8+asr6((d-152)*(c-64));
        const auto e=battle_follow_envelope(flat);
        CHECK(e.valid && e.minimum<=lo && e.maximum>=hi);
        for(unsigned width:{284u,384u}) {
            const auto result=battle_follow_framing(width,lo,hi,flat);
            CHECK(result.active);
            CHECK(result.framing.anchor==battle_follow_framing(width,e.minimum,e.maximum,flat).framing.anchor);
        }
    }
    for(unsigned width:{0u,240u,256u,320u,385u}) {
        CHECK(!battle_follow_framing(width,72,72,72).active);
        CHECK(!battle_follow_object_range(width).enabled);
    }
    for(int d:{128,136}) for(unsigned width:{284u,384u}) {
        unsigned previous=0;
        for(int c=0;c<=128;++c) {
            const int flat=c+8+asr6((d-152)*(c-64));
            const auto e=battle_follow_envelope(flat);
            const auto result=battle_follow_framing(width,e.minimum,e.maximum,flat);
            CHECK(result.active && result.framing.anchor>=previous);
            previous=result.framing.anchor;
        }
    }
    CHECK(battle_follow_framing(384,72,72,72).framing.anchor==72);
    const auto middle_cover=battle_scenery_cover(battle_follow_framing(384,72,72,72),384,3,true);
    CHECK(middle_cover.begin==32 && middle_cover.end==352 && middle_cover.anchor==72);
    CHECK(battle_follow_framing(384,3,32,32).framing.anchor==22);
    CHECK(battle_follow_framing(384,120,141,120).framing.anchor==122);
    CHECK(!battle_follow_rom_supported(nullptr,0));
    for(int n=1;n<argc;++n) {
        std::ifstream input(argv[n],std::ios::binary);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
        CHECK(battle_follow_rom_supported(bytes.data(),bytes.size()));
        bytes[0xc3154c]^=1;
        CHECK(!battle_follow_rom_supported(bytes.data(),bytes.size()));
    }
    std::cout<<"PASS: "<<cases<<" source-bound cases, stable frames, edge clamps, OAM union/wrap, Current/native fallback, ROM guards\n";
}
