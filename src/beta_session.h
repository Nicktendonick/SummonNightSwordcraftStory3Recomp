#pragma once

// A reset needs a new CPU, bus, audio state AND fresh game-owned statics.
// Keep one supervisor after the initial run; reset children never spawn their
// own children. The normal beta starter continues waiting for this process.
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
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

inline int supervise_beta_resets(int result, const std::vector<std::string>& resolved_args) {
#if defined(_WIN32)
    if (std::getenv("SWORDCRAFT3_RESET_CHILD")) return result;
    auto args = reset_arguments(resolved_args);
    std::vector<wchar_t> module(32768);
    const DWORD n = GetModuleFileNameW(nullptr, module.data(), DWORD(module.size()));
    if (!n || n >= module.size()) return result == beta_reset_exit_code ? 1 : result;
    const std::wstring exe(module.data(), n);
    const char* original_trace = std::getenv("GBARECOMP_INPUT_RECORD");
    const std::string trace = original_trace ? original_trace : "";
    unsigned reset_count = 0;
    while (result == beta_reset_exit_code) {
        ++reset_count;
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
