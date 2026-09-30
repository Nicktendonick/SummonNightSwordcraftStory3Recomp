#pragma once
#include <cstddef>
#include <cstdint>

namespace swordcraft3 {
// Select-to-Guard experiment. No host-side latch, timers, forced animation or
// direct RAM writes: the native input-history/guard routines do that work.
// Exact read/write sites and their surrounding routines are authenticated.
namespace guard_detail {
inline std::uint32_t u16(const std::uint8_t* p) { return p[0] | (std::uint32_t(p[1]) << 8); }
inline std::uint32_t u32(const std::uint8_t* p) { return u16(p) | (u16(p + 2) << 16); }
inline std::uint64_t hash(const std::uint8_t* p, std::size_t n) {
    std::uint64_t h = 14695981039346656037ull;
    for (std::size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}
}
inline bool guard_rom_supported(const std::uint8_t* rom, std::size_t size) {
    struct Range { unsigned offset, size; std::uint64_t hash; };
    // Entire reviewed routines; identical in the supported JP and English ROMs.
    constexpr Range ranges[] = {
        {0x270ac, 0x3b8, 0x77abbd67703dbc58ull}, // Manual/AI input dispatch.
        {0x29930, 0xac, 0xb09adeb964f2d589ull}, // Select auto-battle toggle.
        {0x412d8, 0x4fc, 0x4812a2243b4a4ab4ull}, // Input edges and action gates.
        {0x42688, 0x23c, 0x883c85b0211a847eull}, // Ability/Guard dispatch.
        {0x49018, 0x4c, 0x720210e6fcce5b3eull}, // Native hold/release.
    };
    if (!rom) return false;
    for (const auto& r : ranges)
        if (r.offset > size || r.size > size - r.offset ||
            guard_detail::hash(rom + r.offset, r.size) != r.hash) return false;
    return true;
}
struct GuardExperiment {
    bool supported = false;
    const std::uint8_t* ram = nullptr;
    std::size_t size = 0;
    bool battle() const {
        return supported && ram && size >= 0x6ac4 &&
            guard_detail::u32(ram + 0x6ac0) == 0x03000000 &&
            guard_detail::u32(ram + 0x6ab4) == 4 && ram[0xc] == 2 &&
            ram[0xd] != 3 && ram[0xf] == 0;
    }
    bool manual() const { return battle() && ram[0x12] == 0; }
    bool held() const { return manual() && (guard_detail::u16(ram + 0x594c) & 4); }
    bool read(std::uint32_t pc, std::uint32_t address, unsigned width,
              std::uint32_t original, std::uint32_t actor, std::uint32_t* value) const {
        if (pc != 0x080272e2 && pc != 0x08029966 && pc != 0x08042698 &&
            pc != 0x080426dc && pc != 0x08042866) return false;
        if (!value || !manual()) return false;
        if (pc == 0x080272e2 && address == 0x0300594c && width == 2) {
            // Feed native input-history processing, not a repeated synthetic tap.
            *value = (original & ~4u) | ((original & 4u) ? 2u : 0u);
            return true;
        }
        if (pc == 0x08029966 && address == 0x03005920 && width == 2) {
            *value = original & ~4u; // Do not turn auto-battle on.
            return true;
        }
        if (!held() || actor != 0x030008c0) return false;
        if ((pc == 0x08042698 || pc == 0x080426dc) && address == 0x03000a26 && width == 1) {
            *value = 0; // Only this native action sees Guard, not the real R slot.
            return true;
        }
        if (pc == 0x08042866 && address == 0x03006ac0 && width == 4 && original == 0x03000000) {
            // This load is ONLY an actor-equality test before the HUD reset.
            // The sentinel is never dereferenced. Skip resetting the ability
            // cursor/label; RAM still contains its real owner and selected slot.
            *value = 0;
            return true;
        }
        return false;
    }
    bool write(std::uint32_t pc, std::uint32_t address, unsigned width,
               std::uint32_t original, std::uint32_t actor, std::uint32_t* value) const {
        if (!value || pc != 0x08042862 || address != 0x03000a26 || width != 1 ||
            original != 0 || actor != 0x030008c0 || !held()) return false;
        *value = ram[0xa26]; // Keep the selected ability; native Guard still runs.
        return true;
    }
};
}
