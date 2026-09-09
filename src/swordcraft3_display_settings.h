#pragma once

namespace gbarecomp { struct RunOptions; }

// Host preference, deliberately separate from serialized guest/save state.
void configure_swordcraft3_display_settings(gbarecomp::RunOptions& opts,
                                          const char* executable_path);
bool swordcraft3_battle_hud_borders_enabled();
