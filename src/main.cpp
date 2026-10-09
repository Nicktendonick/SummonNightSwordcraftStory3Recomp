#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

#include "runtime.h"
#include "adaptive_widescreen.h"
#include "custom_renderer.h"
#include "swordcraft3_display_settings.h"
#include "beta_boxart.h"
#include "beta_credits.h"
#include "beta_session.h"
#include "portable_language.h"
#include "guard_preferences.h"
#include "battle_camera_preferences.h"

#if defined(SWORDCRAFT3_RECOMP_UI)
#include "game_launcher_boot.h"
#include "recomp_launcher.h"
#include "graphics_presets.h"
#include "recomp_runtime_ui.h"
#endif

namespace {
swordcraft3::BattleCameraPreferences battle_camera_preferences;
int (*previous_ui_get)(const char*,int*)=nullptr;
int (*previous_ui_set)(const char*,int)=nullptr;
int camera_get(const char* key,int* value) {
    if(!key || !value) return 0;
    if(!std::strcmp(key,"swordcraft3.battle_edge_cover")) { *value=battle_camera_preferences.cover_edges; return 1; }
    if(std::strcmp(key,"swordcraft3.battle_camera")) return previous_ui_get?previous_ui_get(key,value):0;
    *value=battle_camera_preferences.mode; return 1;
}
int camera_set(const char* key,int value) {
    if(!key) return 0;
    if(!std::strcmp(key,"swordcraft3.battle_edge_cover")) return battle_camera_preferences.save_cover(value);
    if(std::strcmp(key,"swordcraft3.battle_camera")) return previous_ui_set?previous_ui_set(key,value):0;
    if(value<0 || value>2) return 0;
    const bool ok=battle_camera_preferences.save(value);
    if(!ok) std::fprintf(stderr,"[sc3:battle-camera] %s\n",battle_camera_preferences.error.c_str());
    return ok;
}
int camera_enabled(const char* key) {
    return !key || std::strcmp(key, "swordcraft3.battle_edge_cover") ||
           battle_camera_preferences.mode == 2;
}

void print_usage() {
    std::printf(
        "SummonNightSwordcraftStory3Recomp [--bios <path>] [--rom <path>] "
        "[game.toml]\n\n"
        "A legally dumped Japanese ROM and verified GBA BIOS are required.\n"
        "Windowed play opens the recomp-ui launcher; --no-launcher bypasses it.\n");
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    gbarecomp::RunOptions opts;
    swordcraft3::GuardPreferences guard_preferences;
#if defined(SWORDCRAFT3_RECOMP_UI)
    static const char* const camera_choices[]{"Current", "Bounded", "Follow + edge stops"};
    RecompLauncherCBuiltinMod built_in_mods[]{ {
        "select-guard", "Hold Select to Guard",
        "Hold Select to guard during manual combat; release to stop. Your R-slot ability stays selected. "
        "Replaces auto-battle in combat only. Off restores the original controls.",
        &guard_preferences, swordcraft3::GuardPreferences::get,
        swordcraft3::GuardPreferences::set, swordcraft3::GuardPreferences::last_error},
        {"battle-camera", "Battle framing",
         "Experimental: Current keeps the existing view. Bounded frames inward. Follow + edge stops tracks within the scenery. "
         "Reviewed widescreen battles only; field maps and unsupported scenes are unchanged.",
         &battle_camera_preferences,swordcraft3::BattleCameraPreferences::get,
         swordcraft3::BattleCameraPreferences::set,swordcraft3::BattleCameraPreferences::last_error,
         "Graphics", camera_choices, 3, nullptr},
        {"battle-scenery-edge-cover", "Cover scenery edges",
         "Experimental: covers the rocky arena's scenery cutoffs. Requires Follow + edge stops; other arenas are unchanged.",
         &battle_camera_preferences,swordcraft3::BattleCameraPreferences::get_cover,
         swordcraft3::BattleCameraPreferences::set_cover,swordcraft3::BattleCameraPreferences::last_error,
         "Graphics", nullptr, 0, [](void* p) { return static_cast<swordcraft3::BattleCameraPreferences*>(p)->mode == 2 ? 1 : 0; }},
        {"battle-hud-borders", "Battle HUD borders",
         "Extend the battle HUD's decorative borders into the widescreen margins.",
         nullptr,
         [](void*) { int value=0; camera_get("swordcraft3.battle_hud_borders", &value); return value; },
         [](void*, int value) { return camera_set("swordcraft3.battle_hud_borders", value); },
         nullptr, "Graphics", nullptr, 0, nullptr}};
    RecompRuntimeUiItem camera_item{};
    camera_item.key="swordcraft3.battle_camera"; camera_item.section="Graphics";
    camera_item.label="Battle framing";
    camera_item.description=built_in_mods[1].description;
    camera_item.group="Battle View";
    camera_item.type=RECOMP_RUNTIME_UI_CHOICE; camera_item.maximum=2; camera_item.step=1;
    camera_item.choices=camera_choices; camera_item.choice_count=3;
    RecompRuntimeUiItem cover_item{};
    cover_item.key="swordcraft3.battle_edge_cover"; cover_item.section="Graphics";
    cover_item.label="Cover scenery edges";
    cover_item.description=built_in_mods[2].description;
    cover_item.group="Battle View";
    cover_item.type=RECOMP_RUNTIME_UI_BOOL; cover_item.maximum=1; cover_item.step=1;
#endif
    std::string original_credits, port_credits, tools_credits, launcher_folder_note; // owns launcher text until exit
    opts.builtin_game_name =
        "Summon Night: Craft Sword Monogatari - Hajimari no Ishi";
#if defined(SWORDCRAFT3_BETA_BUILD)
    opts.builtin_rom_sha1 = swordcraft3::english_sha1;
    opts.builtin_rom_crc32 = swordcraft3::english_crc32_value;
    opts.launcher_source_rom_sha1 =
        "3f5253fcf57e07ce52472bd29a61d16b98a12376";
    opts.launcher_required_patch_sha1 = opts.builtin_rom_sha1;
#else
    opts.builtin_rom_sha1 = "3f5253fcf57e07ce52472bd29a61d16b98a12376";
    opts.builtin_rom_crc32 = 0x12AFAE5Du;
#endif
    opts.mod_game_id = "summon-night-swordcraft-story-3-jp";
    opts.launcher_region = "Japan";
    opts.launcher_game_config = "game.toml";
    opts.launcher_enable_rom_patches = true;
    opts.launcher_rom_patch_note =
        "Apply an IPS, IPS32, or BPS translation/mod patch to the verified "
        "Japanese ROM. Original files stay unchanged. Patches that alter "
        "executable code may require a matching static recompilation.";
#if defined(SWORDCRAFT3_BETA_BUILD)
    opts.launcher_config_filename = "config-beta.ini";
    opts.launcher_rom_cache_filename = "rom-beta.cfg";
    opts.launcher_rom_patch_note =
        "This executable is statically recompiled for the supplied English "
#if defined(SWORDCRAFT3_RELEASE106_TRANSLATION)
        "1.0.6.f BPS. Select that patch and the verified Japanese ROM.";
#elif defined(SWORDCRAFT3_RELEASE105_TRANSLATION)
        "1.0.5.f BPS. Select that patch and the verified Japanese ROM.";
#else
        "beta BPS. Select that patch and the verified Japanese ROM.";
#endif
#endif
    // Local native-launcher preview uses the already verified private beta ROM.
    // Keep legacy patch-import and automated command-line paths unchanged.
    if (const char* preview = std::getenv("SWORDCRAFT3_BETA_LAUNCHER")) {
        if (std::strcmp(preview, "1") == 0) {
#if defined(SWORDCRAFT3_RELEASE105_TRANSLATION)
            opts.builtin_game_name = "Summon Night: Swordcraft Story 3 - PC Port";
#else
            opts.builtin_game_name = "Summon Night: Swordcraft Story 3 - PC Beta";
#endif
            opts.data_root = std::getenv("SWORDCRAFT3_PORTABLE_ROOT");
            if (opts.data_root && *opts.data_root) {
#if defined(_WIN32)
                const wchar_t* guard_root = _wgetenv(L"SWORDCRAFT3_PORTABLE_ROOT");
                guard_preferences.load(guard_root ? std::filesystem::path(guard_root)
                                                  : std::filesystem::path(opts.data_root));
#else
                guard_preferences.load(std::filesystem::path(opts.data_root));
#endif
                battle_camera_preferences.load(guard_preferences.path.parent_path().parent_path());
#if defined(SWORDCRAFT3_RECOMP_UI)
                opts.launcher_builtin_mods = built_in_mods;
                opts.launcher_builtin_mod_count = sizeof(built_in_mods)/sizeof(built_in_mods[0]);
#endif
            }
            opts.launcher_source_rom_sha1 = swordcraft3::japanese_sha1;
            opts.launcher_required_patch_sha1 = swordcraft3::english_sha1;
            opts.launcher_allow_unpatched = true;
            opts.launcher_unpatched_label = swordcraft3::japanese_language.label;
            opts.launcher_patched_label = swordcraft3::english_language.label;
            opts.launcher_rom_patch_note =
#if defined(SWORDCRAFT3_RELEASE106_TRANSLATION)
                "Japanese works without a patch. English uses the released translation 1.0.6.f. "
                "Select its BPS patch and enable it here; disable it to return to Japanese. "
                "Older English patches are not supported by this package. "
                "English 1.0.6.f has separate save files; older saves remain untouched. "
                "Save states from older revisions are not automatically converted.";
#elif defined(SWORDCRAFT3_RELEASE105_TRANSLATION)
                "Japanese works without a patch. English uses the released translation 1.0.5.f. "
                "Select its BPS patch and enable it here; disable it to return to Japanese. "
                "The old English beta patch is not supported by this package. "
                "Released-English saves are separate; old beta saves remain untouched. "
                "Do not load old beta save states; battery-save compatibility is not established.";
#else
                "Japanese works without a patch. To play in English, select the supported "
                "English beta BPS and enable it here. Disable it to return to Japanese. "
                "Already-translated ROMs play in English directly; choose the original ROM "
                "to return to Japanese. Each language has separate battery saves and save states.";
#endif
            const auto credits_root = std::filesystem::path(opts.data_root ? opts.data_root : ".") / "Credits";
            original_credits = swordcraft3::read_beta_credits(credits_root / "original-game.txt", "Pending A");
            port_credits = swordcraft3::read_beta_credits(credits_root / "pc-port.txt", "Pending B");
            opts.launcher_credits_title = "Original Game Credits";
            opts.launcher_credits_text = original_credits.c_str();
            opts.launcher_credits_secondary_title = "PC Port Credits";
            opts.launcher_credits_secondary_text = port_credits.c_str();
            tools_credits = swordcraft3::read_beta_credits(credits_root / "tools-and-projects.txt",
                "Tools list unavailable. Restore Credits/tools-and-projects.txt from the portable package.");
            opts.launcher_credits_tools_text = tools_credits.c_str();
            opts.launcher_hybrid_input = true;
            opts.launcher_battery_save_size = 8192;
            opts.launcher_config_filename = "Settings/launcher.ini";
            opts.launcher_rom_cache_filename = "Settings/rom.cfg";
            opts.launcher_bios_cache_filename = "Settings/bios.cfg";
            opts.launcher_keybinds_filename = "Settings/keybinds.ini";
            opts.launcher_save_path = swordcraft3::japanese_language.save;
            opts.launcher_patched_save_path = swordcraft3::english_language.save;
            opts.save_state_basename = "Save States/beta";
            const char* art = std::getenv("SWORDCRAFT3_BETA_BOXART");
            opts.launcher_boxart = swordcraft3::beta_boxart_path(art ? art : "clean");
            opts.launcher_theme = "storybook";
            opts.ui_theme = "storybook";
            opts.ui_title = "Swordcraft Story 3";
            opts.ui_subtitle = "Craftknights!\nArtisans of weapons!\nMasters of the sword!";
            opts.ui_session_controls = true;
            opts.ui_pause_on_open = true;
            opts.ui_preferences_filename = "Settings/runtime-menu.ini";
#if defined(_WIN32)
            opts.ui_reset_exit_code = swordcraft3::beta_reset_exit_code;
#endif
            opts.launcher_logo = "assets/beta/launcher-logo.png";
            opts.launcher_backdrop = "assets/beta/launcher-village.png";
            opts.launcher_tagline = opts.ui_subtitle;
            opts.launcher_sidebar = true;
            opts.launcher_organized_settings = true;
            launcher_folder_note = "Your saves, save states, imported game files, mods, settings, credits and captures "
                "stay beside the launcher. Move the complete folder to keep them together.\n\nFolder: ";
            launcher_folder_note += opts.data_root ? opts.data_root : ".";
            opts.launcher_settings_note = launcher_folder_note.c_str();
            std::fprintf(stderr,"[sc3:launcher] boxart=%s\n",opts.launcher_boxart);
        }
    }
    configure_swordcraft3_adaptive_widescreen(opts);
    configure_swordcraft3_custom_renderer(opts);
    configure_swordcraft3_display_settings(opts, argc > 0 ? argv[0] : nullptr);
#if defined(SWORDCRAFT3_RECOMP_UI)
    // Preserve the existing game-owned display entries and callbacks.
    std::vector<RecompRuntimeUiItem> game_menu_items;
    if(!battle_camera_preferences.path.empty()) {
        const auto* prior=static_cast<const RecompRuntimeUiItem*>(opts.ui_extra_items);
        if(prior) game_menu_items.assign(prior,prior+opts.ui_extra_item_count);
        for (auto& item : game_menu_items) {
            if (item.key && !std::strcmp(item.key, "swordcraft3.battle_hud_borders")) {
                item.section = "Graphics";
                item.group = "Battle View";
                item.description = built_in_mods[3].description;
            }
        }
        game_menu_items.insert(game_menu_items.begin(), cover_item);
        game_menu_items.insert(game_menu_items.begin(), camera_item);
        previous_ui_get=opts.ui_get; previous_ui_set=opts.ui_set;
        opts.ui_extra_items=game_menu_items.data(); opts.ui_extra_item_count=game_menu_items.size();
        opts.ui_get=camera_get; opts.ui_set=camera_set;
        opts.ui_enabled=camera_enabled;
    }
#endif
    // Native game rendering stays 3:2, but the host window itself may be
    // reshaped freely and will letterbox/pillarbox as needed.
    opts.freely_resizable_window = true;
    opts.show_fps_by_default = true;
    opts.launcher_expose_sharp_filter = true;
    opts.launcher_expose_smooth_filter = true;
    opts.launcher_expose_screen_effects = true;

    opts.expose_assist_tools = true;
    opts.assist_tools_enabled_by_default = true;
    opts.assist_fast_forward_multiplier_default = 4;
    opts.save_state_slot_count = 10;
    opts.rewind_history_seconds = 10;
    opts.rewind_capture_interval_frames = 15;

#if defined(SWORDCRAFT3_RECOMP_UI)
    std::vector<std::string> args(argv, argv + argc);
    opts.graphics_presets = swordcraft3::graphics_presets;
    opts.graphics_preset_count = swordcraft3::graphics_preset_count;
    if (game_launcher_preboot(args, opts)) {
        return 0;
    }
    if (!guard_preferences.path.empty()) {
        set_swordcraft3_select_guard(guard_preferences.enabled);
        if (!guard_preferences.error.empty())
            std::fprintf(stderr, "[sc3:guard] %s\n", guard_preferences.error.c_str());
    }
    if(!battle_camera_preferences.path.empty()) {
        set_swordcraft3_battle_camera_mode(battle_camera_preferences.mode);
        set_swordcraft3_battle_edge_cover(battle_camera_preferences.cover_edges);
        if(!battle_camera_preferences.error.empty())
            std::fprintf(stderr,"[sc3:battle-camera] %s\n",battle_camera_preferences.error.c_str());
    }
    if (opts.launcher_allow_unpatched) {
        try {
            if (!opts.data_root || !*opts.data_root)
                throw std::runtime_error("Portable data folder is missing. Start with Swordcraft Story 3 Beta.exe.");
            const auto& language = swordcraft3::resolve_portable_language(args, opts.data_root);
            if (std::strcmp(language.sha1, opts.builtin_rom_sha1) != 0)
                return swordcraft3::run_language_engine(language, args);
            opts.save_state_basename = language.states;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[sc3:language] %s\n", error.what());
            return 1;
        }
    }
    std::vector<char*> forwarded;
    forwarded.reserve(args.size());
    for (auto& arg : args) {
        forwarded.push_back(arg.data());
    }
    const int result = gbarecomp::run_game(
        static_cast<int>(forwarded.size()), forwarded.data(), opts);
    return opts.ui_reset_exit_code ? swordcraft3::supervise_beta_resets(result, args) : result;
#else
    return gbarecomp::run_game(argc, argv, opts);
#endif
}
