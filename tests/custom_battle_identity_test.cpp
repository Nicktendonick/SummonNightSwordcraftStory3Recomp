#include "custom_battle_identity.h"
#include <array>
#include <cstdio>
#include <cstdlib>
static void check(bool ok) { if(!ok) { std::fputs("battle identity test failed\n",stderr); std::exit(1); } }
int main() {
    using swordcraft3::BattleArena;
    using swordcraft3::battle_arena_identity;
    constexpr auto forest_near="7f0309c30b95187d7b409e0a5ba7f73b2431694b";
    constexpr auto forest_far="c25d865a7ad8e6c20d9804f6c2aacca32ff09a70";
    constexpr auto rocky_near="a8ce0c8aa57c09f164966847722e2d9c627ac29d";
    constexpr auto rocky_far="71979aeb549c54f05620f4fda638614f28684d60";
    check(battle_arena_identity(forest_near,forest_far)==BattleArena::forest);
    check(battle_arena_identity(rocky_near,rocky_far)==BattleArena::manig_rocks);
    check(battle_arena_identity(forest_near,rocky_far)==BattleArena::unknown);
    check(battle_arena_identity(rocky_near,forest_far)==BattleArena::unknown);
    check(battle_arena_identity("",rocky_far)==BattleArena::unknown);
    check(battle_arena_identity(rocky_near,"")==BattleArena::unknown);
    std::array<std::uint8_t,0x4800> original{},changed{};
    for(unsigned i=0;i<original.size();++i) original[i]=std::uint8_t(i*37+(i>>8));
    changed=original;
    const auto identity=swordcraft3::battle_near_identity(original.data(),original.size());
    // Arbitrary new item/weapon/ability text, not just the captured Guard bytes.
    for(unsigned i=0x3800;i<0x4800;++i)
        if(!((i>=0x3800 && i<0x3d00) || (i>=0x4000 && i<0x4500))) changed[i]^=0xff;
    check(identity==swordcraft3::battle_near_identity(changed.data(),changed.size()));
    check(swordcraft3::battle_scenery_rows_equal(original.data(),changed.data(),changed.size()));
    for(unsigned offset:{0x3800u,0x3980u,0x3cffu,0x4000u,0x4180u,0x44ffu}) {
        changed=original; changed[offset]^=1;
        check(identity!=swordcraft3::battle_near_identity(changed.data(),changed.size()));
        check(!swordcraft3::battle_scenery_rows_equal(original.data(),changed.data(),changed.size()));
    }
    changed=original; changed[0x2800]^=1;
    check(!swordcraft3::battle_scenery_rows_equal(original.data(),changed.data(),changed.size()));
    check(swordcraft3::battle_near_identity(nullptr,0).empty());
    check(swordcraft3::battle_near_identity(original.data(),0x44ff).empty());
    check(!swordcraft3::battle_scenery_rows_equal(nullptr,changed.data(),changed.size()));
    check(!swordcraft3::battle_scenery_rows_equal(original.data(),changed.data(),0x44ff));
    check(swordcraft3::battle_near_row_reviewed(19,32));
    check(swordcraft3::battle_near_row_reviewed(124,32));
    check(swordcraft3::battle_near_row_reviewed(127,32));
    check(!swordcraft3::battle_near_row_reviewed(128,32));
    check(swordcraft3::battle_near_row_reviewed(15,32));
    check(swordcraft3::battle_near_row_reviewed(19,14));
    check(swordcraft3::battle_near_row_reviewed(0,0));
    check(!swordcraft3::battle_near_row_reviewed(19,512-20));
    check(swordcraft3::battle_near_row_reviewed(59,512-8));
    std::puts("battle scenery identity/HUD isolation: PASS");
}
