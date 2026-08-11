#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "runtime.h"
#include "adaptive_widescreen.h"

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
#endif
    configure_swordcraft3_adaptive_widescreen(opts);
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
    return gbarecomp::run_game(
        static_cast<int>(forwarded.size()), forwarded.data(), opts);
#else
    return gbarecomp::run_game(argc, argv, opts);
#endif
}
