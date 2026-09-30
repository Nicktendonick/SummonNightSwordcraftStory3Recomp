#pragma once

#include "../gbarecomp/src/runtime/presentation_preferences.h"
#include <stdexcept>

namespace swordcraft3 {
// Game-owned preference; separate from launcher.ini so launcher/defaults and
// live presentation writes cannot accidentally remove the user's mod choice.
// Loaded once before preboot, not polled per frame. No environment override in
// portable mode: an explicit Off must also beat the retired Guard starter.
class GuardPreferences {
public:
    std::filesystem::path path;
    bool enabled = false;
    std::string error;

    void load(const std::filesystem::path& root) {
        path = root / "Settings/guard.ini";
        enabled = false;
        error.clear();
        try {
            if (!std::filesystem::exists(path)) return;
            if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) > 65536)
                throw std::runtime_error("Invalid Guard settings file.");
            std::ifstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("Cannot read Guard settings.");
            bool section = false, found = false, value = false, first = true;
            std::string line;
            while (std::getline(file, line)) {
                if (first && line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
                first = false;
                if (line.find('\0') != std::string::npos) throw std::runtime_error("Invalid Guard settings.");
                line = gbarecomp::presentation_preferences_detail::trim(line);
                line = gbarecomp::presentation_preferences_detail::trim(line.substr(0, line.find_first_of(";#")));
                if (line.empty() || line[0] == ';' || line[0] == '#') continue;
                if (line[0] == '[') { section = line == "[Launcher]"; continue; }
                const auto eq = line.find('=');
                if (!section || eq == std::string::npos ||
                    gbarecomp::presentation_preferences_detail::trim(line.substr(0, eq)) != "select_guard") continue;
                const auto text = gbarecomp::presentation_preferences_detail::trim(line.substr(eq + 1));
                if (found || (text != "0" && text != "1"))
                    throw std::runtime_error("Invalid Guard setting. Expected select_guard = 0 or 1.");
                found = true;
                value = text == "1";
            }
            if (file.bad()) throw std::runtime_error("Cannot read Guard settings.");
            if (!found) throw std::runtime_error("Guard setting is missing.");
            enabled = value;
        } catch (const std::exception& e) {
            error = std::string(e.what()) + " Guard is off.";
        }
    }

    bool save(bool value) {
        // Do not report success for a file that would fail on the next boot.
        // A malformed file requires the user to repair it, not silent erasure.
        if (!path.empty()) {
            GuardPreferences current;
            current.load(path.parent_path().parent_path());
            if (!current.error.empty()) {
                error = current.error + " Repair Settings/guard.ini before changing this option.";
                return false;
            }
        }
        // Shared atomic INI writer preserves unrelated keys/comments/sections.
        if (path.empty() || !gbarecomp::update_launcher_preferences(path, {"select_guard"}, {value ? 1 : 0})) {
            error = "Could not save Settings/guard.ini. Check folder permissions. Previous choice retained.";
            return false;
        }
        enabled = value;
        error.clear();
        return true;
    }
    static int get(void* ctx) { return static_cast<GuardPreferences*>(ctx)->enabled; }
    static int set(void* ctx, int value) { return static_cast<GuardPreferences*>(ctx)->save(value != 0); }
    static const char* last_error(void* ctx) { return static_cast<GuardPreferences*>(ctx)->error.c_str(); }
};
} // namespace swordcraft3
