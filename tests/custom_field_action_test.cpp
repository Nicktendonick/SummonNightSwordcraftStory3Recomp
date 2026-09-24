#include "custom_field_action.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
constexpr std::uint32_t ewram_base = 0x02000000;
constexpr unsigned root_offset = 0xe000;
constexpr unsigned action_offset = 0x1fbc;
constexpr unsigned action_stride = 0x1c;
constexpr unsigned action_count = 8;
constexpr unsigned tool_offset = 0x1eb8 + 0xf;
constexpr std::uint32_t tool_callback = 0x0809da99;
constexpr std::uint32_t selection_callback = 0x0809d859;
constexpr std::uint32_t bow_callback = 0x0809e3ed;
constexpr std::uint32_t object_callback = 0x080a0ad1;

unsigned checks = 0;
unsigned failures = 0;

void put16(std::vector<std::uint8_t>& bytes, unsigned offset, unsigned value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::vector<std::uint8_t>& bytes, unsigned offset, std::uint32_t value) {
    put16(bytes, offset, value);
    put16(bytes, offset + 2, value >> 16);
}

struct Fixture {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x40000);
    swordcraft3::FieldOwner owner{ewram_base + root_offset, 0x1000};

    Fixture() { put16(ram, root_offset, owner.flags); }

    void tool(unsigned value) { ram[root_offset + tool_offset] = static_cast<std::uint8_t>(value); }

    void action(unsigned slot = 0, std::uint32_t callback = tool_callback,
                unsigned state = 0, unsigned active = 1,
                unsigned enabled = 0x1000, unsigned disabled = 1) {
        const unsigned offset = root_offset + action_offset + slot * action_stride;
        put16(ram, offset, state);
        put16(ram, offset + 0x12, enabled);
        put16(ram, offset + 0x14, disabled);
        put16(ram, offset + 0x16, active);
        put32(ram, offset + 0x18, callback);
    }

    void flags(unsigned value) {
        owner.flags = value;
        put16(ram, root_offset, value);
    }

    void target(unsigned slot, unsigned index, unsigned type) {
        put16(ram, root_offset + action_offset + slot * action_stride + 2, index);
        if (index < 32) ram[root_offset + 0x1538 + index * 0x3c + 4] = static_cast<std::uint8_t>(type);
    }
};

// Deliberately not assert(): these checks remain active in Release/NDEBUG builds.
void expect(const char* label, const Fixture& fixture, bool wanted) {
    const auto before = fixture.ram;
    const auto owner_before = fixture.owner;
    const bool actual = swordcraft3::field_tool_action(fixture.ram, fixture.owner);
    ++checks;
    if (actual != wanted) {
        std::fprintf(stderr, "FAIL: %s (got %d, wanted %d)\n", label, actual, wanted);
        ++failures;
    }
    if (fixture.ram != before || fixture.owner.pointer != owner_before.pointer ||
        fixture.owner.flags != owner_before.flags) {
        std::fprintf(stderr, "FAIL: %s mutated read-only state\n", label);
        ++failures;
    }
}
}

int main() {
    // Each ordinary tool's dispatcher has the same admitted entry/running states.
    // Iterate every slot so the last record cannot accidentally go unchecked.
    for (unsigned tool = 0; tool < 7; ++tool) {
        for (unsigned state = 0; state < 2; ++state) {
            for (unsigned slot = 0; slot < action_count; ++slot) {
                Fixture fixture;
                fixture.tool(tool);
                fixture.action(slot, tool_callback, state);
                expect("ordinary tool action at each state and slot", fixture, true);
                fixture.action(slot, selection_callback, state);
                expect("field selection at each tool, state and slot", fixture, true);
                fixture.flags(0x1004);
                expect("foreground script overrides field selection", fixture, false);
            }
        }
    }

    Fixture fixture;
    for (unsigned state : {2u, 3u, 0xffffu}) {
        fixture = Fixture{};
        fixture.action(0, selection_callback, state);
        expect("unknown field selection substate", fixture, false);
    }
    fixture = Fixture{};
    fixture.action(0, selection_callback, 1);
    fixture.action(7, 0x0809b849, 0);
    expect("concurrent arbitrary-event action overrides field selection", fixture, false);
    fixture = Fixture{};
    expect("no active action is not a tool-owned lock", fixture, false);
    fixture.action();
    expect("tool-owned lock", fixture, true);

    for (unsigned flags : {0u, 1u, 4u, 5u, 0x1001u, 0x1004u, 0xffffu}) {
        fixture = Fixture{};
        fixture.action();
        fixture.flags(flags);
        expect("foreground script or other control flags remain narrow", fixture, false);
    }
    for (unsigned flags : {0x1002u, 0x1010u, 0x9000u}) {
        fixture = Fixture{};
        fixture.action();
        fixture.flags(flags);
        expect("unrelated flags do not change tool lock ownership", fixture, true);
    }
    fixture = Fixture{};
    fixture.action();
    put16(fixture.ram, root_offset, 0x1004);
    expect("stale owner flags cannot override current foreground script", fixture, false);
    for (unsigned tool : {7u, 8u, 0xffu}) {
        fixture = Fixture{};
        fixture.action();
        fixture.tool(tool);
        expect("unknown tool enum", fixture, false);
    }
    for (unsigned state : {2u, 3u, 0xffffu}) {
        fixture = Fixture{};
        fixture.action(0, tool_callback, state);
        expect("unknown ordinary action state", fixture, false);
    }
    for (std::uint32_t callback : {0u, 0x0809da98u, 0x08093995u, 0xffffffffu}) {
        fixture = Fixture{};
        fixture.action(0, callback);
        expect("unknown or non-Thumb action callback", fixture, false);
    }
    for (unsigned enabled : {0u, 1u, 0x1001u, 0xffffu}) {
        fixture = Fixture{};
        fixture.action(0, tool_callback, 0, 1, enabled, 1);
        expect("tool enable mask must match", fixture, false);
    }
    for (unsigned disabled : {0u, 2u, 3u, 0xffffu}) {
        fixture = Fixture{};
        fixture.action(0, tool_callback, 0, 1, 0x1000, disabled);
        expect("tool disable mask must match", fixture, false);
    }

    fixture = Fixture{};
    fixture.action(0, tool_callback, 0, 0);
    expect("stale inactive tool callback", fixture, false);
    fixture.action(7);
    expect("inactive stale action does not suppress a live action", fixture, true);
    for (unsigned active : {2u, 3u, 0xffffu}) {
        fixture = Fixture{};
        fixture.action(0, tool_callback, 0, active);
        expect("tool allocator must have exactly its traced active flags", fixture, false);
    }

    fixture = Fixture{};
    fixture.action();
    fixture.action(7, 0x08012345, 0, 1, 0, 0);
    expect("concurrent passive no-lock action", fixture, true);
    fixture.action(7, 0x08012345, 0, 1, 0x10, 2);
    expect("concurrent action with unrelated masks", fixture, true);
    fixture.action(7, 0x08012345, 0, 1, 0x1000, 0);
    expect("unknown concurrent lock owner after valid action", fixture, false);
    fixture.action(0, 0x08012345, 0, 1, 0, 1);
    fixture.action(7);
    expect("unknown concurrent lock owner before valid action", fixture, false);
    fixture.action(0, 0x08012345, 0, 0, 0x1000, 1);
    expect("inactive unknown action cannot own lock", fixture, true);

    for (unsigned state : {0u, 1u}) {
        fixture = Fixture{};
        fixture.tool(6);
        fixture.action(0, bow_callback, state);
        expect("traced bow action states", fixture, true);
    }
    for (unsigned state : {2u, 3u, 0xffffu}) {
        fixture = Fixture{};
        fixture.tool(6);
        fixture.action(0, bow_callback, state);
        expect("untraced bow action states", fixture, false);
    }
    for (unsigned tool = 0; tool < 6; ++tool) {
        fixture = Fixture{};
        fixture.tool(tool);
        fixture.action(0, bow_callback);
        expect("bow callback requires the bow tool", fixture, false);
    }

    // The shared object dispatcher has subtype-specific progress states;
    // independent control-flow review established 9 as the largest substate.
    for (unsigned tool = 0; tool < 6; ++tool) {
        for (unsigned type = 2; type <= 9; ++type) {
            for (unsigned state : {0u, 1u, 2u, 3u, 6u, 9u}) {
                fixture = Fixture{};
                fixture.tool(tool);
                fixture.action(7, object_callback, state);
                fixture.target(7, 31, type);
                expect("object tool, kind and subtype-specific progress state", fixture, true);
            }
        }
    }
    for (unsigned state : {10u, 0xffffu}) {
        fixture = Fixture{};
        fixture.action(0, object_callback, state);
        fixture.target(0, 0, 2);
        expect("untraced object action substate", fixture, false);
    }
    fixture = Fixture{};
    fixture.action(0, tool_callback, 1);
    fixture.action(1, object_callback, 2);
    fixture.target(1, 0, 2);
    fixture.action(7, object_callback, 9);
    fixture.target(7, 31, 9);
    expect("ordinary tool animation and two concurrent object actions", fixture, true);
    fixture.flags(0x1004);
    expect("foreground script overrides valid concurrent tool and object actions", fixture, false);
    for (unsigned index : {0u, 1u, 30u, 31u}) {
        fixture = Fixture{};
        fixture.action(0, object_callback);
        fixture.target(0, index, 2);
        expect("object index within full record table", fixture, true);
    }
    for (unsigned index : {32u, 33u, 0xffffu}) {
        fixture = Fixture{};
        fixture.action(0, object_callback);
        fixture.target(0, index, 2);
        expect("object index out of bounds", fixture, false);
    }
    for (unsigned type : {0u, 1u, 10u, 0xffu}) {
        fixture = Fixture{};
        fixture.action(0, object_callback);
        fixture.target(0, 0, type);
        expect("untraced object kind", fixture, false);
    }
    fixture = Fixture{};
    fixture.tool(6);
    fixture.action(0, object_callback);
    fixture.target(0, 0, 2);
    expect("bow does not use direct object tool action", fixture, false);

    for (std::uint32_t pointer : {0u, ewram_base - 4, ewram_base + root_offset + 1,
                                 ewram_base + 0x40000, 0xffffffffu}) {
        fixture = Fixture{};
        fixture.action();
        fixture.owner.pointer = pointer;
        expect("invalid or unaligned root pointer", fixture, false);
    }
    fixture = Fixture{};
    fixture.action();
    fixture.ram.resize(root_offset + action_offset + action_count * action_stride);
    expect("full pool ending exactly at supplied span", fixture, true);
    fixture.ram.pop_back();
    expect("truncated final action record", fixture, false);
    fixture.ram.clear();
    expect("empty RAM span", fixture, false);

    std::printf("Field tool action predicate: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
