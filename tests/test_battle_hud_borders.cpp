#include "battle_hud_borders.h"
#include "battle_layer_policy.h"
#include "swordcraft3_display_settings.h"
#include "runtime.h"
#include "recomp_runtime_ui.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(expr) do { if (!(expr)) throw std::runtime_error(#expr); } while (0)
namespace {
using Color = std::array<std::uint8_t, 3>;
constexpr Color kTan{255, 247, 206};
constexpr const char* kKey = "swordcraft3.battle_hud_borders";

void env_override(const char* value) {
#ifdef _WIN32
    _putenv_s("SWORDCRAFT3_BATTLE_HUD_BORDERS", value ? value : "");
#else
    if (value) setenv("SWORDCRAFT3_BATTLE_HUD_BORDERS", value, 1);
    else unsetenv("SWORDCRAFT3_BATTLE_HUD_BORDERS");
#endif
}

void put(std::vector<std::uint8_t>& rgb, unsigned width, unsigned x,
         unsigned y, Color color) {
    for (unsigned c = 0; c < 3; ++c) rgb[(y * width + x) * 3 + c] = color[c];
}

Color get(const std::vector<std::uint8_t>& rgb, unsigned width,
          unsigned x, unsigned y) {
    const auto* p = &rgb[(y * width + x) * 3];
    return {p[0], p[1], p[2]};
}

std::vector<std::uint8_t> frame(unsigned width, unsigned left) {
    std::vector<std::uint8_t> rgb(width * 160 * 3, 31);
    for (unsigned y = 0; y < 160; ++y) {
        for (unsigned x = 0; x < 240; ++x) {
            Color color = (y < 19 || y >= 125) ? kTan : Color{55, 86, 117};
            if (y == 16 || y == 127) color = {181, 115, 82};
            if (y == 17 || y == 126) color = {255, 189, 0};
            if (y == 18 || y == 125) color = {107, 0, 0};
            put(rgb, width, left + x, y, color);
        }
    }
    // A dominant HP-bar color is deliberately NOT the desired border color.
    for (unsigned y = 0; y < 16; ++y)
        for (unsigned x = 45; x < 220; ++x)
            put(rgb, width, left + x, y, {255, 0, 0});
    return rgb;
}

void rendering_tests() {
    CHECK(swordcraft3::battle_critical_layout(0x1741, 0, 0x450B, 0x4385));
    CHECK(!swordcraft3::battle_critical_layout(0x17C1, 0, 0x450B, 0x4385)); // blank
    CHECK(!swordcraft3::battle_critical_layout(0x1740, 0, 0x450B, 0x4385)); // text
    CHECK(!swordcraft3::battle_critical_layout(0x1742, 0, 0x450B, 0x4385)); // Mode 2
    CHECK(!swordcraft3::battle_critical_layout(0x1741, 0, 0x450B, 0x6385)); // wrap
    CHECK(!swordcraft3::battle_critical_layout(0x3741, 0, 0x450B, 0x4385)); // window
    CHECK(!swordcraft3::battle_critical_layout(0x1741, 1, 0x450B, 0x4385)); // other HUD
    CHECK(!swordcraft3::battle_critical_layout(0x1741, 0, 0x440B, 0x4385)); // other map
    // Left-hand attack at scroll 40: its wrapped copy at x296 is not real.
    CHECK(swordcraft3::battle_effect_contains(-20, 40, 128));
    CHECK(swordcraft3::battle_effect_contains(40, 40, 128));
    CHECK(!swordcraft3::battle_effect_contains(296, 40, 128));
    CHECK(!swordcraft3::battle_effect_contains(88, 40, 128));
    CHECK(swordcraft3::battle_effect_contains(245, 134, 128));
    CHECK(!swordcraft3::battle_effect_contains(-11, 134, 128));
    CHECK(!swordcraft3::battle_effect_contains(-60, 72, 128)); // tied copies
    CHECK(!swordcraft3::battle_effect_contains(0, 0, 0));
    CHECK(!swordcraft3::battle_effect_contains(0, 0, 257));
    CHECK(swordcraft3::battle_backdrop_x(-1, 0, 384) == 383);
    CHECK(swordcraft3::battle_backdrop_x(-41, 40, 384) == 343);
    CHECK(swordcraft3::battle_backdrop_x(250, 40, 384) == 250);
    std::array<std::uint64_t,48> columns{};
    for (unsigned x=0; x<columns.size(); ++x) columns[x]=x%20;
    CHECK(swordcraft3::battle_repeat_columns(columns.data(),48)==20); // forest: 160px
    columns[47]=999;
    CHECK(swordcraft3::battle_repeat_columns(columns.data(),48)==48);
    CHECK(swordcraft3::battle_repeat_columns(nullptr,48)==0);
    CHECK(swordcraft3::battle_backdrop_x(-1,0,160)==159);
    CHECK(swordcraft3::battle_backdrop_x(240,40,160)==80);
    for (unsigned width : {241u, 263u, 284u, 320u, 383u, 384u}) {
        for (unsigned left : {0u, (width - 240) / 2, width - 240}) {
            auto rgb = frame(width, left);
            auto original = rgb;
            CHECK(swordcraft3::extend_battle_hud_borders(rgb.data(), width, 160, left, width - 240 - left));
            for (unsigned y = 0; y < 160; ++y) {
                for (unsigned x = 0; x < width; ++x) {
                    if ((x >= left && x < left + 240) || (y >= 19 && y < 125))
                        CHECK(get(rgb, width, x, y) == get(original, width, x, y));
                    else
                        CHECK(get(rgb, width, x, y) == get(original, width, left + (y >= 128 ? 231 : 8), y));
                }
            }
        }
    }
    auto native = frame(240, 0);
    auto native_before = native;
    CHECK(!swordcraft3::extend_battle_hud_borders(native.data(), 240, 160, 0, 0));
    CHECK(native == native_before);
    CHECK(!swordcraft3::extend_battle_hud_borders(nullptr, 384, 160, 72, 72));

    auto paused = frame(384, 72);
    for (unsigned y = 16; y < 56; ++y) {
        for (unsigned x = 0; x < 240; ++x)
            put(paused, 384, 72+x, y, kTan);
    }
    // The middle rows deliberately contain gameplay-like
    // colors outside the native HUD, as in the repeated paused HUD capture.
    for (unsigned x = 72; x < 312; ++x) {
        put(paused, 384, x, 56, {181,115,82});
        put(paused, 384, x, 57, {255,189,0});
        put(paused, 384, x, 58, {107,0,0});
    }
    auto paused_before = paused;
    CHECK(swordcraft3::extend_battle_hud_borders(paused.data(), 384, 160, 72, 72));
    for (unsigned y = 0; y < 160; ++y)
        for (unsigned x = 0; x < 384; ++x) {
            const bool protected_pixel = (x >= 72 && x < 312) || (y >= 59 && y < 125);
            CHECK(get(paused, 384, x, y) == get(paused_before, 384,
                  protected_pixel ? x : 80, y));
        }

    // A full charge gauge touches x231. Its frame and fill must never be
    // extruded into the margins (regression from the user's F10 captures).
    auto gauge = frame(285, 22);
    for (unsigned y = 134; y < 143; ++y)
        for (unsigned x = 208; x < 232; ++x)
            put(gauge, 285, 22 + x, y, {181, 0, 0});
    auto gauge_before = gauge;
    CHECK(swordcraft3::extend_battle_hud_borders(gauge.data(), 285, 160, 22, 23));
    for (unsigned y = 128; y < 160; ++y) {
        CHECK(get(gauge, 285, 0, y) == kTan);
        CHECK(get(gauge, 285, 284, y) == kTan);
        for (unsigned x = 22; x < 262; ++x)
            CHECK(get(gauge, 285, x, y) == get(gauge_before, 285, x, y));
    }

    for (auto [x, y] : {std::pair{8u, 3u}, {228u, 4u}, {100u, 18u},
                        {200u, 125u}, {10u, 128u}, {230u, 159u}, {179u, 137u}}) {
        auto rgb = frame(384, 72);
        put(rgb, 384, 72 + x, y, {17, 23, 29});
        auto original = rgb;
        CHECK(!swordcraft3::extend_battle_hud_borders(rgb.data(), 384, 160, 72, 72));
        CHECK(rgb == original);  // failed authentication cannot partially paint
    }
    auto invalid = frame(384, 72);
    auto before = invalid;
    CHECK(!swordcraft3::extend_battle_hud_borders(invalid.data(), 384, 160, 73, 72));
    CHECK(!swordcraft3::extend_battle_hud_borders(invalid.data(), 384, 159, 72, 72));
    CHECK(!swordcraft3::extend_battle_hud_borders(invalid.data(), 384, 160, 0xFFFFFFFFu, 72));
    CHECK(invalid == before);

    for (std::uint8_t shade : {0, 255}) {
        std::vector<std::uint8_t> cover(384 * 160 * 3, shade);
        auto original = cover;
        CHECK(!swordcraft3::extend_battle_hud_borders(cover.data(), 384, 160, 72, 72));
        CHECK(cover == original);
    }
    // Palette changes, fade-to-black, fade-to-white and per-scanline effects:
    // use actual transformed native samples, never a cached or fixed tan.
    for (int transform = 0; transform < 3; ++transform) {
        auto rgb = frame(384, 72);
        for (unsigned y = 0; y < 160; ++y) {
            for (unsigned x = 72; x < 312; ++x) {
                auto p = get(rgb, 384, x, y);
                for (auto& c : p) c = transform == 0 ? c / 4 :
                    transform == 1 ? 200 + c / 5 : (c + y) / 2;
                put(rgb, 384, x, y, p);
            }
        }
        const auto wanted_top = get(rgb, 384, 80, 7);
        const auto wanted_bottom = get(rgb, 384, 303, 145);
        CHECK(swordcraft3::extend_battle_hud_borders(rgb.data(), 384, 160, 72, 72));
        CHECK(get(rgb, 384, 0, 7) == wanted_top);
        CHECK(get(rgb, 384, 383, 145) == wanted_bottom);
    }
}

void settings_tests() {
    const char* old_env = std::getenv("SWORDCRAFT3_BATTLE_HUD_BORDERS");
    const std::string saved_env = old_env ? old_env : "";
    env_override(nullptr);
    const auto dir = std::filesystem::current_path() /
        ("presentation-settings-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(std::filesystem::create_directory(dir));
    const auto config = dir / "swordcraft3-display.ini";
    const std::string exe = (dir / "test.exe").string();
    auto cleanup = [&] {
        std::filesystem::remove(config);
        std::filesystem::remove(dir);
        env_override(saved_env.empty() ? nullptr : saved_env.c_str());
    };
    try {
        gbarecomp::RunOptions opts;
        configure_swordcraft3_display_settings(opts, exe.c_str());
        CHECK(!std::filesystem::exists(config)); // no startup configuration writes
        CHECK(swordcraft3_battle_hud_borders_enabled());
        CHECK(opts.ui_extra_item_count == 1);
        const auto& item = *static_cast<const RecompRuntimeUiItem*>(opts.ui_extra_items);
        CHECK(std::string(item.section) == "Display");
        CHECK(std::string(item.label) == "Battle HUD borders");
        CHECK(item.type == RECOMP_RUNTIME_UI_BOOL);
        int value = -1;
        CHECK(opts.ui_get(kKey, &value) && value == 1);
        CHECK(!opts.ui_get("unknown", &value));
        CHECK(!opts.ui_get(kKey, nullptr));
        CHECK(!opts.ui_set(kKey, 2));
        CHECK(opts.ui_set(kKey, 0));
        CHECK(!swordcraft3_battle_hud_borders_enabled());
        configure_swordcraft3_display_settings(opts, exe.c_str());
        CHECK(!swordcraft3_battle_hud_borders_enabled());

        env_override("1");
        configure_swordcraft3_display_settings(opts, exe.c_str());
        CHECK(swordcraft3_battle_hud_borders_enabled());
        CHECK(opts.ui_set(kKey, 1)); // session override must not save to disk
        env_override(nullptr);
        configure_swordcraft3_display_settings(opts, exe.c_str());
        CHECK(!swordcraft3_battle_hud_borders_enabled());
        { std::ofstream file(config); file << "[display]\nbattle_hud_borders=10\n"; }
        configure_swordcraft3_display_settings(opts, exe.c_str());
        CHECK(swordcraft3_battle_hud_borders_enabled()); // invalid value ignored
        { std::ofstream file(config); file << "[other]\nbattle_hud_borders=0\n"; }
        configure_swordcraft3_display_settings(opts, exe.c_str());
        CHECK(swordcraft3_battle_hud_borders_enabled());
        { std::ofstream file(config); file << " [display]\r\n battle_hud_borders = 0 \r\n"; }
        configure_swordcraft3_display_settings(opts, exe.c_str());
        CHECK(!swordcraft3_battle_hud_borders_enabled());
    } catch (...) { cleanup(); throw; }
    cleanup();
}
}

int main() {
    try {
        rendering_tests();
        settings_tests();
        std::cout << "Battle HUD border geometry, native/gameplay preservation, "
                     "guards, fades, runtime menu and persistence tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
