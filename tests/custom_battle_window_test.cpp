#include "battle_layer_policy.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {
unsigned checks = 0;

void check(bool condition, const char* label, unsigned display = 0,
           unsigned bg0 = 0, unsigned bg1 = 0, unsigned bg2 = 0) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr,
            "battle canvas layout: %s failed (DISPCNT=%04x BG0CNT=%04x "
            "BG1CNT=%04x BG2CNT=%04x)\n",
            label, display, bg0, bg1, bg2);
        std::exit(1);
    }
}

void expect(const char* label, bool expected, unsigned display,
            unsigned bg0, unsigned bg1, unsigned bg2) {
    check(swordcraft3::battle_canvas_layout(display, bg0, bg1, bg2) == expected,
          label, display, bg0, bg1, bg2);
}

// Frozen pre-change register policy. This is deliberately independent of the
// new helper: the Aqua Ball text-canvas exception must not broaden old layouts.
bool previous_layout(unsigned display, unsigned bg0, unsigned bg1,
                     unsigned bg2) {
    return (display & 0x0b86u) == 0x0300u && (display & 7u) <= 1u &&
           (bg2 == 0x0305u || bg2 == 0x4385u) && bg1 == 0x450bu &&
           (bg0 == 0u || bg0 == 0x470bu);
}
}

int main() {
    // Only guest register tuples are inspected. No raster pixels, screenshots,
    // framebuffer comparison, or visual scene identification are involved.
    expect("normal text canvas", true, 0x1740, 0x470b, 0x450b, 0x0305);
    expect("normal HUD band", true, 0x1740, 0, 0x450b, 0x0305);
    expect("critical affine canvas", true, 0x1741, 0x470b, 0x450b, 0x4385);
    expect("critical HUD band", true, 0x1741, 0, 0x450b, 0x4385);
    for (unsigned bg0 : {0u, 0x470bu}) {
        expect("Dark Hole affine active", true, 0x1741, bg0, 0x450b, 0x0385);
        expect("Dark Hole affine retired", true, 0x1341, bg0, 0x450b, 0x0385);
        expect("small affine is not text", false, 0x1740, bg0, 0x450b, 0x0385);
        expect("small affine may not wrap", false, 0x1741, bg0, 0x450b, 0x2385);
        for (unsigned window : {0x2000u, 0x4000u, 0x8000u})
            expect("small affine unreviewed window", false, 0x1741 | window,
                   bg0, 0x450b, 0x0385);
    }

    // DISPCNT and the canvas descriptor can be updated in separate guest
    // instructions; preserve all already-supported intermediate tuples.
    for (unsigned mode : {0u, 1u}) {
        for (unsigned descriptor : {0x0305u, 0x4385u}) {
            expect("legacy disabled canvas", true, 0x1340u | mode,
                   0x470b, 0x450b, descriptor);
            expect("legacy disabled canvas with window", true, 0x3340u | mode,
                   0, 0x450b, descriptor);
        }
    }

    expect("Aqua Ball removed canvas", true, 0x1340, 0x470b, 0x450b, 0x4305);
    expect("Aqua Ball removed canvas HUD", true, 0x1340, 0, 0x450b, 0x4305);
    expect("Aqua Ball removed canvas with WIN0", true, 0x3340, 0x470b, 0x450b, 0x4305);
    // Complete-raster teardown also re-enables this 512-wide text descriptor.
    // Layout ownership may continue; the separate margin policy requires
    // signed-window proof instead of applying a 256-wide extent to it.
    expect("Aqua Ball text canvas re-enabled", true, 0x1740, 0x470b, 0x450b, 0x4305);
    expect("Aqua Ball text canvas re-enabled with WIN0", true, 0x3740, 0x470b, 0x450b, 0x4305);
    expect("disabled unreviewed affine canvas", false, 0x1341, 0x470b, 0x450b, 0x4305);
    expect("disabled unreviewed affine canvas with WIN0", false, 0x3341, 0x470b, 0x450b, 0x4305);
    expect("active unreviewed affine canvas", false, 0x1741, 0x470b, 0x450b, 0x4305);
    expect("active unreviewed affine canvas with WIN0", false, 0x3741, 0x470b, 0x450b, 0x4305);

    for (unsigned descriptor : {0x0305u, 0x4385u, 0x4305u}) {
        expect("forced blank", false, 0x13c0, 0x470b, 0x450b, descriptor);
        expect("BG0 disabled", false, 0x1240, 0x470b, 0x450b, descriptor);
        expect("BG1 disabled", false, 0x1140, 0x470b, 0x450b, descriptor);
        expect("unreviewed BG3 enabled", false, 0x1b40, 0x470b, 0x450b, descriptor);
        expect("different HUD descriptor", false, 0x1340, 1, 0x450b, descriptor);
        expect("different near descriptor", false, 0x1340, 0x470a, 0x450b, descriptor);
        expect("different far descriptor", false, 0x1340, 0x470b, 0x450a, descriptor);
        for (unsigned mode = 2; mode < 8; ++mode)
            expect("unsupported display mode", false, 0x1340u | mode,
                   0x470b, 0x450b, descriptor);
    }

    // Exhaustively preserve the old mode/enable/window flag combinations.
    for (unsigned display = 0; display <= 0xffff; ++display) {
        for (unsigned bg0 : {0u, 0x470bu}) {
            for (unsigned bg2 : {0x0305u, 0x4385u})
                expect("legacy register equivalence",
                       previous_layout(display, bg0, 0x450b, bg2),
                       display, bg0, 0x450b, bg2);

            // Derive the new exception from the unchanged old canvas guard,
            // constraining the additional descriptor to Mode 0 regardless
            // of the captured BG2 enable bit during the transition.
            const bool reviewed_text =
                previous_layout(display, bg0, 0x450b, 0x0305) &&
                (display & 7u) == 0u;
            expect("Mode 0 text-canvas exception", reviewed_text,
                   display, bg0, 0x450b, 0x4305);
            const bool reviewed_small_affine =
                previous_layout(display, bg0, 0x450b, 0x4385) &&
                (display & 7u) == 1u && (display & 0xe000u) == 0;
            expect("Mode 1 bounded affine exception", reviewed_small_affine,
                   display, bg0, 0x450b, 0x0385);
        }
    }

    // The additional descriptor is exact, whether BG2 is hidden or enabled.
    for (unsigned bg2 = 0; bg2 <= 0xffff; ++bg2) {
        const bool reviewed = bg2 == 0x0305 || bg2 == 0x4385 || bg2 == 0x4305;
        expect("disabled descriptor allowlist", reviewed,
               0x1340, 0x470b, 0x450b, bg2);
        expect("windowed disabled descriptor allowlist", reviewed,
               0x3340, 0, 0x450b, bg2);
        expect("enabled descriptor allowlist", reviewed,
               0x1740, 0x470b, 0x450b, bg2);
        expect("windowed enabled descriptor allowlist", reviewed,
               0x3740, 0, 0x450b, bg2);
        const bool affine_reviewed = bg2 == 0x0305 || bg2 == 0x4385 || bg2 == 0x0385;
        expect("Mode 1 active descriptor allowlist", affine_reviewed,
               0x1741, 0x470b, 0x450b, bg2);
        expect("Mode 1 retired descriptor allowlist", affine_reviewed,
               0x1341, 0, 0x450b, bg2);
    }

    std::printf("battle canvas register guards and teardown transitions: PASS (%u checks)\n", checks);
}
