#pragma once

#include <cstdint>

namespace swordcraft3 {

// Completed-frame pixel boundaries, not the VCOUNT trigger values. Reviewed
// captures have separators at 16..18 and 125..127, with scenery at 19..124.
inline constexpr unsigned kBattleHudTopEnd = 19;
inline constexpr unsigned kBattleHudBottomStart = 125;

// Presentation only: authentic center and gameplay band are NEVER written.
// Caller must first authenticate a battle and a full composite capture.
// Normal and START-paused top HUD heights are authenticated from native pixels.
// Returns false without writing anything if the native HUD no longer matches.
bool extend_battle_hud_borders(std::uint8_t* rgb, unsigned width,
                              unsigned height, unsigned extra_left,
                              unsigned extra_right);

}  // namespace swordcraft3
