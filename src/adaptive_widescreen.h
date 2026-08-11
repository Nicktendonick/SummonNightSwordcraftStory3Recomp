#pragma once

namespace gbarecomp {
struct RunOptions;
}

// Opt this game into the shared adaptive-view renderer and install the
// Swordcraft-specific scene policy. Native 240x160 remains the default.
void configure_swordcraft3_adaptive_widescreen(gbarecomp::RunOptions& opts);
