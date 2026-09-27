#pragma once
#include <string_view>
#include <array>

namespace swordcraft3 {
struct BetaArtwork { std::string_view token; const char* path; };
// Supplied artwork stays intact. Persist only a token from this allowlist.
inline constexpr std::array<BetaArtwork, 11> beta_artwork{{
    {"clean", "assets/beta/boxart-clean.png"},
    {"archer", "assets/beta/art-archer.png"},
    {"winter", "assets/beta/art-winter.png"},
    {"parchment", "assets/beta/art-parchment.png"},
    {"crystal", "assets/beta/art-crystal.png"},
    {"glasses", "assets/beta/art-glasses.png"},
    {"ensemble", "assets/beta/art-ensemble.png"},
    {"horizon", "assets/beta/art-horizon.png"},
    {"beginnings", "assets/beta/art-beginnings.png"},
    {"swordsmith", "assets/beta/art-swordsmith.png"},
    {"original", "assets/beta/boxart-original.png"},
}};
constexpr std::string_view next_beta_boxart(std::string_view previous) {
    for (std::size_t i = 0; i < beta_artwork.size(); ++i)
        if (previous == beta_artwork[i].token)
            return beta_artwork[(i + 1) % beta_artwork.size()].token;
    return beta_artwork.front().token;
}
constexpr const char* beta_boxart_path(std::string_view selected) {
    for (const auto& art : beta_artwork)
        if (selected == art.token) return art.path;
    return beta_artwork.front().path;
}
}
