#include "custom_battle_spell_window.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

static unsigned checks=0;
static void check(bool v) { ++checks; if(!v) throw std::runtime_error("spell window state assertion"); }
static void put16(std::uint8_t* p,unsigned v) { p[0]=v; p[1]=v>>8; }
static void put32(std::uint8_t* p,std::uint32_t v) { put16(p,v); put16(p+2,v>>16); }
int main() {
    try {
        std::array<std::uint8_t,0x8000> iwram{};
        std::vector<std::uint8_t> ewram(0x40000);
        std::array<std::uint8_t,0x400> io{};
        auto* d=ewram.data()+0x323c;
        auto read=[&] { return swordcraft3::read_battle_spell_window(
            iwram.data(),iwram.size(),ewram.data(),ewram.size(),io.data(),io.size()); };
        auto setup=[&](unsigned cnt,int x,int y,unsigned w,unsigned h,unsigned orient=1) {
            iwram.fill(0); std::fill(ewram.begin(),ewram.end(),0); io.fill(0);
            put32(iwram.data()+0x6ac0,0x03000000); put32(iwram.data()+0x1a90,1);
            put32(iwram.data()+0x1e44,0x02003200);
            iwram[0x44e]=2; iwram[0x44f]=1; iwram[0x0c]=2; iwram[0x0e]=1;
            put32(ewram.data()+0x3238,0x030008c0);
            d[0]=1; d[1]=orient; put16(d+2,unsigned(x)); put16(d+4,unsigned(y));
            put16(d+0x0e,w); put16(d+0x10,h);
            put16(iwram.data()+0x2a8c,cnt==0x4305 ? 512 : 256);
            put16(iwram.data()+0x2a8e,256);
            put16(iwram.data()+0x2990,0x3740);
            put16(io.data(),0x3740); put16(io.data()+0x0c,cnt);
            put16(io.data()+0x18,unsigned(x)&511); put16(io.data()+0x1a,unsigned(y)&511);
            put16(io.data()+0x48,0x3f37); put16(io.data()+0x4a,0x5513);
            put16(io.data()+0x50,0x1344); put16(io.data()+0x52,0x1010);
        };
        auto expect=[&](int l,int r,int t,int b) {
            const auto a=read(); check(a.valid && a.kind==2);
            check(a.left==l && a.right==r && a.top==t && a.bottom==b);
        };
        // Exact descriptor/register numbers from the reported edge capture.
        setup(0x4305,443,216,512,256);
        put16(io.data()+0x40,0x0045); put16(io.data()+0x44,0x28ff);
        expect(-444,69,40,296);
        for(unsigned high=0;high<256;++high) {
            put16(io.data()+0x48,(high<<8)|0x37);
            put16(io.data()+0x4a,((255-high)<<8)|0x13);
            expect(-444,69,40,296);
        }
        put16(io.data()+0x48,0x3f37); put16(io.data()+0x4a,0x5513);
        for(unsigned mode:{0u,2u,7u}) { iwram[0x0c]=mode; expect(-444,69,40,296); }
        d[0]=3; expect(-444,69,40,296); // Script map-upload flag is independent.
        d[0]=1;
        // Opposite facing uses the same signed source position, not a wrapped
        // screen-overlap heuristic; it moves the mask to the other side.
        iwram[0x8c0+0x318]=4; put16(io.data()+0x40,0x45ff);
        expect(69,581,40,296);
        d[1]=2; expect(69,581,40,296);
        put32(ewram.data()+0x3238,0x03000c50);
        iwram[0xc50+0x318]=4; expect(69,581,40,296);
        iwram[0x0e]=0; check(!read().valid); iwram[0x0e]=1;
        put32(ewram.data()+0x3238,0x03000c54); check(!read().valid);

        // Smaller 256-wide canvas and finite authored window.
        setup(0x0305,71,216,120,120);
        put16(io.data()+0x40,0x40b9); put16(io.data()+0x44,0x28a0);
        expect(64,185,40,160);
        // Position mode zero preserves signed offsets before guest clamping.
        setup(0x0305,-10,-20,100,80,0);
        put16(io.data()+0x40,0xa5ff); put16(io.data()+0x44,0x1464);
        expect(165,266,20,100);

        auto reject16=[&](std::uint8_t* p,unsigned value) {
            const unsigned saved=unsigned(p[0])|(unsigned(p[1])<<8);
            put16(p,value); check(!read().valid); put16(p,saved);
        };
        reject16(io.data(),0x3340); // BG2 disabled.
        reject16(io.data(),0x1740); // WIN0 disabled.
        reject16(io.data(),0x7740); // Unreviewed additional window.
        reject16(io.data()+0x0c,0x4385); // Affine layout is not this path.
        reject16(io.data()+0x40,0xa4ff); reject16(io.data()+0x44,0x1564);
        reject16(io.data()+0x18,501); reject16(io.data()+0x1a,491);
        reject16(io.data()+0x48,0x3f13); reject16(io.data()+0x4a,0x5537);
        reject16(io.data()+0x50,0x1345); reject16(io.data()+0x52,0x1110);
        reject16(iwram.data()+0x2a8c,512); reject16(iwram.data()+0x2a8e,128);
        reject16(d+0x0e,0); reject16(d+0x0e,257); reject16(d+0x10,257);
        d[0]=0; check(!read().valid); d[0]=5; check(!read().valid); d[0]=1;
        d[1]=3; check(!read().valid); d[1]=0;
        iwram[0x44e]=3; check(!read().valid); iwram[0x44e]=2;
        iwram[0x44f]=2; check(!read().valid); iwram[0x44f]=1;
        put32(iwram.data()+0x1e44,0x02003300); check(!read().valid);
        put32(iwram.data()+0x1e44,0x02003200);

        // Script retirement clears host-side metadata one displayed frame
        // before its pending BG2/WIN0 disable is committed. The still-owned
        // captured row retains the same signed window; no history is needed.
        setup(0x4305,443,216,512,256);
        put16(io.data()+0x40,0x0045); put16(io.data()+0x44,0x28ff);
        d[0]=0; put16(d+0x12,0x100); put16(d+0x14,0x100);
        std::fill(iwram.begin()+0x2a88,iwram.begin()+0x2aba,0);
        check(!read().valid); // Inactive without a pending display change.
        put16(iwram.data()+0x2990,0x1340);
        expect(-444,69,40,296);
        // Padding after the guest's cleared range is not part of its state.
        put16(iwram.data()+0x2aba,0x5555); expect(-444,69,40,296);
        reject16(iwram.data()+0x2990,0x3340); // Only BG2 disabled, not WIN0.
        reject16(iwram.data()+0x2990,0x1740); // Only WIN0 disabled, not BG2.
        reject16(iwram.data()+0x2990,0x0340); // Unrelated display change too.
        reject16(iwram.data()+0x2a8c,512); // Metadata was not retired.
        reject16(iwram.data()+0x2a88,2);
        reject16(d+0x12,0); reject16(d+0x14,0);
        for(unsigned offset:{0x16u,0x18u,0x1au,0x28u,0x2au}) reject16(d+offset,1);
        reject16(io.data()+0x40,0x0046); reject16(io.data()+0x18,442);
        d[0]=2; check(!read().valid); d[0]=0;
        put16(io.data(),0x1340); check(!read().valid); // Committed retirement.
        put16(io.data(),0x3740);
        check(!swordcraft3::read_battle_spell_window(nullptr,0,ewram.data(),ewram.size(),io.data(),io.size()).valid);
        check(!swordcraft3::read_battle_spell_window(iwram.data(),0x6ac3,ewram.data(),ewram.size(),io.data(),io.size()).valid);
        check(!swordcraft3::read_battle_spell_window(iwram.data(),iwram.size(),ewram.data(),0x3273,io.data(),io.size()).valid);
        check(!swordcraft3::read_battle_spell_window(iwram.data(),iwram.size(),ewram.data(),ewram.size(),io.data(),0x55).valid);
        check(!swordcraft3::battle_spell_window_rom_supported(nullptr,0));
        check(!swordcraft3::battle_spell_window_rom_supported(ewram.data(),ewram.size()));

        // 0803B91C owns casting windows separately from the type-2 script.
        // Its signed 120x120 rectangle comes from the casting actor at +454,
        // not the retired/stale effect descriptor at 0200323C.
        auto casting_setup=[&](unsigned kind,int x,int y) {
            setup(0x0305,184-x,344-y,256,256);
            iwram[0x44e]=kind; iwram[0x44f]=2; iwram[0x0c]=7;
            put32(iwram.data()+0x454,0x030008c0);
            put16(iwram.data()+0x450,unsigned(x));
            put16(iwram.data()+0x452,unsigned(y));
            put16(iwram.data()+0x8c0+0x19a,unsigned(x));
            put16(iwram.data()+0x8c0+0x19c,unsigned(y));
            // Deliberately reproduce the unrelated stale descriptor from the
            // reported casting snapshot. These are not this effect's bounds.
            d[0]=0; d[1]=1; put16(d+2,unsigned(-49));
            put16(d+4,unsigned(-32)); put16(d+0x0e,512); put16(d+0x10,256);
            auto clamp=[](int v) { return unsigned(std::clamp(v,0,255)); };
            put16(io.data()+0x40,(clamp(x-56)<<8)|clamp(x+64));
            put16(io.data()+0x44,(clamp(y-80)<<8)|clamp(y+40));
        };
        auto casting_expect=[&](unsigned kind,int l,int r,int t,int b) {
            const auto a=read(); check(a.valid && a.kind==kind);
            check(a.left==l && a.right==r && a.top==t && a.bottom==b);
        };
        for(unsigned kind:{3u,4u}) {
            casting_setup(kind,220,120); // Exact right-edge casting snapshot.
            check(io[0x40]==0xff && io[0x41]==0xa4);
            check(io[0x44]==0xa0 && io[0x45]==0x28);
            casting_expect(kind,164,284,40,160);
            casting_setup(kind,20,120); // Genuine left continuation, not wrap.
            casting_expect(kind,-36,84,40,160);
            casting_setup(kind,120,120); // Entire window fits native width.
            casting_expect(kind,64,184,40,160);
            casting_setup(kind,220,60); // Signed top extends above zero.
            casting_expect(kind,164,284,-20,100);
            casting_setup(kind,220,240); // Bottom exceeds hardware clamp.
            casting_expect(kind,164,284,160,280);
        }

        casting_setup(3,220,120);
        for(unsigned high=0;high<256;++high) {
            put16(io.data()+0x48,(high<<8)|0x37);
            put16(io.data()+0x4a,((255-high)<<8)|0x13);
            casting_expect(3,164,284,40,160);
        }
        put16(io.data()+0x48,0x3f37); put16(io.data()+0x4a,0x5513);
        // The actor pointer, coordinates, and matching native window must all
        // belong to this casting path. Merely having compatible BG2 art is
        // insufficient to extend another actor or effect.
        for(unsigned actor:{0u,0x020008c0u,0x030008bcu,0x030008c4u,0x03001704u}) {
            put32(iwram.data()+0x454,actor); check(!read().valid);
        }
        put32(iwram.data()+0x454,0x030008c0);
        casting_expect(3,164,284,40,160);
        put32(iwram.data()+0x454,0x03000c50);
        put16(iwram.data()+0xc50+0x19a,220); put16(iwram.data()+0xc50+0x19c,120);
        casting_expect(3,164,284,40,160);
        iwram[0x0e]=0; check(!read().valid); iwram[0x0e]=1;
        iwram[0x0e]=5; check(!read().valid); iwram[0x0e]=1;
        put32(iwram.data()+0x454,0x030008c0);

        // The casting window also controls OBJ/effects. It remains owned when
        // BG2 turns off; returning bounds must not turn that layer back on.
        put16(io.data(),0x3340);
        casting_expect(3,164,284,40,160);
        check((unsigned(io[0])|(unsigned(io[1])<<8))==0x3340);
        put16(io.data(),0x3740);
        reject16(io.data(),0x1740); // No casting WIN0 ownership.
        reject16(io.data(),0x37c0); // Forced blank.
        reject16(io.data(),0x3741); // Affine/critical path is unrelated.
        reject16(io.data(),0x7740); // Unknown extra coordinate window.
        reject16(io.data(),0xb740); // Unknown OBJ-window interaction.
        reject16(io.data()+0x0c,0x4385); // Wrong canvas format.
        reject16(io.data()+0x0c,0x4305); // Casting uses the reviewed 256-wide map.
        reject16(io.data()+0x40,0xa3ff); // Native mask must agree exactly.
        reject16(io.data()+0x44,0x27a0);
        reject16(io.data()+0x18,477); reject16(io.data()+0x1a,223);
        reject16(iwram.data()+0x450,221); reject16(iwram.data()+0x452,121);
        // Source retirement can precede display by a row/frame. Casting width
        // is authenticated by the captured CNT, not this retired metadata.
        std::fill(iwram.begin()+0x2a88,iwram.begin()+0x2aba,0);
        put16(iwram.data()+0x2990,0x3340);
        casting_expect(3,164,284,40,160); // Captured BG2 is still enabled.
        check((unsigned(io[0])|(unsigned(io[1])<<8))==0x3740);
        put16(iwram.data()+0x2990,0x3740);
        reject16(io.data()+0x48,0x3f13); reject16(io.data()+0x4a,0x5537);
        reject16(io.data()+0x50,0x1345); // Different layer/effect operation.
        reject16(io.data()+0x52,0x1110); reject16(io.data()+0x52,0x1011);
        reject16(io.data()+0x52,0x3010); // Reserved alpha bits cannot authorize.
        for(unsigned alpha:{0u,0x1000u,0x0010u,0x0808u,0x1010u}) {
            put16(io.data()+0x52,alpha); casting_expect(3,164,284,40,160);
        }
        put16(io.data()+0x52,0x1010);
        for(unsigned kind:{0u,1u,5u,6u,255u}) {
            iwram[0x44e]=kind; check(!read().valid);
        }
        iwram[0x44e]=3;
        // No stage/presentation heuristic or stale script allocation decides
        // this window: the guest routine's kind, actor, scroll, and mask do.
        for(unsigned stage:{0u,1u,2u,11u}) {
            iwram[0x44f]=stage; casting_expect(3,164,284,40,160);
        }
        std::fill(ewram.begin(),ewram.end(),0xa5);
        put32(iwram.data()+0x1e44,0xffffffff);
        casting_expect(3,164,284,40,160);
        // Signed16 overflow is not a host window stretching across the map.
        casting_setup(3,-32768,120); check(!read().valid);
        casting_setup(3,32767,120); check(!read().valid);
        casting_setup(3,220,-32768); check(!read().valid);
        casting_setup(3,220,32767); check(!read().valid);

        // Actual second capture: casting substate11 has disabled BG2 and
        // retired source metadata, but its window still governs live OBJ.
        casting_setup(3,220,120);
        iwram[0x44f]=11; put16(io.data(),0x3340);
        put16(iwram.data()+0x2990,0x3340);
        std::fill(iwram.begin()+0x2a88,iwram.begin()+0x2aba,0);
        casting_expect(3,164,284,40,160);
        check((unsigned(io[0])|(unsigned(io[1])<<8))==0x3340);
        // Once the guest disables WIN0 or retires this kind, stop extending.
        put16(io.data(),0x1340); check(!read().valid);
        put16(io.data(),0x3340); iwram[0x44e]=0;
        check(!read().valid);
        // The following spell owns kind2/state0 before the old hardware WIN0
        // disable is applied. Do not carry the retired casting rectangle into
        // its new descriptor just because the actor/scroll registers still fit.
        iwram[0x44e]=2; iwram[0x44f]=0;
        put16(iwram.data()+0x2990,0x1340);
        check(!read().valid);

        // Display publication owns the metadata; subsequent guest updates
        // must not invalidate an unchanged raster or select another epoch.
        swordcraft3::BattleSpellDisplayEpoch epoch;
        auto publish=[&] { epoch.publish(iwram.data(),iwram.size(),ewram.data(),ewram.size()); };
        auto displayed=[&] { return epoch.read(iwram.data(),iwram.size(),ewram.data(),ewram.size(),io.data(),io.size()); };
        for(int step:{-1,1,17,-17}) {
            setup(0x4305,312,216,512,256);
            iwram[0x8c0+0x318]=4;
            put16(io.data()+0x40,0xc8ff); put16(io.data()+0x44,0x28ff);
            publish();
            put16(d+2,312+step);
            check(!read().valid);
            check(displayed().valid && displayed().left==200 && displayed().right==712);
            // Register changes without matching published metadata fail.
            put16(io.data()+0x18,312+step);
            put16(io.data()+0x40,swordcraft3::battle_spell_detail::clamped_window(200-step,712-step));
            check(!displayed().valid);
            publish(); check(displayed().valid && displayed().left==200-step);
            // No last-good state across ownership changes or restoration.
            iwram[0x44e]=0; publish(); check(!displayed().valid);
            iwram[0x44e]=2; publish(); check(displayed().valid);
            put16(d+2,312+step+1); epoch.reset(); check(!displayed().valid);
        }
        casting_setup(3,220,120); publish();
        put16(iwram.data()+0x8c0+0x19a,221);
        check(!read().valid); check(displayed().valid && displayed().left==164);
        epoch.publish(nullptr,0,ewram.data(),ewram.size());
        check(!displayed().valid);
        std::printf("script and casting spell signed windows: %u state assertions PASS (no pixels)\n",checks);
        return 0;
    } catch(const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
