#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "runtime.h"

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
    opts.builtin_rom_sha1 = "3f5253fcf57e07ce52472bd29a61d16b98a12376";
    opts.builtin_rom_crc32 = 0x12AFAE5Du;
    opts.mod_game_id = "summon-night-swordcraft-story-3-jp";
    opts.launcher_region = "Japan";
    opts.launcher_game_config = "game.toml";
    opts.launcher_expose_widescreen = false;
    opts.launcher_expose_adaptive_view = false;

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
