#pragma once

#include <cstdint>

namespace gbarecomp {
struct ExtendedViewFrameInfo;
}

// Snapshot Swordcraft 3's game-owned background-map descriptors. The guest
// keeps a complete source map beside the 32x32 hardware ring; this adapter
// exposes that original map to the shared wide PPU without modifying guest
// RAM or VRAM.
void swordcraft3_true_map_update(
    const gbarecomp::ExtendedViewFrameInfo* frame);

// Hardware BG bitmask whose complete source map is valid and horizontally
// wider than the native viewport for the current frame.
unsigned swordcraft3_true_map_layers();

// gba::g_ws_tilemap_provider-compatible callback.
int swordcraft3_true_map_tilemap(int bg, int hw_x, int screen_y,
                                 std::uint16_t* out_entry);
