#include "launcher_seam.h"
#include "common/launcher_model.h"
#include "common/launcher_theme.h"
#include "common/sha1.h"
#include "../src/beta_credits.h"
#include <chrono>
#include <memory>
#include <iostream>
#define SDL_MAIN_HANDLED
#include <SDL.h>

#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (0)
namespace fs = std::filesystem;
static void put(const fs::path& p, const std::string& data) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary); f << data; f.flush(); CHECK(f.good());
}
static std::string get(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
int main(int argc, char** argv) {
    const auto root = fs::current_path() / "validation/pt" /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto portable = root / "Portable Game";
    fs::create_directories(portable / "Settings");
    put(root / "source/test.gba", "synthetic source, not a ROM");
    const auto imported = gbarecomp::portable_import(portable, "ROMs", root/"source/test.gba");
    CHECK(get(imported) == get(root/"source/test.gba"));
    CHECK(gbarecomp::portable_import(portable, "ROMs", imported) == imported);
    CHECK(gbarecomp::portable_import(portable, "ROMs", root/"source/test.gba") == imported);
    const auto cache = portable/"Settings/rom.cfg";
    const auto ref = gbarecomp::portable_reference(imported, cache);
    CHECK(!fs::path(ref).is_absolute());
    CHECK(fs::weakly_canonical(cache.parent_path()/ref) == imported);
    // Move the folder, retaining only its relative persisted references.
    const auto moved = root/"Moved Game";
    fs::rename(portable, moved);
    CHECK(get(moved/"Settings"/ref) == get(root/"source/test.gba"));

    gbarecomp_seam::SeamConfig cfg;
    CHECK(cfg.player_key[0] == SDL_SCANCODE_X);
    CHECK(cfg.player_key[9] == SDL_SCANCODE_C);
    CHECK(cfg.player_pad[4] == RECOMP_LAUNCHER_PAD_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_RIGHT));
    cfg.player_key[0] = SDL_SCANCODE_G;
    cfg.player_bindings_declared = true;
    cfg.player_pad[0] = RECOMP_LAUNCHER_PAD_AXIS(SDL_CONTROLLER_AXIS_LEFTX, 1);
    cfg.assist_fast_forward_multiplier = 7;
    const auto ini = moved/"Settings/launcher.ini";
    put(ini, "[KeyMap]\nPause = Shift+P\n");
    gbarecomp_seam::seam_config_save(ini.string(), cfg);
    gbarecomp_seam::SeamConfig loaded;
    gbarecomp_seam::seam_config_load(ini.string(), &loaded);
    CHECK(loaded.player_key[0] == SDL_SCANCODE_G);
    CHECK(loaded.player_pad[0] == cfg.player_pad[0]);
    CHECK(loaded.assist_fast_forward_multiplier == 7);
    CHECK(get(ini).find("Pause = Shift+P") != std::string::npos);

    auto m = std::make_unique<LauncherModel>();
    const auto credits_file = moved/"Credits/original-game.txt";
    CHECK(swordcraft3::read_beta_credits(credits_file, "Pending A") == "Pending A");
    put(credits_file, "\xEF\xBB\xBFOriginal game\r\n100% credited\n");
    CHECK(swordcraft3::read_beta_credits(credits_file, "Pending A") == "Original game\r\n100% credited\n");
    std::string long_credits;
    for (int i = 0; i < 500; ++i) long_credits += "Contributor " + std::to_string(i) + "\n";
    put(credits_file, long_credits);
    CHECK(swordcraft3::read_beta_credits(credits_file, "Pending A") == long_credits);
    put(credits_file, "");
    CHECK(swordcraft3::read_beta_credits(credits_file, "Pending A") == "Pending A");
    RecompLauncherCGameInfo gi{};
    launcher_profile_apply("gba", &gi);
    gi.hybrid_input = gi.settings_bindings = 1;
    RecompLauncherCSettings settings{};
    settings.player_src[0] = 1;
    settings.player_key_bind[0][4] = SDL_SCANCODE_X;
    settings.player_pad_bind[0][4] = 1;
    gi.default_settings = &settings;
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    CHECK(!m->sidebar_layout && !m->logo_path && !m->backdrop_path && !m->tagline);
    gi.theme = "storybook";
    gi.logo_path = "assets/test-logo.png";
    gi.backdrop_path = "assets/test-backdrop.png";
    gi.tagline = "Test tagline";
    gi.sidebar_layout = true;
    gi.settings_note = "Test folder note";
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    CHECK(m->sidebar_layout && m->logo_path == gi.logo_path && m->backdrop_path == gi.backdrop_path);
    CHECK(m->tagline == gi.tagline);
    CHECK(m->settings_note == gi.settings_note);
    const auto paper = launcher_theme_by_name(gi.theme);
    CHECK(paper.scanlines == 0 && paper.background.r > .9f && paper.text.r < .3f);
    CHECK(launcher_theme_by_name("gba").background.r < .1f);
    for (auto view : {LNG_VIEW_DASHBOARD, LNG_VIEW_SETTINGS, LNG_VIEW_CONTROLLER,
                     LNG_VIEW_ASSIST_TOOLS, LNG_VIEW_MODS, LNG_VIEW_CREDITS}) {
        launcher_model_set_view(m.get(), view);
        CHECK(m->view == view);
    }
    launcher_model_set_view(m.get(), LNG_VIEW_DASHBOARD);
    CHECK(!launcher_model_has_credits_view(m.get()));
    CHECK(!launcher_model_has_credits_view(nullptr));
    gi.credits_text = "Pending A";
    gi.credits_title = "Original Game Credits";
    gi.credits_secondary_text = "Pending B";
    gi.credits_secondary_title = "PC Port Credits";
    gi.credits_tools_text = "Tools\nhttps://example.org/\n";
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    CHECK(launcher_model_has_credits_view(m.get()));
    CHECK(std::string(m->credits_text) == "Pending A");
    CHECK(std::string(m->credits_secondary_text) == "Pending B");
    CHECK(std::string(m->credits_title) == "Original Game Credits");
    CHECK(std::string(m->credits_secondary_title) == "PC Port Credits");
    launcher_model_set_view(m.get(), LNG_VIEW_CREDITS);
    CHECK(m->view == LNG_VIEW_CREDITS);
    CHECK(std::string(m->credits_tools_text) == gi.credits_tools_text);
    CHECK(!m->credits_tools_open);
    launcher_model_show_credit_tools(m.get(), true);
    CHECK(m->credits_tools_open);
    launcher_model_show_credit_tools(m.get(), false);
    CHECK(!m->credits_tools_open);
    launcher_model_show_credit_tools(m.get(), true);
    launcher_model_set_view(m.get(), LNG_VIEW_DASHBOARD);
    CHECK(!m->credits_tools_open);
    launcher_model_show_credit_tools(m.get(), true);
    CHECK(!m->credits_tools_open);
    launcher_model_set_view(m.get(), LNG_VIEW_CREDITS);
    m->credits_tools_text = nullptr;
    launcher_model_show_credit_tools(m.get(), true);
    CHECK(!m->credits_tools_open);
    m->credits_tools_text = gi.credits_tools_text;
    m->credits_text = nullptr;
    CHECK(launcher_model_has_credits_view(m.get()));
    m->credits_text = gi.credits_text;
    CHECK(m->hybrid_input);
    // A patch-only launcher must expose Mods even with the catalog disabled.
    CHECK(!launcher_model_has_mods_view(nullptr));
    CHECK(!launcher_model_has_mods_view(m.get()));
    m->rom_patch_supported = true;
    CHECK(m->mods == nullptr);
    CHECK(launcher_model_has_mods_view(m.get()));
    launcher_model_set_view(m.get(), LNG_VIEW_MODS);
    CHECK(m->view == LNG_VIEW_MODS);
    m->rom_patch_supported = false;
    RecompLauncherCModProvider provider{};
    m->mods = &provider;
    CHECK(launcher_model_has_mods_view(m.get()));
    m->mods = nullptr;
    CHECK(!launcher_model_has_mods_view(m.get()));
    launcher_model_begin_capture(m.get(), 4);
    launcher_model_set_captured_key(m.get(), SDL_SCANCODE_G);
    CHECK(m->s.player_key_bind[0][4] == SDL_SCANCODE_G);
    launcher_model_begin_pad_capture(m.get(), 4);
    launcher_model_set_captured_pad(m.get(), cfg.player_pad[0]);
    CHECK(m->s.player_pad_bind[0][4] == cfg.player_pad[0]);
    launcher_model_reset_player_bindings(m.get(), 0);
    CHECK(m->s.player_key_bind[0][4] == SDL_SCANCODE_X);
    CHECK(m->s.player_pad_bind[0][4] == 1);

    const auto save = moved/"Saves/battery.eep";
    const auto selected = root/"source/import.eep";
    const auto bad = root/"source/bad.eep";
    put(save, "old!"); put(selected, "new!"); put(bad, "bad");
    const auto save_string = save.string();
    m->sram_path = save_string.c_str(); m->battery_save_size = 4;
    launcher_model_import_sram(m.get(), bad.string().c_str());
    CHECK(get(save) == "old!");
    launcher_model_import_sram(m.get(), selected.string().c_str());
    CHECK(get(save) == "new!");
    CHECK(get(save.string()+".backup-1") == "old!");
    CHECK(get(selected) == "new!");
    launcher_model_clear_sram(m.get());
    CHECK(!fs::exists(save));
    CHECK(get(save.string()+".backup-2") == "new!");
    CHECK(get(save.string()+".backup-1") == "old!");
    // Missing/unwritable parent cannot destroy the source.
    m->sram_path = "does-not-exist/battery.eep";
    launcher_model_import_sram(m.get(), selected.string().c_str());
    CHECK(get(selected) == "new!");

    // A stock image cannot launch unpatched against a target-only corpus.
    m->rom_present = true; std::strcpy(m->rom_size, "1 MB");
    m->has_bios = false;
    m->rom_patch_supported = true;
    m->rom_patch_required_sha1 = "target";
    std::strcpy(m->rom_sha1_hex, "stock");
    m->s.rom_patch_enabled = false;
    CHECK(!launcher_model_can_launch(m.get()));
    std::strcpy(m->rom_sha1_hex, "target");
    CHECK(launcher_model_can_launch(m.get()));
    // An exact target is never patched a second time.
    m->s.rom_patch_enabled = true; std::strcpy(m->s.rom_patch_path, "missing.bps");
    CHECK(launcher_model_prepare_rom_patch(m.get()));
    CHECK(!m->rom_patch_prepared_path[0]);
    if (argc == 3) {
        // Optional private integration: real stock ROM + supplied beta patch.
        const char* known[] = {"3f5253fcf57e07ce52472bd29a61d16b98a12376",
                               "bb2eebf98deb59bb6218442c2308bb5033ae2915"};
        const auto patchcache = moved/"Mods/rom-patches";
        fs::create_directories(patchcache);
        const auto cache_string = patchcache.string();
        gi.rom_patch_supported = 1;
        gi.known_sha1_hex = known; gi.num_known_sha1 = 2;
        gi.rom_patch_required_sha1 = known[1];
        gi.rom_patch_cache_dir = cache_string.c_str();
        gi.has_bios = 0;
        launcher_model_init(m.get(), &settings, &gi, argv[1]);
        CHECK(launcher_model_rom_verified(m.get()));
        CHECK(!launcher_model_can_launch(m.get()));
        launcher_model_set_rom_patch(m.get(), argv[2]);
        CHECK(launcher_model_can_launch(m.get()));
        if (!launcher_model_prepare_rom_patch(m.get()))
            throw std::runtime_error(m->rom_patch_status);
        CHECK(std::strcmp(m->rom_patch_prepared_sha1, known[1]) == 0);
        CHECK(fs::exists(m->rom_patch_prepared_path));
        // Valid IPS syntax but incompatible output must be rejected.
        const auto wrong = moved/"Mods/wrong.ips";
        put(wrong, std::string("PATCH\0\0\0\0\1XEOF", 14));
        launcher_model_set_rom_patch(m.get(), wrong.string().c_str());
        CHECK(!launcher_model_prepare_rom_patch(m.get()));
        std::cout << "PASS: real beta BPS produces exact target; incompatible IPS rejected\n";
    }
    std::cout << "PASS: portable import/relocation, bindings, defaults, battery safety, patch target gates\n";
    return 0;
}
