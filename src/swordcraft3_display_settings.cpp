#include "swordcraft3_display_settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "recomp_runtime_ui.h"
#include "runtime.h"

namespace {
constexpr const char* kKey = "swordcraft3.battle_hud_borders";
bool s_enabled = true;
bool s_session_only = false;
std::filesystem::path s_config;

const RecompRuntimeUiItem kItems[] = {{
    kKey, "Display", "Battle HUD borders",
    "Experimental: extend the battle HUD background and separators into the "
    "wide margins. Keeps the original HUD and gameplay unchanged. Off restores "
    "the previous margins; unrecognized layouts are left alone.",
    RECOMP_RUNTIME_UI_BOOL, 0, 1, 1, nullptr, 0, nullptr
}};

int get_setting(const char* key, int* out) {
    if (!key || !out || std::strcmp(key, kKey) != 0) return 0;
    *out = s_enabled ? 1 : 0;
    return 1;
}

int set_setting(const char* key, int value) {
    if (!key || std::strcmp(key, kKey) != 0 || (value != 0 && value != 1))
        return 0;
    s_enabled = value != 0;
    // Environment overrides are session-only escape hatches / deterministic
    // audit inputs. Do not overwrite the player's remembered preference.
    if (!s_session_only && !s_config.empty()) {
        std::ofstream file(s_config, std::ios::trunc);
        file << "[display]\nbattle_hud_borders=" << value << '\n';
        file.flush();
        if (!file) std::fprintf(stderr,
            "[swordcraft3:display] Could not save Battle HUD borders; "
            "the change applies to this session only.\n");
    }
    return 1;
}
}  // namespace

void configure_swordcraft3_display_settings(gbarecomp::RunOptions& opts,
                                          const char* executable_path) {
    s_enabled = true;
    s_session_only = false;
    s_config.clear();
    if (executable_path && *executable_path) {
        std::error_code error;
        auto executable = std::filesystem::absolute(executable_path, error);
        if (!error) s_config = executable.parent_path() / "swordcraft3-display.ini";
    }
    std::ifstream file(s_config);
    std::string line;
    bool display_section = false;
    while (std::getline(file, line)) {
        // Accept ordinary INI whitespace/CRLF, but never partially parse an
        // invalid value such as "10" as true or mutate another config file.
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;
        line = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
        if (line[0] == '[') display_section = line == "[display]";
        if (!display_section) continue;
        auto equal = line.find('=');
        if (equal == std::string::npos) continue;
        auto key = line.substr(0, equal);
        key.erase(key.find_last_not_of(" \t") + 1);
        auto value = line.substr(equal + 1);
        const auto begin = value.find_first_not_of(" \t");
        if (begin != std::string::npos) value.erase(0, begin);
        if (key == "battle_hud_borders" && (value == "0" || value == "1"))
            s_enabled = value == "1";
    }
    if (const char* env = std::getenv("SWORDCRAFT3_BATTLE_HUD_BORDERS")) {
        if (std::strcmp(env, "0") == 0 || std::strcmp(env, "1") == 0) {
            s_enabled = env[0] == '1';
            s_session_only = true;
        } else if (*env) {
            std::fprintf(stderr,
                "[swordcraft3:display] Ignoring invalid "
                "SWORDCRAFT3_BATTLE_HUD_BORDERS (use 0 or 1).\n");
        }
    }
    opts.ui_extra_items = kItems;
    opts.ui_extra_item_count = sizeof(kItems) / sizeof(kItems[0]);
    opts.ui_get = get_setting;
    opts.ui_set = set_setting;
}

bool swordcraft3_battle_hud_borders_enabled() { return s_enabled; }
