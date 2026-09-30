#include "launcher_seam.h"
#include "presentation_preferences.h"
#include "common/launcher_model.h"
#include "common/launcher_theme.h"
#include "common/sha1.h"
#include "../src/beta_credits.h"
#include "../src/guard_preferences.h"
#include "../src/graphics_presets.h"
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

static void test_guard_mod(const fs::path& root) {
    using swordcraft3::GuardPreferences;
    fs::create_directories(root / "Settings");
    GuardPreferences prefs;
    prefs.load(root);
    CHECK(!prefs.enabled && prefs.error.empty() && !fs::exists(prefs.path));
    auto m = std::make_unique<LauncherModel>();
    RecompLauncherCSettings settings{};
    RecompLauncherCGameInfo gi{};
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    CHECK(!launcher_model_has_mods_view(m.get()));
    CHECK(!launcher_model_set_builtin_mod(m.get(), 0, true));
    RecompLauncherCBuiltinMod mod{"select-guard", "Hold Select to Guard", "Combat only",
        &prefs, GuardPreferences::get, GuardPreferences::set, GuardPreferences::last_error};
    gi.builtin_mods = &mod;
    gi.builtin_mod_count = 1;
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    CHECK(launcher_model_has_mods_view(m.get()));
    CHECK(!launcher_model_set_builtin_mod(m.get(), -1, true));
    CHECK(!launcher_model_set_builtin_mod(m.get(), 1, true));
    CHECK(launcher_model_set_builtin_mod(m.get(), 0, true) && prefs.enabled);
    GuardPreferences reopened;
    reopened.load(root);
    CHECK(reopened.enabled && reopened.error.empty());
    CHECK(launcher_model_set_builtin_mod(m.get(), 0, false) && !prefs.enabled);
    reopened.load(root);
    CHECK(!reopened.enabled && reopened.error.empty());
    // Preserve comments, BOM and unrelated sections; never reset other settings.
    put(prefs.path, "\xef\xbb\xbf[Launcher] ; mods\r\nselect_guard = 1 # keep\r\nother = 7\r\n[Private]\r\nkeep = yes\r\n");
    prefs.load(root);
    CHECK(prefs.enabled && prefs.error.empty());
    CHECK(launcher_model_set_builtin_mod(m.get(), 0, false));
    reopened.load(root);
    CHECK(!reopened.enabled && reopened.error.empty());
    CHECK(get(prefs.path).find("# keep") != std::string::npos);
    CHECK(get(prefs.path).find("keep = yes") != std::string::npos);
    CHECK(get(prefs.path).compare(0, 3, "\xef\xbb\xbf") == 0);
    for (const auto& text : {std::string("[Launcher]\nselect_guard = 2\n"),
                            std::string("[Launcher]\nselect_guard = 1\nselect_guard = 0\n"),
                            std::string(65537, 'x')}) {
        put(prefs.path, text);
        prefs.load(root);
        CHECK(!prefs.enabled && !prefs.error.empty());
        CHECK(!launcher_model_set_builtin_mod(m.get(), 0, true));
        CHECK(!prefs.enabled && m->builtin_mod_status[0] && get(prefs.path) == text);
    }
    put(prefs.path, "[Launcher]\nselect_guard = 1\n");
    prefs.load(root);
    CHECK(prefs.enabled);
    fs::permissions(prefs.path, fs::perms::owner_read, fs::perm_options::replace);
    CHECK(!launcher_model_set_builtin_mod(m.get(), 0, false));
    CHECK(prefs.enabled); // failed saves never lie about the active selection
    fs::permissions(prefs.path, fs::perms::owner_write, fs::perm_options::add);
    CHECK(launcher_model_set_builtin_mod(m.get(), 0, false));
    std::cout << "PASS: built-in Mods visibility, on/off, restart persistence, strict parsing, failure safety\n";
}

static void test_graphics_presets(const fs::path& root) {
    using namespace swordcraft3;
    using namespace gbarecomp_seam;
    auto m = std::make_unique<LauncherModel>();
    RecompLauncherCGameInfo gi{};
    launcher_profile_apply("gba", &gi);
    RecompLauncherCSettings settings{};
    settings.screen_effect_strength = 35;
    settings.screen_kind = 4; settings.window_scale = 4; settings.fullscreen = 1;
    settings.volume = 73;
    gi.default_settings = &settings;
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    CHECK(m->graphics_preset_count == 0);
    CHECK(launcher_model_graphics_preset(m.get()) == -1);
    gi.graphics_presets = graphics_presets; gi.graphics_preset_count = graphics_preset_count;
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    CHECK(m->graphics_preset_count == 0); // recipes cannot advertise unsupported features
    gi.has_sharp_filter = gi.has_smooth_filter = gi.has_screen_effects = 1;
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    CHECK(m->graphics_preset_count == 5 && launcher_model_graphics_preset(m.get()) == 0);
    const auto original = m->s;
    const auto ini = root / "Settings/launcher.ini";
    for (int i = 0; i < graphics_preset_count; ++i) {
        const auto& p = graphics_presets[i];
        CHECK(launcher_model_apply_graphics_preset(m.get(), i));
        CHECK(launcher_model_graphics_preset(m.get()) == i);
        CHECK(m->s.linear_filter == (p.scaling == 1));
        CHECK(m->s.sharp_filter == (p.scaling == 2));
        CHECK(m->s.smooth_filter == (p.scaling == 3));
        CHECK(m->s.screen_effect == p.effect && m->s.screen_effect_strength == p.strength);
        // Byte-for-byte model check with just the five recipe fields restored:
        // no aspect, colour, window, audio, controls, paths or patch side effects.
        auto unrelated = m->s;
        unrelated.linear_filter = original.linear_filter;
        unrelated.sharp_filter = original.sharp_filter;
        unrelated.smooth_filter = original.smooth_filter;
        unrelated.screen_effect = original.screen_effect;
        unrelated.screen_effect_strength = original.screen_effect_strength;
        CHECK(std::memcmp(&unrelated, &original, sizeof(original)) == 0);
        put(ini, "[Launcher]\nscreen = classic\nhost_aspect_index = 2\nvolume = 73\n[Other]\nkeep = yes\n");
        CHECK(gbarecomp::save_presentation_preferences(ini, p.scaling, p.effect, p.strength));
        SeamConfig reloaded;
        seam_config_load(ini.string(), &reloaded);
        CHECK(reloaded.linear_filter == (p.scaling == 1) && reloaded.sharp_filter == (p.scaling == 2));
        CHECK(reloaded.smooth_filter == (p.scaling == 3));
        CHECK(reloaded.screen_effect == p.effect && reloaded.screen_effect_strength == p.strength);
        CHECK(reloaded.volume == 73 && reloaded.host_aspect_index == 2);
        CHECK(get(ini).find("keep = yes") != std::string::npos);
    }
    const auto before_invalid = m->s;
    CHECK(!launcher_model_apply_graphics_preset(m.get(), -1));
    CHECK(!launcher_model_apply_graphics_preset(m.get(), 5));
    CHECK(std::memcmp(&before_invalid, &m->s, sizeof(m->s)) == 0);
    launcher_model_set_screen_effect_strength(m.get(), 40);
    CHECK(launcher_model_graphics_preset(m.get()) == -1);
    CHECK(launcher_model_apply_graphics_preset(m.get(), 1));
    launcher_model_set_screen_effect_strength(m.get(), 99);
    CHECK(launcher_model_graphics_preset(m.get()) == 1); // inactive strength is not a different look
    launcher_model_set_screen_effect(m.get(), 1);
    CHECK(launcher_model_graphics_preset(m.get()) == -1);
    launcher_model_restore_defaults(m.get());
    CHECK(launcher_model_graphics_preset(m.get()) == 0);
    RecompGraphicsPreset invalid{"Invalid", "Invalid", 4, 0, 35};
    gi.graphics_presets = &invalid; gi.graphics_preset_count = 1;
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    CHECK(m->graphics_preset_count == 0);
    CHECK(!recomp_graphics_preset_valid(&invalid));
    CHECK(recomp_graphics_preset_match(nullptr, 5, 0, 0, 35) == -1);
    std::cout << "PASS: five graphics recipes, capability gates, inferred Custom, preservation and live preference reload\n";
}

static void test_presentation_filters(const fs::path& root) {
    using namespace gbarecomp_seam;
    const auto ini = root / "Settings/filters.ini";
    for (int screen = 0; screen < gbarecomp::screen_model_count; ++screen) {
        put(ini, "[Launcher]\nscreen = raw\nvolume = 73\n");
        CHECK(gbarecomp::save_screen_model_preference(ini, screen));
        SeamConfig colour_loaded;
        seam_config_load(ini.string(), &colour_loaded);
        CHECK(colour_loaded.screen_kind == screen && colour_loaded.volume == 73);
        seam_config_save(ini.string(), colour_loaded);
        CHECK(gbarecomp::read_screen_model_preference(ini) == screen);
    }
    SeamConfig cfg;
    CHECK(cfg.linear_filter == 0 && cfg.smooth_filter == 0);
    CHECK(cfg.sharp_filter == -1); // unchanged host-default sentinel
    CHECK(cfg.screen_kind == 0 && cfg.screen_effect == 0);
    CHECK(cfg.screen_effect_strength == 35);
    // Native Esc preferences preserve a UTF-8 BOM. The launcher must still
    // recognize the first section instead of silently reverting its settings.
    put(ini, "\xef\xbb\xbf[Launcher]\r\nlinear_filter = 0\r\nsharp_filter = 0\r\n"
             "smooth_filter = 1\r\nscreen_effect = 2\r\nscreen_effect_strength = 61\r\nvolume = 73\r\n"
             "[KeyMap]\r\nPause = Shift+P\r\n[Other]\r\nkeep = yes\r\n");
    SeamConfig bom_loaded;
    seam_config_load(ini.string(), &bom_loaded);
    CHECK(bom_loaded.smooth_filter == 1 && bom_loaded.screen_effect == 2);
    CHECK(bom_loaded.screen_effect_strength == 61 && bom_loaded.volume == 73);
    CHECK(gbarecomp::save_presentation_preferences(ini, 2, 1, 28));
    CHECK(get(ini).compare(0, 3, "\xef\xbb\xbf") == 0);
    seam_config_load(ini.string(), &bom_loaded);
    CHECK(!bom_loaded.linear_filter && bom_loaded.sharp_filter == 1 && !bom_loaded.smooth_filter);
    CHECK(bom_loaded.screen_effect == 1 && bom_loaded.screen_effect_strength == 28);
    CHECK(bom_loaded.volume == 73);
    // Launcher writeback must recognize that same BOM header, replace its
    // section only once, preserve the BOM, and retain unrelated settings.
    seam_config_save(ini.string(), bom_loaded);
    const auto bom_saved = get(ini);
    CHECK(bom_saved.compare(0, 3, "\xef\xbb\xbf") == 0);
    CHECK(bom_saved.find("Pause = Shift+P") != std::string::npos);
    CHECK(bom_saved.find("keep = yes") != std::string::npos);
    for (const char* unique : {"[Launcher]", "linear_filter =", "sharp_filter =",
                               "smooth_filter =", "screen_effect =", "screen_effect_strength =", "volume ="}) {
        const auto first = bom_saved.find(unique);
        CHECK(first != std::string::npos);
        CHECK(bom_saved.find(unique, first + 1) == std::string::npos);
    }
    SeamConfig bom_reopened;
    seam_config_load(ini.string(), &bom_reopened);
    CHECK(!bom_reopened.linear_filter && bom_reopened.sharp_filter == 1 && !bom_reopened.smooth_filter);
    CHECK(bom_reopened.screen_effect == 1 && bom_reopened.screen_effect_strength == 28);
    CHECK(bom_reopened.volume == 73);
    // After the launcher has moved its section below the unrelated settings,
    // the live writer must still produce a readable BOM-preserving snapshot.
    CHECK(gbarecomp::save_presentation_preferences(ini, 3, 2, 62));
    seam_config_load(ini.string(), &bom_reopened);
    CHECK(bom_reopened.smooth_filter == 1 && !bom_reopened.linear_filter && !bom_reopened.sharp_filter);
    CHECK(bom_reopened.screen_effect == 2 && bom_reopened.screen_effect_strength == 62);
    CHECK(bom_reopened.volume == 73);
    put(ini, "[Launcher]\nscale = 3\nscreen = raw\n");
    seam_config_load(ini.string(), &cfg);
    CHECK(cfg.linear_filter == 0 && cfg.smooth_filter == 0);
    CHECK(cfg.screen_kind == 0 && cfg.screen_effect == 0);
    CHECK(cfg.screen_effect_strength == 35);
    put(ini, "[Launcher]\nlinear_filter = 1\nsharp_filter = 1\n");
    seam_config_load(ini.string(), &cfg);
    CHECK(cfg.linear_filter == 1 && cfg.sharp_filter == 0);
    CHECK(cfg.smooth_filter == 0 && cfg.screen_effect == 0);

    put(ini, "[Launcher]\nlinear_filter = 1\nsharp_filter = 1\n"
             "smooth_filter = 1\nscreen_effect = 99\nscreen_effect_strength = -8\n");
    seam_config_load(ini.string(), &cfg);
    CHECK(cfg.linear_filter == 0 && cfg.sharp_filter == 0 && cfg.smooth_filter == 1);
    CHECK(cfg.screen_effect == 0 && cfg.screen_effect_strength == 0);
    put(ini, "[Launcher]\nsmooth_filter = -1\nscreen_effect = -1\n"
             "screen_effect_strength = 500\n");
    seam_config_load(ini.string(), &cfg);
    CHECK(cfg.smooth_filter == 0 && cfg.screen_effect == 0);
    CHECK(cfg.screen_effect_strength == 100);

    auto m = std::make_unique<LauncherModel>();
    RecompLauncherCGameInfo gi{};
    launcher_profile_apply("gba", &gi);
    RecompLauncherCSettings settings{};
    set_extra_presentation_filters(settings, 0, 0, 35);
    gi.default_settings = &settings;
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    CHECK(!m->has_sharp_filter && !m->has_smooth_filter && !m->has_screen_effects);
    launcher_model_cycle_scaling_filter(m.get());
    launcher_model_set_scaling_filter(m.get(), 2);
    launcher_model_set_scaling_filter(m.get(), 3);
    launcher_model_set_screen_effect(m.get(), 2);
    launcher_model_set_screen_effect_strength(m.get(), 80);
    CHECK(!m->s.linear_filter && !m->s.sharp_filter && !m->s.smooth_filter);
    CHECK(m->s.screen_effect == 0 && m->s.screen_effect_strength == 35);
    launcher_model_toggle_filter(m.get());
    CHECK(m->s.linear_filter == 1);
    launcher_model_toggle_filter(m.get());
    CHECK(m->s.linear_filter == 0);

    gi.has_sharp_filter = 1;
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    for (const char* label : {"Linear", "Sharp fractional", "Nearest"}) {
        launcher_model_cycle_scaling_filter(m.get());
        CHECK(std::string(launcher_model_scaling_filter_label(m.get())) == label);
    }
    gi.has_sharp_filter = 0; gi.has_smooth_filter = 1;
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    for (const char* label : {"Linear", "Smooth 2x", "Nearest"}) {
        launcher_model_cycle_scaling_filter(m.get());
        CHECK(std::string(launcher_model_scaling_filter_label(m.get())) == label);
    }
    gi.has_sharp_filter = gi.has_screen_effects = 1;
    launcher_model_init(m.get(), &settings, &gi, nullptr);
    for (int expected : {1, 2, 3, 0, 1, 2, 3, 0}) {
        launcher_model_cycle_scaling_filter(m.get());
        CHECK(m->s.linear_filter == (expected == 1));
        CHECK(m->s.sharp_filter == (expected == 2));
        CHECK(m->s.smooth_filter == (expected == 3));
    }
    for (int chosen : {3, 1, 2, 0, 3}) {
        launcher_model_set_scaling_filter(m.get(), chosen);
        CHECK(m->s.linear_filter == (chosen == 1));
        CHECK(m->s.sharp_filter == (chosen == 2));
        CHECK(m->s.smooth_filter == (chosen == 3));
    }
    launcher_model_set_scaling_filter(m.get(), -1);
    launcher_model_set_scaling_filter(m.get(), 4);
    CHECK(m->s.smooth_filter == 1 && !m->s.linear_filter && !m->s.sharp_filter);
    launcher_model_toggle_filter(m.get());
    CHECK(m->s.linear_filter == 1 && !m->s.smooth_filter && !m->s.sharp_filter);
    launcher_model_set_scaling_filter(m.get(), 3);
    for (const char* label : {"LCD Grid", "CRT", "Off"}) {
        launcher_model_cycle_screen_effect(m.get());
        CHECK(std::string(launcher_model_screen_effect_label(m.get())) == label);
        CHECK(m->s.smooth_filter == 1); // screen effects layer over any scaler
    }
    launcher_model_set_screen_effect(m.get(), -8);
    CHECK(m->s.screen_effect == 0);
    launcher_model_set_screen_effect_strength(m.get(), -8);
    CHECK(m->s.screen_effect_strength == 0);
    launcher_model_set_screen_effect_strength(m.get(), 108);
    CHECK(m->s.screen_effect_strength == 100);
    launcher_model_set_screen_effect(m.get(), 2);
    launcher_model_set_screen_effect_strength(m.get(), 61);

    RecompLauncherCSettings committed{};
    launcher_model_commit(m.get(), &committed);
    cfg = SeamConfig{};
    cfg.linear_filter = committed.linear_filter;
    cfg.sharp_filter = get_sharp_filter(committed, false);
    get_extra_presentation_filters(committed, cfg.smooth_filter,
                                   cfg.screen_effect, cfg.screen_effect_strength);
    put(ini, "[KeyMap]\nPause = Shift+P\n[Other]\nkeep = yes\n");
    seam_config_save(ini.string(), cfg);
    SeamConfig reopened;
    seam_config_load(ini.string(), &reopened);
    CHECK(reopened.smooth_filter == 1 && reopened.screen_effect == 2);
    CHECK(reopened.screen_effect_strength == 61);
    CHECK(!reopened.linear_filter && !reopened.sharp_filter);
    CHECK(get(ini).find("Pause = Shift+P") != std::string::npos);
    CHECK(get(ini).find("keep = yes") != std::string::npos);
    RecompLauncherCSettings reopen_settings{};
    reopen_settings.linear_filter = reopened.linear_filter;
    set_presentation_filters(reopen_settings, reopened.sharp_filter, false);
    set_extra_presentation_filters(reopen_settings, reopened.smooth_filter,
                                  reopened.screen_effect, reopened.screen_effect_strength);
    launcher_model_init(m.get(), &reopen_settings, &gi, nullptr);
    CHECK(std::string(launcher_model_scaling_filter_label(m.get())) == "Smooth 2x");
    CHECK(std::string(launcher_model_screen_effect_label(m.get())) == "CRT");
    CHECK(m->s.screen_effect_strength == 61);
    launcher_model_restore_defaults(m.get());
    CHECK(!m->s.linear_filter && !m->s.sharp_filter && !m->s.smooth_filter);
    CHECK(m->s.screen_kind == 0 && m->s.screen_effect == 0);
    CHECK(m->s.screen_effect_strength == 35);

    // Malformed model snapshots cannot retain mutually exclusive scalers or
    // out-of-range screen effects, even when not loaded through the seam.
    reopen_settings.linear_filter = reopen_settings.sharp_filter = reopen_settings.smooth_filter = 1;
    reopen_settings.screen_effect = 30; reopen_settings.screen_effect_strength = -20;
    launcher_model_init(m.get(), &reopen_settings, &gi, nullptr);
    CHECK(!m->s.linear_filter && !m->s.sharp_filter && m->s.smooth_filter);
    CHECK(m->s.screen_effect == 0 && m->s.screen_effect_strength == 0);

    gbarecomp::RunOptions opts;
    std::vector<std::string> args;
    seam_append_setting_args(args, reopened, opts);
    for (const char* option : {"--smooth-filter", "--screen-effect", "--screen-effect-strength"})
        CHECK(std::find(args.begin(), args.end(), option) == args.end());
    opts.launcher_expose_sharp_filter = true;
    opts.launcher_expose_smooth_filter = opts.launcher_expose_screen_effects = true;
    const auto value = [](const std::vector<std::string>& av, const std::string& key) {
        const auto pos = std::find(av.begin(), av.end(), key);
        CHECK(pos != av.end() && pos + 1 != av.end());
        return *(pos + 1);
    };
    for (int effect : {0, 1, 2}) {
        args.clear(); reopened.screen_effect = effect;
        seam_append_setting_args(args, reopened, opts);
        CHECK(value(args, "--linear-filter") == "0");
        CHECK(value(args, "--sharp-filter") == "0");
        CHECK(value(args, "--smooth-filter") == "1");
        CHECK(value(args, "--screen-effect") == screen_effect_token(effect));
        CHECK(value(args, "--screen-effect-strength") == "61");
        CHECK(value(args, "--screen") == "raw");
    }
    args.clear(); reopened.smooth_filter = 0; reopened.screen_effect = 99;
    reopened.screen_effect_strength = 120;
    seam_append_setting_args(args, reopened, opts);
    CHECK(value(args, "--smooth-filter") == "0");
    CHECK(value(args, "--screen-effect") == "off");
    CHECK(value(args, "--screen-effect-strength") == "100");

    // Host-only aspects must not widen guest PPU scanout or reuse a legacy
    // hidden aspect_index=0 left in older portable installations.
    static const char* labels[] = {"Original GBA (3:2)", "Widescreen (16:9)", "Ultrawide (12:5)"};
    static const std::uint16_t widths[] = {240, 284, 384};
    opts.launcher_aspects_host_only = true;
    opts.launcher_expose_widescreen = true;
    opts.launcher_aspect_labels = labels;
    opts.launcher_aspect_view_widths = widths;
    opts.launcher_num_aspects = 3;
    opts.launcher_default_aspect = 2;
    reopened.aspect_index = 0;
    CHECK(reopened.host_aspect_index == -1);
    args.clear(); seam_append_setting_args(args, reopened, opts);
    CHECK(value(args, "--host-aspect") == "2");
    CHECK(std::find(args.begin(), args.end(), "--view-width") == args.end());
    for (int index : {0, 1, 2}) {
        reopened.host_aspect_index = index;
        seam_config_save(ini.string(), reopened);
        SeamConfig restored;
        seam_config_load(ini.string(), &restored);
        CHECK(restored.host_aspect_index == index);
        CHECK(gbarecomp::read_host_aspect_preference(ini, 3) == index);
        args.clear(); seam_append_setting_args(args, restored, opts);
        CHECK(value(args, "--host-aspect") == std::to_string(index));
        CHECK(std::find(args.begin(), args.end(), "--view-width") == args.end());
    }
    opts.launcher_aspects_host_only = false;
    args.clear(); seam_append_setting_args(args, reopened, opts);
    CHECK(std::find(args.begin(), args.end(), "--host-aspect") == args.end());

    // Additive seam helpers must also compile and safely no-op against an old
    // consumer ABI; older hosts are not required to upgrade with the runtime.
    struct LegacySettings {} legacy;
    int old_smooth = 0, old_effect = 0, old_strength = 35;
    set_extra_presentation_filters(legacy, 1, 2, 61);
    get_extra_presentation_filters(legacy, old_smooth, old_effect, old_strength);
    set_extra_presentation_filter_caps(legacy, true, true);
    CHECK(old_smooth == 0 && old_effect == 0 && old_strength == 35);
    std::cout << "PASS: presentation filters capability gates, scaler exclusivity, defaults, bounds, persistence and runtime arguments\n";
}

int main(int argc, char** argv) {
    const auto root = fs::current_path() / "validation/pt" /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto portable = root / "Portable Game";
    fs::create_directories(portable / "Settings");
    test_guard_mod(root / std::filesystem::path(u8"Guard mod 日本語"));
    test_presentation_filters(root / "Filter checks");
    test_graphics_presets(root / "Preset checks");
    put(root / "source/test.gba", "synthetic source, not a ROM");
    const auto imported = gbarecomp::portable_import(portable, "ROMs", root/"source/test.gba");
    CHECK(get(imported) == get(root/"source/test.gba"));
    CHECK(imported.filename() == "test.gba");
    CHECK(gbarecomp::portable_import(portable, "ROMs", imported) == imported);
    CHECK(gbarecomp::portable_import(portable, "ROMs", root/"source/test.gba") == imported);
    put(root/"other/test.gba", "different synthetic ROM");
    const auto second = gbarecomp::portable_import(portable, "ROMs", root/"other/test.gba");
    CHECK(second.filename() == "test (2).gba");
    CHECK(get(second) == "different synthetic ROM");
    CHECK(get(imported) == "synthetic source, not a ROM");
    CHECK(gbarecomp::portable_import(portable, "ROMs", root/"other/test.gba") == second);
    put(root/"source/English Beta Patch.bps", "synthetic patch");
    CHECK(gbarecomp::portable_import(portable, "Mods", root/"source/English Beta Patch.bps").filename() == "English Beta Patch.bps");
    put(root/"source/GBA BIOS.bin", "synthetic bios");
    CHECK(gbarecomp::portable_import(portable, "BIOS", root/"source/GBA BIOS.bin").filename() == "GBA BIOS.bin");
    put(portable/"ROMs/busy.gba.importing", "existing incomplete import");
    put(root/"source/busy.gba", "new data");
    CHECK(gbarecomp::portable_import(portable, "ROMs", root/"source/busy.gba").filename() == "busy (2).gba");
    CHECK(get(portable/"ROMs/busy.gba.importing") == "existing incomplete import");
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
    // Real model/import bridge: filenames survive re-opening and older numeric
    // names get labels ONLY after the existing identity checks pass.
    gi.name = "Synthetic Game";
    auto moved_string = moved.string();
    gi.import_file_ctx = moved_string.data();
    gi.import_file = [](void* ctx, const char* kind, const char* src,
                        char* out, size_t cap, char*, size_t) -> int {
        const auto path = gbarecomp::portable_import(static_cast<char*>(ctx), kind, src).string();
        if (path.size() >= cap) return 0;
        std::snprintf(out, cap, "%s", path.c_str()); return 1;
    };
    const std::string bytes = "synthetic source, not a ROM";
    uint8_t digest[20]; char digest_hex[41];
    recompui_sha1_compute(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), digest);
    recompui_sha1_hex(digest, digest_hex);
    const char* synthetic_known[] = {digest_hex};
    gi.known_sha1_hex = synthetic_known; gi.num_known_sha1 = 1;
    launcher_model_init(m.get(), &settings, &gi, (root/"source/test.gba").string().c_str());
    CHECK(std::string(m->rom_file) == "test.gba" && !m->rom_file_is_label);
    const std::string remembered_path = m->rom_full;
    launcher_model_init(m.get(), &settings, &gi, remembered_path.c_str());
    CHECK(std::string(m->rom_file) == "test.gba");
    std::uint64_t legacy_hash = 14695981039346656037ull;
    for (unsigned char byte : bytes) legacy_hash = (legacy_hash ^ byte) * 1099511628211ull;
    const auto legacy = moved/"ROMs"/(std::to_string(legacy_hash) + ".gba");
    put(legacy, bytes);
    gi.rom_patch_required_sha1 = "different target";
    launcher_model_init(m.get(), &settings, &gi, legacy.string().c_str());
    CHECK(m->rom_file_is_label && std::string(m->rom_file) == "Synthetic Game (source ROM)");
    CHECK(fs::equivalent(m->rom_full, legacy) && get(legacy) == bytes);
    gi.rom_patch_required_sha1 = digest_hex;
    launcher_model_init(m.get(), &settings, &gi, legacy.string().c_str());
    CHECK(m->rom_file_is_label && std::string(m->rom_file) == "Synthetic Game (patched ROM)");
    synthetic_known[0] = "0000000000000000000000000000000000000000";
    launcher_model_init(m.get(), &settings, &gi, legacy.string().c_str());
    CHECK(!m->rom_file_is_label && !launcher_model_rom_verified(m.get()));
    synthetic_known[0] = digest_hex;
    put(moved/"ROMs/123456.gba", bytes);
    launcher_model_init(m.get(), &settings, &gi, (moved/"ROMs/123456.gba").string().c_str());
    CHECK(!m->rom_file_is_label && std::string(m->rom_file) == "123456.gba");
    gi.import_file = nullptr; gi.import_file_ctx = nullptr;
    gi.known_sha1_hex = nullptr; gi.num_known_sha1 = 0; gi.rom_patch_required_sha1 = nullptr;
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
        const std::string prepared = m->rom_patch_prepared_path;
        // Dual-engine opt-in: stock is now launchable, without weakening the
        // patch target gate. Import/Clear must follow the displayed language.
        const auto jp_save = (moved / "Saves/japanese.eep").string();
        const auto en_save = (moved / "Saves/battery.eep").string();
        put(jp_save, "japan"); put(en_save, "english");
        gi.rom_patch_allow_unpatched = 1;
        gi.unpatched_label = "Japanese"; gi.patched_label = "English";
        gi.sram_path = jp_save.c_str(); gi.patched_sram_path = en_save.c_str();
        launcher_model_init(m.get(), &settings, &gi, argv[1]);
        CHECK(launcher_model_can_launch(m.get()));
        CHECK(std::strcmp(m->rom_variant_label, "Japanese") == 0);
        CHECK(std::strcmp(m->sram_path, jp_save.c_str()) == 0);
        CHECK(launcher_model_prepare_rom_patch(m.get()));
        CHECK(!m->rom_patch_prepared_path[0]);
        launcher_model_set_rom_patch(m.get(), argv[2]);
        CHECK(std::strcmp(m->rom_variant_label, "English") == 0);
        CHECK(std::strcmp(m->sram_path, en_save.c_str()) == 0);
        launcher_model_toggle_rom_patch(m.get());
        CHECK(launcher_model_can_launch(m.get()));
        CHECK(std::strcmp(m->sram_path, jp_save.c_str()) == 0);
        launcher_model_toggle_rom_patch(m.get());
        CHECK(launcher_model_prepare_rom_patch(m.get()));
        CHECK(std::strcmp(m->rom_patch_prepared_sha1, known[1]) == 0);
        RecompLauncherCSettings selected_settings{};
        launcher_model_commit(m.get(), &selected_settings);
        CHECK(std::strcmp(selected_settings.selected_sram_path, en_save.c_str()) == 0);
        m->has_default_settings = true;
        m->default_settings.rom_patch_enabled = 0;
        m->default_settings.rom_patch_path[0] = '\0';
        launcher_model_restore_defaults(m.get());
        CHECK(std::strcmp(m->sram_path, jp_save.c_str()) == 0);
        CHECK(!m->rom_patch_prepared_path[0]);
        CHECK(launcher_model_can_launch(m.get()));
        launcher_model_clear_rom_patch(m.get());
        CHECK(std::strcmp(m->sram_path, jp_save.c_str()) == 0);
        // An already translated file stays English even when patching is off.
        launcher_model_set_rom(m.get(), prepared.c_str());
        CHECK(launcher_model_can_launch(m.get()));
        CHECK(std::strcmp(m->sram_path, en_save.c_str()) == 0);
        m->s.rom_patch_enabled = 1; m->s.rom_patch_path[0] = '\0';
        CHECK(launcher_model_can_launch(m.get()));
        CHECK(launcher_model_prepare_rom_patch(m.get()));
        CHECK(!m->rom_patch_prepared_path[0]);
        CHECK(get(jp_save) == "japan" && get(en_save) == "english");
        launcher_model_clear_rom_patch(m.get());
        launcher_model_set_rom(m.get(), argv[1]);
        // Valid IPS syntax but incompatible output must be rejected.
        const auto wrong = moved/"Mods/wrong.ips";
        put(wrong, std::string("PATCH\0\0\0\0\1XEOF", 14));
        launcher_model_set_rom_patch(m.get(), wrong.string().c_str());
        CHECK(!launcher_model_prepare_rom_patch(m.get()));
        std::cout << "PASS: Japanese unpatched, optional exact English BPS, direct English, separate save banks, incompatible IPS rejected\n";
    }
    std::cout << "PASS: portable import/relocation, bindings, defaults, battery safety, patch target gates\n";
    return 0;
}
