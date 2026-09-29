#include "presentation_preferences.h"
#include <iostream>
#include <stdexcept>

#define CHECK(value) do { if (!(value)) throw std::runtime_error("Failed: " #value); } while (0)

namespace fs = std::filesystem;
static std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    CHECK(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
static void write(const fs::path& path, const std::string& value) {
    std::ofstream output(path, std::ios::binary);
    output << value;
    CHECK(output.good());
}
static bool save(const fs::path& path, int scaling, int effect, int strength) {
    return gbarecomp::save_presentation_preferences(path, scaling, effect, strength);
}

int main() {
    // Keep evidence in the current test/build directory, never a player's data.
    const fs::path directory = fs::current_path() / ("presentation-preferences-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(fs::create_directory(directory));
    try {
        const auto file = directory / fs::path(u8"launcher-日本.ini");
        CHECK(save(file, 3, 2, 75));
        CHECK(read(file) == "[Launcher]\nlinear_filter = 0\nsharp_filter = 0\nsmooth_filter = 1\n"
                            "screen_effect = 2\nscreen_effect_strength = 75\n");
        for (int scaling = 0; scaling <= 3; ++scaling) {
            CHECK(save(file, scaling, scaling % 3, scaling * 25));
            const auto text = read(file);
            CHECK(text.find("linear_filter = " + std::to_string(scaling == 1) + "\n") != std::string::npos);
            CHECK(text.find("sharp_filter = " + std::to_string(scaling == 2) + "\n") != std::string::npos);
            CHECK(text.find("smooth_filter = " + std::to_string(scaling == 3) + "\n") != std::string::npos);
            CHECK(text.find("screen_effect = " + std::to_string(scaling % 3) + "\n") != std::string::npos);
            CHECK(text.find("screen_effect_strength = " + std::to_string(scaling * 25) + "\n") != std::string::npos);
        }
        const std::string prefix = "\xef\xbb\xbf; custom settings\r\n[General]\r\nlinear_filter = 99\r\n";
        const std::string suffix = "[KeyMap]\r\n; preserve input\r\na = 32\r\n";
        write(file, prefix + "[Launcher] ; comment\r\nvolume = 81\r\n  linear_filter\t=\t1  ; keep\r\n"
              "sharp_filter = nonsense # replace value only\r\nunknown = keep me\r\n" + suffix);
        CHECK(save(file, 3, 1, 42));
        const auto preserved = read(file);
        CHECK(preserved.substr(0, prefix.size()) == prefix);
        CHECK(preserved.substr(preserved.size() - suffix.size()) == suffix);
        CHECK(preserved.find("volume = 81\r\n") != std::string::npos);
        CHECK(preserved.find("  linear_filter\t=\t0  ; keep\r\n") != std::string::npos);
        CHECK(preserved.find("sharp_filter = 0 # replace value only\r\n") != std::string::npos);
        CHECK(preserved.find("unknown = keep me\r\n") != std::string::npos);
        CHECK(preserved.find("screen_effect_strength = 42\r\n[KeyMap]") != std::string::npos);
        CHECK(save(file, 3, 1, 42));
        CHECK(read(file) == preserved);
        for (const auto& invalid : {std::array<int, 3>{-1, 0, 30}, {4, 0, 30}, {0, -1, 30},
                                   {0, 3, 30}, {0, 0, -1}, {0, 0, 101}}) {
            CHECK(!save(file, invalid[0], invalid[1], invalid[2]));
            CHECK(read(file) == preserved);
        }
        for (const auto& malformed : {std::string("[Launcher\nvolume = 5\n"),
                                      std::string("[Launcher]junk\n"), std::string("abc\0def", 7)}) {
            write(file, malformed);
            CHECK(!save(file, 0, 0, 0));
            CHECK(read(file) == malformed);
        }
        write(file, "[Launcher]\nlinear_filter=1\n[Other]\na=2\n[Launcher]\nlinear_filter=1");
        CHECK(save(file, 2, 0, 0));
        CHECK(read(file).find("linear_filter=1") == std::string::npos);
        CHECK(read(file).find("[Other]\na=2\n") != std::string::npos);
        write(file, "[General]\nvolume=9");
        CHECK(save(file, 0, 0, 0));
        CHECK(read(file).find("[General]\nvolume=9\n[Launcher]\n") == 0);
        CHECK(!save(directory, 0, 0, 0));
        CHECK(!save(directory / "missing" / "launcher.ini", 0, 0, 0));
        CHECK(gbarecomp::save_presentation_preferences("", 0, 0, 0));
        CHECK(!gbarecomp::save_presentation_preferences("", 9, 0, 0));

        write(file, prefix + "[Launcher]\r\nvolume = 73\r\n" + suffix);
        CHECK(!gbarecomp::read_host_aspect_preference(file, 3));
        for (int index : {0, 1, 2, 0, 2, 1}) {
            CHECK(gbarecomp::save_host_aspect_preference(file, index, 3));
            CHECK(gbarecomp::read_host_aspect_preference(file, 3) == index);
            CHECK(save(file, 3, 2, 65));
            CHECK(gbarecomp::read_host_aspect_preference(file, 3) == index);
            CHECK(read(file).find("volume = 73\r\n") != std::string::npos);
            CHECK(read(file).find(suffix) != std::string::npos);
            CHECK(read(file).substr(0, prefix.size()) == prefix);
        }
        const auto valid_aspect = read(file);
        for (int bad : {-1, 3, 99}) {
            CHECK(!gbarecomp::save_host_aspect_preference(file, bad, 3));
            CHECK(read(file) == valid_aspect);
        }
        for (const std::string bad : {"no", "1oops", "3", "-1", "", "0\nhost_aspect_index=1"}) {
            write(file, "[Launcher]\nhost_aspect_index=" + bad + "\n");
            CHECK(!gbarecomp::read_host_aspect_preference(file, 3));
        }
        write(file, valid_aspect);
        const auto before_failure = read(file);
#ifdef _WIN32
        CHECK(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_READONLY));
        CHECK(!save(file, 3, 2, 100));
        CHECK(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_NORMAL));
        CHECK(read(file) == before_failure);
        HANDLE lock = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(lock != INVALID_HANDLE_VALUE);
        const bool blocked = !save(file, 3, 2, 100);
        CHECK(CloseHandle(lock));
        CHECK(blocked && read(file) == before_failure);
        lock = CreateFileW(file.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(lock != INVALID_HANDLE_VALUE);
        const bool unreadable = !save(file, 3, 2, 100);
        CHECK(CloseHandle(lock));
        CHECK(unreadable && read(file) == before_failure);
#else
        fs::permissions(file, fs::perms::owner_read);
        CHECK(!save(file, 3, 2, 100));
        fs::permissions(file, fs::perms::owner_read | fs::perms::owner_write);
        CHECK(read(file) == before_failure);
#endif
        for (const auto& entry : fs::directory_iterator(directory)) CHECK(entry.path() == file);
        fs::remove(file);
        fs::remove(directory);
        std::cout << "PASS: presentation preferences atomic update, section/comment preservation, validation, and failures\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nTest evidence retained at " << directory << '\n';
        return 1;
    }
}
