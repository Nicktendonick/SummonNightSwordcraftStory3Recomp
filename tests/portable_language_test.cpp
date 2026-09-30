#include "portable_language.h"
#include <iostream>
#define CHECK(x) do { if (!(x)) throw std::runtime_error("Failed: " #x); } while (0)
int main(int argc, char** argv) {
    using namespace swordcraft3;
    std::vector<std::string> args{"game", "--rom", "first", "--save", "chosen.eep", "--rom", "last"};
    CHECK(argument_value(args, "--rom") == "last");
    replace_argument(args, "--rom", "verified");
    CHECK(args.size() == 5 && argument_value(args, "--rom") == "verified");
    CHECK(argument_value(args, "--save") == "chosen.eep");
    CHECK(std::string(japanese_language.save) != english_language.save);
    CHECK(std::string(japanese_language.states) != english_language.states);
    CHECK(std::string(english_language.save) == "Saves/battery.eep");
    if (argc == 4) {
        CHECK(&language_for_rom(argv[1]) == &japanese_language);
        CHECK(&language_for_rom(argv[2]) == &english_language);
        bool rejected = false;
        try { language_for_rom(argv[3]); } catch (const std::exception&) { rejected = true; }
        CHECK(rejected);
        // A false caller fingerprint cannot select the wrong instruction corpus.
        args = {"game", "--rom", argv[1], "--rom-sha1", english_sha1};
        CHECK(&resolve_portable_language(args, ".") == &japanese_language);
        CHECK(argument_value(args, "--rom-sha1") == japanese_sha1);
        CHECK(std::filesystem::path(argument_value(args, "--save")).filename() == "japanese.eep");
        args = {"game", "--rom", argv[2], "--save", "explicit.eep"};
        CHECK(&resolve_portable_language(args, ".") == &english_language);
        CHECK(argument_value(args, "--save") == "explicit.eep");
    }
    std::cout << "PASS: language routing, legacy English save paths and explicit save overrides\n";
}
