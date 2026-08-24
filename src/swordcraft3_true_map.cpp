#include "swordcraft3_true_map.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "gba_ppu.h"
#include "runtime.h"

namespace {

constexpr std::uint32_t kEwramBase = 0x02000000u;
constexpr std::uint32_t kIwramBase = 0x03000000u;
constexpr std::uint32_t kRomBase = 0x08000000u;

// gUnk_03002A20 is the game's four-entry background descriptor array. Each
// 0x34-byte entry is consumed by sub_08004EB8/sub_08005114, which copy from
// the complete tilemap at +0x1C into the 32x32 hardware ring at +0x20.
constexpr std::uint32_t kBgStateAddress = 0x03002A20u;
constexpr std::size_t kBgStateStride = 0x34u;
constexpr std::size_t kWidthOffset = 0x04u;
constexpr std::size_t kHeightOffset = 0x06u;
constexpr std::size_t kScrollXOffset = 0x08u;
constexpr std::size_t kScrollYOffset = 0x0Au;
constexpr std::size_t kSourceMapOffset = 0x1Cu;
constexpr std::size_t kRingMapOffset = 0x20u;
constexpr std::size_t kPaletteBankOffset = 0x19u;
constexpr std::size_t kTileBaseOffset = 0x1Au;

struct GuestMemory {
    const std::uint8_t* ewram = nullptr;
    std::size_t ewram_size = 0;
    const std::uint8_t* iwram = nullptr;
    std::size_t iwram_size = 0;
    const std::uint8_t* rom = nullptr;
    std::size_t rom_size = 0;
};

struct LayerState {
    bool valid = false;
    std::uint16_t width_px = 0;
    std::uint16_t height_px = 0;
    std::uint16_t scroll_x = 0;
    std::uint16_t scroll_y = 0;
    std::uint16_t entry_bias = 0;
    std::uint32_t source_map = 0;
};

GuestMemory s_memory{};
std::array<LayerState, 4> s_layers{};
unsigned s_layer_mask = 0;

const std::uint8_t* guest_bytes(std::uint32_t address, std::size_t bytes) {
    auto within = [&](std::uint32_t base, const std::uint8_t* data,
                      std::size_t size) -> const std::uint8_t* {
        if (!data || address < base) return nullptr;
        const std::size_t offset = static_cast<std::size_t>(address - base);
        if (offset > size || bytes > size - offset) return nullptr;
        return data + offset;
    };

    if (const auto* p = within(kEwramBase, s_memory.ewram,
                               s_memory.ewram_size)) return p;
    if (const auto* p = within(kIwramBase, s_memory.iwram,
                               s_memory.iwram_size)) return p;

    // The cartridge is mirrored at 0x08000000/0x0A000000/0x0C000000.
    const std::uint32_t region = address & 0x0E000000u;
    if (region == 0x08000000u || region == 0x0A000000u ||
        region == 0x0C000000u) {
        const std::uint32_t canonical = kRomBase | (address & 0x01FFFFFFu);
        const std::size_t offset =
            static_cast<std::size_t>(canonical - kRomBase);
        if (!s_memory.rom || offset > s_memory.rom_size ||
            bytes > s_memory.rom_size - offset) return nullptr;
        return s_memory.rom + offset;
    }
    return nullptr;
}

std::uint16_t load16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(
        p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

std::uint32_t load32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
        (static_cast<std::uint32_t>(p[1]) << 8) |
        (static_cast<std::uint32_t>(p[2]) << 16) |
        (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace

void swordcraft3_true_map_update(
    const gbarecomp::ExtendedViewFrameInfo* frame) {
    s_layer_mask = 0;
    s_layers = {};
    s_memory = {};
    if (!frame) return;

    s_memory = {frame->ewram, frame->ewram_size,
                frame->iwram, frame->iwram_size,
                frame->rom, frame->rom_size};
    const auto* states = guest_bytes(kBgStateAddress,
                                     kBgStateStride * s_layers.size());
    if (!states) return;

    for (unsigned bg = 0; bg < s_layers.size(); ++bg) {
        const auto* state = states + bg * kBgStateStride;
        LayerState layer{};
        layer.width_px = load16(state + kWidthOffset);
        layer.height_px = load16(state + kHeightOffset);
        layer.scroll_x = load16(state + kScrollXOffset);
        layer.scroll_y = load16(state + kScrollYOffset);
        layer.source_map = load32(state + kSourceMapOffset);
        const std::uint32_t ring_map = load32(state + kRingMapOffset);
        layer.entry_bias = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(state[kPaletteBankOffset]) << 12) +
            load16(state + kTileBaseOffset));

        const std::uint32_t width_tiles = layer.width_px >> 3;
        const std::uint32_t height_tiles = layer.height_px >> 3;
        const std::uint64_t map_bytes =
            static_cast<std::uint64_t>(width_tiles) * height_tiles * 2u;
        layer.valid = width_tiles > 30u && height_tiles >= 20u &&
            map_bytes <= static_cast<std::uint64_t>(SIZE_MAX) &&
            guest_bytes(layer.source_map,
                        static_cast<std::size_t>(map_bytes)) != nullptr &&
            guest_bytes(ring_map, 32u * 32u * 2u) != nullptr;
        s_layers[bg] = layer;
        if (layer.valid && layer.width_px > 240u)
            s_layer_mask |= 1u << bg;
    }
}

unsigned swordcraft3_true_map_layers() {
    return s_layer_mask;
}

int swordcraft3_true_map_tilemap(int bg, int hw_x, int screen_y,
                                 std::uint16_t* out_entry) {
    if (!out_entry || bg < 0 || bg >= static_cast<int>(s_layers.size()))
        return gba::kWsTilemapUnavailable;
    const LayerState& layer = s_layers[static_cast<unsigned>(bg)];
    if (!layer.valid) return gba::kWsTilemapUnavailable;

    const int source_x = static_cast<int>(layer.scroll_x) + hw_x;
    const int source_y = static_cast<int>(layer.scroll_y) + screen_y;
    if (source_x < 0 || source_y < 0 ||
        source_x >= static_cast<int>(layer.width_px) ||
        source_y >= static_cast<int>(layer.height_px)) {
        return gba::kWsTilemapUnavailable;
    }

    const std::uint32_t width_tiles = layer.width_px >> 3;
    const std::uint32_t tile_x = static_cast<std::uint32_t>(source_x) >> 3;
    const std::uint32_t tile_y = static_cast<std::uint32_t>(source_y) >> 3;
    const std::uint32_t entry_address = layer.source_map +
        (tile_y * width_tiles + tile_x) * 2u;
    const auto* entry = guest_bytes(entry_address, 2);
    if (!entry) return gba::kWsTilemapUnavailable;

    *out_entry = static_cast<std::uint16_t>(
        load16(entry) + layer.entry_bias);
    return gba::kWsTilemapReplace;
}
