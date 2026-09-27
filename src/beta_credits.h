#pragma once
#include <filesystem>
#include <fstream>
#include <string>

namespace swordcraft3 {
// UTF-8 plain text, loaded once per launcher opening. Never rewrites user text.
inline std::string read_beta_credits(const std::filesystem::path& file,
                                     const char* fallback) {
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    if (!input) return fallback;
    const auto size = input.tellg();
    if (size <= 0 || size > 1024 * 1024) return fallback;
    std::string text(static_cast<std::size_t>(size), '\0');
    input.seekg(0);
    if (!input.read(text.data(), static_cast<std::streamsize>(text.size()))) return fallback;
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
    return text.empty() ? std::string(fallback) : text;
}
}
