#pragma once
namespace gbarecomp { struct RunOptions; }
void configure_swordcraft3_custom_renderer(gbarecomp::RunOptions&);
// Call after preboot, before run_game installs hooks. Never changes guest RAM.
void set_swordcraft3_select_guard(bool enabled);
// 0: Current; 1: Bounded; 2: Follow + edge stops (experimental).
void set_swordcraft3_battle_camera_mode(int mode);
void set_swordcraft3_battle_edge_cover(bool enabled);
