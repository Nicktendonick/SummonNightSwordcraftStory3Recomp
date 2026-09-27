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

#if defined(SWORDCRAFT3_RECOMP_UI)
#include "game_launcher_boot.h"
#endif

namespace {

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
    std::string original_credits, port_credits, tools_credits, launcher_folder_note; // owns launcher text until exit
    opts.builtin_game_name =
        "Summon Night: Craft Sword Monogatari - Hajimari no Ishi";
#if defined(SWORDCRAFT3_BETA_BUILD)
    opts.builtin_rom_sha1 = "bb2eebf98deb59bb6218442c2308bb5033ae2915";
    opts.builtin_rom_crc32 = 0xA8F22FCAu;
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
        "beta BPS. Select that patch and the verified Japanese ROM.";
    // Local native-launcher preview uses the already verified private beta ROM.
    // Keep legacy patch-import and automated command-line paths unchanged.
    if (const char* preview = std::getenv("SWORDCRAFT3_BETA_LAUNCHER")) {
        if (std::strcmp(preview, "1") == 0) {
            opts.builtin_game_name = "Summon Night: Swordcraft Story 3 - PC Beta";
            opts.data_root = std::getenv("SWORDCRAFT3_PORTABLE_ROOT");
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
            opts.launcher_save_path = "Saves/battery.eep";
            opts.save_state_basename = "Save States/beta";
            const char* art = std::getenv("SWORDCRAFT3_BETA_BOXART");
            opts.launcher_boxart = swordcraft3::beta_boxart_path(art ? art : "clean");
            opts.launcher_theme = "storybook";
            opts.ui_theme = "storybook";
            opts.ui_title = "Swordcraft Story 3";
            opts.ui_subtitle = "Craftknights! Artisans of weapons. Masters of the sword.";
            opts.ui_session_controls = true;
            opts.ui_pause_on_open = true;
            opts.ui_preferences_filename = "Settings/runtime-menu.ini";
#if defined(_WIN32)
            opts.ui_reset_exit_code = swordcraft3::beta_reset_exit_code;
#endif
            opts.launcher_logo = "assets/beta/launcher-logo.png";
            opts.launcher_backdrop = "assets/beta/launcher-village.png";
            opts.launcher_tagline = "Craftknights! Artisans of weapons. Masters of the sword.";
            opts.launcher_sidebar = true;
            launcher_folder_note = "Your saves, save states, imported game files, mods, settings, credits and captures "
                "stay beside the launcher. Move the complete folder to keep them together.\n\nFolder: ";
            launcher_folder_note += opts.data_root ? opts.data_root : ".";
            opts.launcher_settings_note = launcher_folder_note.c_str();
            std::fprintf(stderr,"[sc3:launcher] boxart=%s\n",opts.launcher_boxart);
        }
    }
#endif
    configure_swordcraft3_adaptive_widescreen(opts);
    configure_swordcraft3_custom_renderer(opts);
    configure_swordcraft3_display_settings(opts, argc > 0 ? argv[0] : nullptr);
    // Native game rendering stays 3:2, but the host window itself may be
    // reshaped freely and will letterbox/pillarbox as needed.
    opts.freely_resizable_window = true;
    opts.show_fps_by_default = true;

    opts.expose_assist_tools = true;
    opts.assist_tools_enabled_by_default = true;
    opts.assist_fast_forward_multiplier_default = 4;
    opts.save_state_slot_count = 10;
    opts.rewind_history_seconds = 10;
    opts.rewind_capture_interval_frames = 15;

#if defined(SWORDCRAFT3_RECOMP_UI)
    std::vector<std::string> args(argv, argv + argc);
    if (game_launcher_preboot(args, opts)) {
        return 0;
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
