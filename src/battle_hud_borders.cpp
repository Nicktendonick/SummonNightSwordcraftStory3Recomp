#include "battle_hud_borders.h"

#include <array>
#include <cstddef>
#include <cstring>

namespace swordcraft3 {
namespace {
using Color = std::array<std::uint8_t, 3>;
constexpr unsigned kNativeWidth = 240;
constexpr unsigned kHeight = 160;
}

bool extend_battle_hud_borders(std::uint8_t* rgb, unsigned width,
                              unsigned height, unsigned extra_left,
                              unsigned extra_right) {
    if (!rgb || height != kHeight || width <= kNativeWidth || width > 384 ||
        extra_left > width - kNativeWidth ||
        extra_right != width - kNativeWidth - extra_left) {
        return false;
    }
    auto pixel = [&](unsigned x, unsigned y) {
        const auto* p = rgb + (static_cast<std::size_t>(y) * width +
                               extra_left + x) * 3u;
        return Color{p[0], p[1], p[2]};
    };
    auto uniform = [&](unsigned y, unsigned begin, unsigned end, Color color) {
        for (unsigned x = begin; x < end; ++x)
            if (pixel(x, y) != color) return false;
        return true;
    };
    std::array<Color, kHeight> colors{};

    // The narrow blank gutters are outside HP/weapon bars and text. Sampling
    // the row's *composed* pixels derives the actual HUD palette color after
    // scanline blending/brightness. No fixed RGB, previous-frame color, or
    // "most common color" guess (the HP bars often dominate that statistic).
    // START expands the top HUD by 40 rows (separator 56..58). Recognize
    // either completed layout, never infer a HUD from a single tan row.
    unsigned top_end = 0;
    for (unsigned candidate : {kBattleHudTopEnd, 59u}) {
        bool matches = true;
        for (unsigned y = 0; y < candidate - 3; ++y) {
            colors[y] = pixel(8, y);
            matches &= uniform(y, 8, 13, colors[y]) &&
                       uniform(y, 225, 232, colors[y]);
        }
        for (unsigned y = candidate - 3; y < candidate; ++y) {
            colors[y] = pixel(8, y);
            matches &= uniform(y, 8, 232, colors[y]);
        }
        if (colors[candidate-3] == colors[candidate-4] &&
            colors[candidate-2] == colors[candidate-4] &&
            colors[candidate-1] == colors[candidate-4]) matches = false;
        if (matches) { top_end = candidate; break; }
    }
    if (!top_end) return false;
    // x231 intersects the charge gauge on rows 134..142. Use the blank
    // gap between the HP bar and hearts there; below the gauges use the
    // right-hand gutter. Authenticate a run, not an isolated gauge pixel.
    for (unsigned y = 128; y < kHeight; ++y) {
        const unsigned begin = y < 144 ? 176 : 225;
        const unsigned end = y < 144 ? 184 : 232;
        colors[y] = pixel(begin, y);
        if (!uniform(y, begin, end, colors[y])) return false;
    }
    for (unsigned y : {128u, 159u}) {
        if (!uniform(y, 8, 13, colors[y])) return false;
    }
    if (!uniform(159, 225, 232, colors[159])) return false;

    // Authentication as well as decoration: every separator spans the native
    // interior. An unrecognized menu, transition, or attack overlay cannot
    // silently paint tan on an unrelated screen. Validate all
    // rows BEFORE writing any pixel, so a partial match has no visible effect.
    for (unsigned y : {125u, 126u, 127u}) {
        colors[y] = pixel(8, y);
        if (!uniform(y, 8, 232, colors[y])) return false;
    }
    // Uniform black/white covers are not a visible normal HUD. Let the existing
    // scene/effect policy handle them. Ordinary fades retain their live colors
    // until the contrast collapses; no bright cached tan can survive a fade.
    if (colors[125] == colors[128] && colors[126] == colors[128] &&
        colors[127] == colors[128]) return false;

    for (unsigned y = 0; y < kHeight; ++y) {
        if (y >= top_end && y < kBattleHudBottomStart) continue;
        auto* row = rgb + static_cast<std::size_t>(y) * width * 3u;
        for (unsigned x = 0; x < extra_left; ++x)
            std::memcpy(row + x * 3u, colors[y].data(), 3u);
        for (unsigned x = extra_left + kNativeWidth; x < width; ++x)
            std::memcpy(row + x * 3u, colors[y].data(), 3u);
    }
    return true;
}
}  // namespace swordcraft3
