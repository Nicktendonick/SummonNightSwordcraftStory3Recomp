#pragma once

// Static recompilation has one instruction corpus per ROM revision. Resolve
// actual bytes before choosing a sibling engine; never trust a filename or a
// user-supplied --rom-sha1 to decide which compiled code can execute them.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "sha1.h"
#include "beta_session.h"

namespace swordcraft3 {
inline constexpr const char* japanese_sha1 = "3f5253fcf57e07ce52472bd29a61d16b98a12376";
#if defined(SWORDCRAFT3_RELEASE106_TRANSLATION)
inline constexpr const char* english_sha1 = "6753a22a096b8adaa3a869333b99fcfe29ba1fec";
inline constexpr const char* english_crc32 = "0xC76631A9";
inline constexpr std::uint32_t english_crc32_value = 0xC76631A9u;
inline constexpr const char* english_label = "English (translation 1.0.6.f)";
inline constexpr const char* english_patch_name = "English 1.0.6.f BPS";
#elif defined(SWORDCRAFT3_RELEASE105_TRANSLATION)
inline constexpr const char* english_sha1 = "06a9f4db52f40a7034dc1c74161a705f30edb858";
inline constexpr const char* english_crc32 = "0xCC25DCDC";
inline constexpr std::uint32_t english_crc32_value = 0xCC25DCDCu;
inline constexpr const char* english_label = "English (translation 1.0.5.f)";
inline constexpr const char* english_patch_name = "English 1.0.5.f BPS";
#else
inline constexpr const char* english_sha1 = "bb2eebf98deb59bb6218442c2308bb5033ae2915";
inline constexpr const char* english_crc32 = "0xA8F22FCA";
inline constexpr std::uint32_t english_crc32_value = 0xA8F22FCAu;
inline constexpr const char* english_label = "English (translation)";
inline constexpr const char* english_patch_name = "English beta BPS";
#endif
struct PortableLanguage {
    const char* label;
    const char* sha1;
    const char* crc32;
    const char* executable;
    const char* save;
    const char* states;
};
inline constexpr PortableLanguage japanese_language{
    "Japanese (original)", japanese_sha1, "0x12AFAE5D", "Swordcraft3Japanese.exe",
    "Saves/japanese.eep", "Save States/japanese"};
inline constexpr PortableLanguage english_language{
    english_label, english_sha1, english_crc32, "Swordcraft3CustomRendererBeta.exe",
#if defined(SWORDCRAFT3_RELEASE106_TRANSLATION)
    "Saves/english-1.0.6f.eep", "Save States/english-1.0.6f"};
#elif defined(SWORDCRAFT3_RELEASE105_TRANSLATION)
    "Saves/english-1.0.5f.eep", "Save States/english-1.0.5f"};
#else
    "Saves/battery.eep", "Save States/beta"};
#endif

inline const PortableLanguage& language_for_rom(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path) != 0x02000000)
        throw std::runtime_error("Unsupported ROM size. Select the verified Japanese ROM or supported English translation.");
    std::ifstream input(path, std::ios::binary);
    std::vector<char> bytes(0x02000000);
    if (!input.read(bytes.data(), bytes.size())) throw std::runtime_error("Cannot read selected ROM.");
    const auto hash = gba::sha1(bytes.data(), bytes.size()).hex();
    if (hash == japanese_sha1) return japanese_language;
    if (hash == english_sha1) return english_language;
    throw std::runtime_error(std::string("Unsupported ROM revision. This package supports the original Japanese ROM and the matching ") + english_patch_name + " only.");
}

inline std::string argument_value(const std::vector<std::string>& args, const char* key) {
    std::string result;
    for (size_t i = 1; i + 1 < args.size(); ++i)
        if (args[i] == key) result = args[++i];
    return result;
}

inline void replace_argument(std::vector<std::string>& args, const char* key, const std::string& value) {
    std::vector<std::string> clean;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0 && args[i] == key) { if (i + 1 < args.size()) ++i; }
        else clean.push_back(args[i]);
    }
    clean.push_back(key); clean.push_back(value); args.swap(clean);
}

inline const PortableLanguage& resolve_portable_language(std::vector<std::string>& args,
                                                         const std::filesystem::path& root) {
    std::filesystem::path rom = argument_value(args, "--rom");
    if (rom.empty()) {
        std::string cached;
        std::getline(std::ifstream(root / "Settings/rom.cfg"), cached);
        if (!cached.empty() && cached.back() == '\r') cached.pop_back();
        if (cached.empty()) throw std::runtime_error("No ROM selected. Open the launcher and select your game.");
        rom = cached;
        if (rom.is_relative()) rom = root / "Settings" / rom;
    }
    rom = std::filesystem::canonical(rom);
    const auto& language = language_for_rom(rom);
    replace_argument(args, "--rom", rom.string());
    replace_argument(args, "--rom-sha1", language.sha1);
    replace_argument(args, "--rom-crc32", language.crc32);
    if (argument_value(args, "--save").empty())
        replace_argument(args, "--save", (root / language.save).string());
    std::fprintf(stderr, "[sc3:language] %s; engine=%s; save=%s; states=%s\n",
        language.label, language.executable, argument_value(args, "--save").c_str(), language.states);
    return language;
}

// Keep the starter waiting for the real game, including reset/error status.
inline int run_language_engine(const PortableLanguage& language, const std::vector<std::string>& args) {
#if defined(_WIN32)
    std::vector<wchar_t> module(32768);
    const auto n = GetModuleFileNameW(nullptr, module.data(), DWORD(module.size()));
    if (!n || n >= module.size()) throw std::runtime_error("Cannot locate language engine.");
    const auto exe = std::filesystem::path(std::wstring(module.data(), n)).parent_path() / language.executable;
    if (!std::filesystem::is_regular_file(exe))
        throw std::runtime_error("Missing language engine. Extract the complete portable release: " + exe.string());
    std::wstring command = quote_process_arg(exe.wstring());
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--launcher" || args[i] == "--no-launcher") continue;
        const int count = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, args[i].c_str(), -1, nullptr, 0);
        if (!count) throw std::runtime_error("Cannot encode language-engine argument.");
        std::vector<wchar_t> wide(count);
        MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, args[i].c_str(), -1, wide.data(), count);
        command += L" " + quote_process_arg(wide.data());
    }
    command += L" --no-launcher";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION child{};
    std::fflush(nullptr);
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child))
        throw std::runtime_error("Could not start language engine. Windows error " + std::to_string(GetLastError()));
    CloseHandle(child.hThread);
    WaitForSingleObject(child.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(child.hProcess, &code); CloseHandle(child.hProcess);
    return int(code);
#else
    throw std::runtime_error("Portable language handoff is currently supported on Windows only.");
#endif
}
} // namespace swordcraft3
