#pragma once

// A reset needs a new CPU, bus, audio state AND fresh game-owned statics.
// Keep one supervisor after the initial run; reset children never spawn their
// own children. The normal beta starter continues waiting for this process.
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include "../gbarecomp/src/runtime/presentation_preferences.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace swordcraft3 {
constexpr int beta_reset_exit_code = 200;

// Windows CRT argument quoting, including embedded quotes/trailing slashes.
inline std::wstring quote_process_arg(const std::wstring& value) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(slashes * (c == L'\"' ? 2 : 1), L'\\');
        if (c == L'\"') result += L'\\';
        result += c;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'\"';
}

inline std::vector<std::string> reset_arguments(const std::vector<std::string>& args) {
    std::vector<std::string> result;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--launcher" || args[i] == "--no-launcher") continue;
        // Reset always cold-boots, even if the original session used a state.
        if (args[i] == "--load-state") { if (i + 1 < args.size()) ++i; continue; }
        result.push_back(args[i]);
    }
    result.push_back("--no-launcher");
    return result;
}

// A live Esc-menu change has already been saved atomically to this INI, but
// the supervisor's original argv is stale. Refresh ONLY these five settings;
// ROM/BIOS/save/language and every other CLI option keep their resolved values.
// Require a complete valid snapshot: an old, missing, partial or malformed file
// must never turn a working reset command into different/default settings.
inline bool refresh_reset_presentation_arguments(std::vector<std::string>& args,
                                                  const std::filesystem::path& ini) {
    try {
        std::ifstream input(ini, std::ios::binary);
        if (!input) return false;
        const std::array<std::string, 5> keys = {"linear_filter", "sharp_filter", "smooth_filter",
                                                "screen_effect", "screen_effect_strength"};
        const std::array<std::string, 5> options = {"--linear-filter", "--sharp-filter", "--smooth-filter",
                                                   "--screen-effect", "--screen-effect-strength"};
        std::array<int, 5> values{};
        std::array<bool, 5> found{};
        const auto trim = [](const std::string& value) {
            const auto first = value.find_first_not_of(" \t\r");
            return first == std::string::npos ? std::string{} :
                value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
        };
        bool in_launcher = false, first_line = true;
        std::string line;
        size_t read_bytes = 0;
        while (std::getline(input, line)) {
            read_bytes += line.size();
            if (read_bytes > 1024 * 1024 || line.find('\0') != std::string::npos) return false;
            if (first_line && line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
            first_line = false;
            line = trim(line.substr(0, line.find_first_of(";#")));
            if (line.empty()) continue;
            if (line.front() == '[') {
                if (line.back() != ']') return false;
                in_launcher = line == "[Launcher]";
                continue;
            }
            if (!in_launcher) continue;
            const auto equal = line.find('=');
            if (equal == std::string::npos) continue;
            const auto key = trim(line.substr(0, equal));
            for (size_t index = 0; index < keys.size(); ++index) {
                if (keys[index] != key) continue;
                if (found[index]) return false;
                const auto text = trim(line.substr(equal + 1));
                const auto parsed = std::from_chars(text.data(), text.data() + text.size(), values[index]);
                if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return false;
                const int maximum = index < 3 ? 1 : index == 3 ? 2 : 100;
                if (values[index] < 0 || values[index] > maximum) return false;
                found[index] = true;
            }
        }
        if (input.bad() || !input.eof()) return false;
        for (bool present : found) if (!present) return false;
        if (values[0] + values[1] + values[2] > 1) return false;

        auto updated = args;
        updated.clear();
        for (size_t index = 0; index < args.size(); ++index) {
            bool consumed = false;
            for (const auto& option : options) {
                if (args[index] == option) {
                    if (index + 1 < args.size()) ++index;
                    consumed = true;
                    break;
                }
                if (args[index].rfind(option + "=", 0) == 0) { consumed = true; break; }
            }
            if (!consumed) updated.push_back(args[index]);
        }
        for (size_t index = 0; index < options.size(); ++index) {
            updated.push_back(options[index]);
            updated.push_back(index == 3 ? (values[index] == 1 ? "lcd" : values[index] == 2 ? "crt" : "off")
                                         : std::to_string(values[index]));
        }
        args.swap(updated);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool refresh_reset_host_aspect_arguments(std::vector<std::string>& args, const std::filesystem::path& ini) {
    const auto index = gbarecomp::read_host_aspect_preference(ini, 3);
    if (!index) return false;
    std::vector<std::string> next;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--host-aspect") { if (i + 1 < args.size()) ++i; continue; }
        if (args[i].rfind("--host-aspect=", 0) == 0) continue;
        next.push_back(args[i]);
    }
    next.push_back("--host-aspect"); next.push_back(std::to_string(*index));
    args.swap(next);
    return true;
}

inline int supervise_beta_resets(int result, const std::vector<std::string>& resolved_args) {
#if defined(_WIN32)
    if (std::getenv("SWORDCRAFT3_RESET_CHILD")) return result;
    std::vector<wchar_t> module(32768);
    const DWORD n = GetModuleFileNameW(nullptr, module.data(), DWORD(module.size()));
    if (!n || n >= module.size()) return result == beta_reset_exit_code ? 1 : result;
    const std::wstring exe(module.data(), n);
    const char* original_trace = std::getenv("GBARECOMP_INPUT_RECORD");
    const std::string trace = original_trace ? original_trace : "";
    unsigned reset_count = 0;
    while (result == beta_reset_exit_code) {
        ++reset_count;
        auto args = reset_arguments(resolved_args);
        const char* portable_mode = std::getenv("SWORDCRAFT3_BETA_LAUNCHER");
        if (portable_mode && std::string(portable_mode) == "1") {
            // Use the native Unicode environment path, not the narrow argv
            // conversion used by the existing game engine.
            std::array<wchar_t, 32768> portable_root{};
            const DWORD root_size = GetEnvironmentVariableW(L"SWORDCRAFT3_PORTABLE_ROOT",
                portable_root.data(), static_cast<DWORD>(portable_root.size()));
            if (root_size > 0 && root_size < portable_root.size()) {
                refresh_reset_presentation_arguments(args,
                    std::filesystem::path(portable_root.data()) / "Settings/launcher.ini");
                refresh_reset_host_aspect_arguments(args,
                    std::filesystem::path(portable_root.data()) / "Settings/launcher.ini");
            }
        }
        std::wstring command = quote_process_arg(exe);
        for (size_t i = 1; i < args.size(); ++i) {
            // Runtime/launcher paths currently use the host narrow code page.
            const int count = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, args[i].c_str(), -1, nullptr, 0);
            if (!count) { std::fprintf(stderr, "[sc3:session] Cannot encode reset argument.\n"); return 1; }
            std::vector<wchar_t> wide(count);
            MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, args[i].c_str(), -1, wide.data(), count);
            command += L" " + quote_process_arg(wide.data());
        }
        SetEnvironmentVariableW(L"SWORDCRAFT3_RESET_CHILD", L"1");
        if (!trace.empty()) {
            const auto next_trace = trace + ".reset-" + std::to_string(reset_count);
            SetEnvironmentVariableA("GBARECOMP_INPUT_RECORD", next_trace.c_str());
        }
        // Diagnostic scripts must not repeat a reset forever in the new boot.
        // A separately supplied post-reset script can verify the new session.
        const char* after = std::getenv("GBARECOMP_ASSIST_SCRIPT_AFTER_RESET");
        SetEnvironmentVariableA("GBARECOMP_ASSIST_SCRIPT", after && *after ? after : nullptr);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION child{};
        std::fprintf(stderr, "[sc3:session] Clean reset: booting the same resolved ROM/BIOS/save in a fresh process.\n");
        std::fflush(nullptr);
        const BOOL ok = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                                       CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child);
        const DWORD launch_error = GetLastError();
        SetEnvironmentVariableW(L"SWORDCRAFT3_RESET_CHILD", nullptr);
        if (!ok) { std::fprintf(stderr, "[sc3:session] Reset launch failed: %lu\n", launch_error); return 1; }
        CloseHandle(child.hThread);
        WaitForSingleObject(child.hProcess, INFINITE);
        DWORD exit_code = 1;
        GetExitCodeProcess(child.hProcess, &exit_code);
        CloseHandle(child.hProcess);
        result = int(exit_code);
    }
#endif
    return result;
}
} // namespace swordcraft3
