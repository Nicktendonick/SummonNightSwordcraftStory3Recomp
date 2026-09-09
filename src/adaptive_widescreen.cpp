#include "adaptive_widescreen.h"
#include "battle_hud_borders.h"
#include "battle_layer_policy.h"
#include "swordcraft3_display_settings.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "gba_ppu.h"
#include "runtime.h"
#include "runtime_arm.h"
#include "swordcraft3_true_map.h"

namespace {

constexpr std::uint32_t kNativeWidth = 240;
constexpr std::uint16_t kFixed16x9Width = 284;
constexpr std::uint16_t kWide2To1Width = 320;
constexpr std::uint16_t kWholeArenaWidth = 384;
// The battle's VCOUNT program switches BG0 at 18/124; the writes take effect
// on completed rows 19..124. Only that raster
// band may be continued into the margins. The legacy reflected fallback keeps
// its narrower, previously validated crop so it remains an exact escape hatch.
constexpr int kBattleRasterTop = swordcraft3::kBattleHudTopEnd;
constexpr int kBattleRasterBottom = swordcraft3::kBattleHudBottomStart;
constexpr int kBattleReflectedPlayfieldTop = 59;
constexpr int kBattleReflectedPlayfieldBottom = 112;
constexpr std::uint16_t kBattleRasterBg0Cnt = 0x470Bu;
// The previous repeating presentation used the native foreground width as its
// period. Natural mode instead consumes the complete 512px raster map once;
// both the forest and village validation states show authored offscreen art.
constexpr unsigned kBattleRasterBg0LoopWidth = 240u;
constexpr std::size_t kDispcntOffset = 0x00;
constexpr std::size_t kBgcntOffset = 0x08;

constexpr const char* kAspectLabels[] = {
    "3:2 (Native)", "16:9 (284 px)", "2:1 (320 px)",
    "12:5 (Full Arena, 384 px)"
};
constexpr std::uint16_t kAspectWidths[] = {
    static_cast<std::uint16_t>(kNativeWidth), kFixed16x9Width,
    kWide2To1Width, kWholeArenaWidth
};
constexpr const char* kLegacyAspectLabels[] = {
    "3:2 (Native)", "16:9 (284 px)", "2:1 (320 px)"
};
constexpr std::uint16_t kLegacyAspectWidths[] = {
    static_cast<std::uint16_t>(kNativeWidth), kFixed16x9Width, kWide2To1Width
};

// sub_08009840 builds the game's OAM buffer. These are its horizontal object
// culling constants: CMP X,#239 on the right and MOV #64 / NEG on the left.
// The recompiler config opts only these exact instructions into the shared
// immediate-override seam. Both the stock Japanese ROM and English beta retain
// the same instructions at these addresses.
constexpr std::uint32_t kObjRightCullLimitPc = 0x08009B9Eu;
constexpr std::uint32_t kObjLeftCullDistancePc = 0x08009BB4u;
constexpr std::uint32_t kNativeObjRightCullLimit = 239u;
constexpr std::uint32_t kNativeObjLeftCullDistance = 64u;

unsigned s_mirrored_bg_mask = 0;
unsigned s_looped_bg_mask = 0;
unsigned s_natural_battle_bg_mask = 0;
unsigned s_wrap_ok_bg_mask = 0;
unsigned s_true_map_bg_mask = 0;
unsigned s_battle_hybrid_bg_mask = 0;
std::array<unsigned, 4> s_battle_map_width_px{};
std::array<unsigned, 4> s_battle_authored_width_px{};
std::array<unsigned, 4> s_battle_hofs{};
unsigned s_extra_left = 0;
unsigned s_extra_right = 0;
bool s_expand_overworld_objects = false;
bool s_expand_battle_objects = false;
bool s_battle_scenery = false;
bool s_battle_hud_candidate = false;
unsigned s_battle_backdrop_period = 0;
std::array<unsigned,2> s_battle_layer_span{};
unsigned s_battle_span_valid = 0;

bool battle_layer_fix_enabled() {
    const char* legacy = std::getenv("SWORDCRAFT3_LEGACY_BATTLE_LAYERS");
    return !legacy || std::strcmp(legacy, "1") != 0;
}
std::uint64_t s_presentation_frame = 0;

enum class BattleMarginMode {
    Natural,
    Loop,
    Reflect,
};

BattleMarginMode battle_margin_mode() {
    // Sample the finite arena map in its natural world order by default. The
    // former repeated and reflected presentations remain launch-time rollback
    // options while the natural-camera policy receives broader arena testing:
    //   SWORDCRAFT3_BATTLE_MARGIN_MODE=loop
    //   SWORDCRAFT3_BATTLE_MARGIN_MODE=reflect
    const char* mode = std::getenv("SWORDCRAFT3_BATTLE_MARGIN_MODE");
    if (mode && (std::strcmp(mode, "loop") == 0 ||
                 std::strcmp(mode, "repeat") == 0 ||
                 std::strcmp(mode, "1") == 0)) {
        return BattleMarginMode::Loop;
    }
    if (mode && (std::strcmp(mode, "reflect") == 0 ||
                 std::strcmp(mode, "mirror") == 0 ||
                 std::strcmp(mode, "0") == 0)) {
        return BattleMarginMode::Reflect;
    }
    return BattleMarginMode::Natural;
}

bool use_whole_arena_view() {
    // The standard battle configuration table contains seventeen entries and
    // every one declares a 0x180-pixel logical arena. Keep the former 320px
    // ceiling available as a launch-time rollback while this full-arena view
    // receives owner testing:
    //   SWORDCRAFT3_ARENA_VIEW=legacy
    const char* mode = std::getenv("SWORDCRAFT3_ARENA_VIEW");
    return !mode || (std::strcmp(mode, "legacy") != 0 &&
                     std::strcmp(mode, "320") != 0 &&
                     std::strcmp(mode, "0") != 0);
}

constexpr std::uint32_t widened_obj_cull_immediate(
        std::uint32_t instruction_pc, std::uint32_t original_value,
        std::uint32_t extra_left, std::uint32_t extra_right) {
    if (instruction_pc == kObjRightCullLimitPc &&
        original_value == kNativeObjRightCullLimit) {
        return kNativeObjRightCullLimit + extra_right;
    }
    if (instruction_pc == kObjLeftCullDistancePc &&
        original_value == kNativeObjLeftCullDistance) {
        return kNativeObjLeftCullDistance + extra_left;
    }
    return original_value;
}

static_assert(widened_obj_cull_immediate(
    kObjRightCullLimitPc, 239u, 22u, 22u) == 261u);
static_assert(widened_obj_cull_immediate(
    kObjLeftCullDistancePc, 64u, 22u, 22u) == 86u);
static_assert(widened_obj_cull_immediate(
    0x08009BD8u, 159u, 22u, 22u) == 159u);

int swordcraft3_world_alu_immediate(
        std::uint32_t instruction_pc, std::uint32_t original_value,
        std::uint32_t* out_value) {
    if ((!s_expand_overworld_objects && !s_expand_battle_objects) ||
        !out_value) {
        return 0;
    }
    const std::uint32_t widened = widened_obj_cull_immediate(
        instruction_pc, original_value, s_extra_left, s_extra_right);
    if (widened == original_value) return 0;
    *out_value = widened;
    return 1;
}

std::uint64_t audit_env_u64(const char* name, std::uint64_t fallback) {
    const char* value = std::getenv(name);
    if (!value || !value[0]) return fallback;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(value, &end, 0);
    return end != value && *end == '\0' ? parsed : fallback;
}

bool should_emit_audit_frame(std::uint64_t frame) {
    const char* enabled = std::getenv("SWORDCRAFT3_WS_AUDIT");
    if (!enabled || !enabled[0] || enabled[0] == '0') return false;
    const std::uint64_t start = audit_env_u64("SWORDCRAFT3_WS_AUDIT_START", 1);
    const std::uint64_t step =
        audit_env_u64("SWORDCRAFT3_WS_AUDIT_STEP", 1);
    return frame >= start && (frame - start) % (step ? step : 1) == 0;
}

std::uint16_t read16(const std::uint8_t* data, std::size_t size,
                     std::size_t offset) {
    if (!data || offset + 1 >= size) return 0;
    return static_cast<std::uint16_t>(
        data[offset] | (static_cast<std::uint16_t>(data[offset + 1]) << 8));
}

bool battle_map_columns_equal(const gbarecomp::ExtendedViewFrameInfo* frame,
                              std::uint16_t bgcnt, unsigned height_tiles,
                              unsigned column_a, unsigned column_b) {
    if (!frame || !frame->vram) return false;
    const unsigned width_tiles =
        ((bgcnt >> 14) & 1u) ? 64u : 32u;
    const unsigned block_cols = width_tiles >> 5;
    const std::size_t screen_base =
        static_cast<std::size_t>((bgcnt >> 8) & 0x1Fu) * 0x800u;
    auto entry_offset = [&](unsigned x, unsigned y) {
        const unsigned block = (x >> 5) + (y >> 5) * block_cols;
        return screen_base + static_cast<std::size_t>(block) * 0x800u +
            static_cast<std::size_t>((y & 31u) * 32u + (x & 31u)) * 2u;
    };
    for (unsigned y = 0; y < height_tiles; ++y) {
        const std::size_t a = entry_offset(column_a, y);
        const std::size_t b = entry_offset(column_b, y);
        if (a + 1u >= frame->vram_size || b + 1u >= frame->vram_size ||
            read16(frame->vram, frame->vram_size, a) !=
                read16(frame->vram, frame->vram_size, b)) {
            return false;
        }
    }
    return true;
}

unsigned battle_authored_width(const gbarecomp::ExtendedViewFrameInfo* frame,
                               std::uint16_t bgcnt) {
    const unsigned size = (bgcnt >> 14) & 0x3u;
    const unsigned width_tiles = (size & 1u) ? 64u : 32u;
    const unsigned height_tiles = (size & 2u) ? 64u : 32u;
    unsigned fill_start = width_tiles - 1u;
    while (fill_start > 0u && battle_map_columns_equal(
            frame, bgcnt, height_tiles, fill_start - 1u,
            width_tiles - 1u)) {
        --fill_start;
    }
    const unsigned trailing_fill_columns = width_tiles - fill_start;
    // A single matching terminal column is not evidence of padding. Battle
    // maps use long repeated suffixes (16 columns in the first arena).
    return trailing_fill_columns >= 2u ? fill_start * 8u : width_tiles * 8u;
}

unsigned battle_backdrop_period(const gbarecomp::ExtendedViewFrameInfo& frame,
                                std::uint16_t cnt, unsigned span) {
    const unsigned columns = span/8u;
    if (!columns || columns > 64) return span;
    const unsigned height = (cnt & 0x8000u) ? 64u : 32u;
    const unsigned block_cols = (cnt & 0x4000u) ? 2u : 1u;
    const unsigned base = ((cnt >> 8) & 31u) * 0x800u;
    std::array<std::uint64_t,64> hashes{};
    for (unsigned x=0; x<columns; ++x) {
        auto hash = std::uint64_t{14695981039346656037ull};
        for (unsigned y=0; y<height; ++y) {
            const unsigned offset = base + ((x/32)+(y/32)*block_cols)*0x800u +
                ((y%32)*32+(x%32))*2u;
            hash = (hash ^ read16(frame.vram,frame.vram_size,offset))*1099511628211ull;
        }
        hashes[x] = hash;
    }
    const unsigned period = swordcraft3::battle_repeat_columns(hashes.data(),columns);
    // Hashes shortlist candidates; exact entries prove the repeat.
    for (unsigned x=period; x<columns; ++x)
        if (!battle_map_columns_equal(&frame,cnt,height,x,x%period)) return span;
    return period*8u;
}

int swordcraft3_battle_margin(const gba::WsBgMarginContext* context, int* out_x) {
    if (!context || !out_x || !s_battle_scenery ||
        (s_natural_battle_bg_mask & (1u << context->layer)) == 0 ||
        (context->layer != 1 && context->layer != 2)) return 0;
    const auto& c = *context;
    if (c.screen_y < 19 || c.screen_y >= 125) return -1;
    const int x = static_cast<int>(c.output_x) - static_cast<int>(c.native_left);
    const unsigned hofs = read16(c.io, 0x400, 0x10 + c.layer*4) & 511u;
    gbarecomp::ExtendedViewFrameInfo live{};
    live.vram = c.vram;
    live.vram_size = 0x18000;
    // These canvases are uploaded during VBlank. Measure at the first visible
    // gameplay sample, not at frame start or repeatedly on every scanline.
    // Only horizontal/vertical registers need to remain scanline-live.
    const unsigned index = c.layer-1;
    if ((s_battle_span_valid & (1u << index)) == 0) {
        s_battle_layer_span[index] = battle_authored_width(&live, read16(c.io, 0x400, 8+c.layer*2));
        s_battle_span_valid |= 1u << index;
    }
    if (c.layer == 2) {
        if (!swordcraft3::battle_effect_contains(x, hofs, s_battle_layer_span[index])) return -1;
        *out_x = x;
        return 1;
    }
    if (!s_battle_backdrop_period)
        s_battle_backdrop_period = battle_backdrop_period(live, read16(c.io,0x400,10),s_battle_layer_span[index]);
    *out_x = swordcraft3::battle_backdrop_x(x, hofs, s_battle_backdrop_period);
    return 1;
}

void swordcraft3_postprocess_wide_frame(std::uint8_t* rgb,
                                        std::uint32_t width,
                                        std::uint32_t height,
                                        std::uint32_t extra_left,
                                        std::uint32_t extra_right) {
    if (!rgb || width <= kNativeWidth || height != 160u ||
        extra_left + kNativeWidth + extra_right > width) {
        return;
    }

    // Some full-screen cutscene effects composite a small sprite animation
    // over a black native viewport without authoring a matching BG/window for
    // off-screen columns. If at least 90% of the authentic picture is already
    // near-black, treat it as a deliberate full-screen cover and extend black
    // into both margins. Dialogue boxes and portraits occupy enough of the
    // center that they do not meet this deliberately conservative threshold.
    std::uint32_t near_black = 0;
    constexpr std::uint32_t kNativePixels = kNativeWidth * 160u;
    for (std::uint32_t y = 0; y < 160u; ++y) {
        const std::uint8_t* row = rgb +
            (static_cast<std::size_t>(y) * width + extra_left) * 3u;
        for (std::uint32_t x = 0; x < kNativeWidth; ++x) {
            const std::uint8_t* pixel = row + x * 3u;
            if (pixel[0] <= 4u && pixel[1] <= 4u && pixel[2] <= 4u)
                ++near_black;
        }
    }
    if (near_black * 10u >= kNativePixels * 9u) {
        for (std::uint32_t y = 0; y < 160u; ++y) {
            std::uint8_t* row = rgb + static_cast<std::size_t>(y) * width * 3u;
            std::memset(row, 0, static_cast<std::size_t>(extra_left) * 3u);
            std::memset(row + static_cast<std::size_t>(extra_left + kNativeWidth) * 3u,
                        0, static_cast<std::size_t>(extra_right) * 3u);
        }
        return;
    }

    // Final presentation only: no guest palette/VRAM/OAM or original center
    // writes. Never fabricate HUD pixels in a layer-isolation debugger view.
    bool borders_applied = false;
    if (s_battle_hud_candidate && swordcraft3_battle_hud_borders_enabled() &&
        gba::g_ppu_debug_layer_mask == 0x1Fu) {
        borders_applied = swordcraft3::extend_battle_hud_borders(
            rgb, width, height, extra_left, extra_right);
    }
    if (should_emit_audit_frame(s_presentation_frame)) {
        std::fprintf(stderr,
            "swordcraft3_battle_hud_frame={\"frame\":%llu,"
            "\"enabled\":%s,\"eligible\":%s,\"applied\":%s}\n",
            static_cast<unsigned long long>(s_presentation_frame),
            swordcraft3_battle_hud_borders_enabled() ? "true" : "false",
            s_battle_hud_candidate ? "true" : "false",
            borders_applied ? "true" : "false");
    }
}

int swordcraft3_margin_tilemap(int bg, int hw_x, int screen_y,
                               std::uint16_t* out_entry) {
    // Field layers backed by the game's complete source tilemap bypass the
    // 256px hardware ring and return the authentic neighboring world tile.
    if (bg >= 0 && bg <= 3 &&
        (s_true_map_bg_mask & (1u << static_cast<unsigned>(bg))) != 0) {
        return swordcraft3_true_map_tilemap(
            bg, hw_x, screen_y, out_entry);
    }

    if (bg >= 0 && bg <= 3) {
        const unsigned layer_bit = 1u << static_cast<unsigned>(bg);
        if ((s_natural_battle_bg_mask & layer_bit) != 0) {
            const unsigned map_width = s_battle_map_width_px[bg];
            const unsigned authored_width = s_battle_authored_width_px[bg];
            if (map_width != 0u && authored_width != 0u &&
                (map_width & (map_width - 1u)) == 0u) {
                const unsigned texture_x = static_cast<unsigned>(
                    hw_x + static_cast<int>(s_battle_hofs[bg])) &
                    (map_width - 1u);
                // Preserve the hardware's normal camera-to-map projection,
                // but do not let its power-of-two address wrap turn the far
                // edge back into the opposite side of this finite arena.
                return texture_x < authored_width ?
                    gba::kWsTilemapKeepWrapped :
                    gba::kWsTilemapUnavailable;
            }
            return gba::kWsTilemapUnavailable;
        }
    }

    // Mirrored and authored-span-loop layers are remapped by
    // swordcraft3_margin_x before tilemap lookup. Loop coordinates remain
    // inside the reviewed authored prefix and need the normal wrapped lookup.
    // A layer is accepted wrapped only when the reviewed scene family proved
    // its tilemap is genuinely wider than the viewport and fully drawn
    // (battle arenas author their whole 512px map up front). 256px ring
    // buffers are never accepted wrapped -- the wrap is the opposite map seam
    // plus stale streamer columns, the repeated/garbage margin art this
    // policy exists to prevent. Everything else fails closed.
    if (bg >= 0 && bg <= 3 &&
        (s_wrap_ok_bg_mask & (1u << static_cast<unsigned>(bg))) != 0) {
        return gba::kWsTilemapKeepWrapped;
    }
    return gba::kWsTilemapUnavailable;
}

int swordcraft3_margin_x(int bg, int output_x, int output_y,
                         int* out_hw_x) {
    if (!out_hw_x || bg < 0 || bg > 3 ||
        ((s_mirrored_bg_mask | s_looped_bg_mask |
          s_natural_battle_bg_mask) &
         (1u << static_cast<unsigned>(bg))) == 0) {
        return 0;
    }

    const int hw_x = output_x - static_cast<int>(s_extra_left);
    const unsigned layer_bit = 1u << static_cast<unsigned>(bg);
    if (s_battle_scenery && bg == 0 &&
        (hw_x < 0 || hw_x >= static_cast<int>(kNativeWidth))) {
        const bool authored =
            ((s_looped_bg_mask | s_natural_battle_bg_mask) &
             layer_bit) != 0;
        const int top = authored ? kBattleRasterTop :
            kBattleReflectedPlayfieldTop;
        const int bottom = authored ? kBattleRasterBottom :
            kBattleReflectedPlayfieldBottom;
        if (output_y < top || output_y >= bottom) {
            // BG0 also owns the top/bottom HUD. Leave it transparent in those
            // margin rows rather than repeating screen-space panels; BG1 can
            // still supply the distant arena behind them.
            return -1;
        }
    }
    if ((s_natural_battle_bg_mask & layer_bit) != 0) {
        // The tilemap provider performs the finite-map boundary check. Do not
        // alter hw_x here: retaining it is what keeps every plane attached to
        // the guest's real camera instead of mirroring or repeating pixels.
        return 0;
    }
    if ((s_looped_bg_mask & layer_bit) != 0 &&
        (hw_x < 0 || hw_x >= static_cast<int>(kNativeWidth))) {
        const int authored_width = static_cast<int>(
            s_battle_authored_width_px[bg]);
        if (authored_width > 0) {
            int texture_x =
                hw_x + static_cast<int>(s_battle_hofs[bg]);
            texture_x %= authored_width;
            if (texture_x < 0) texture_x += authored_width;
            // Repeat only after the complete authored prefix. Subtracting the
            // scroll again yields a screen coordinate which the PPU maps back
            // to texture_x, so each plane retains its original parallax.
            *out_hw_x = texture_x - static_cast<int>(s_battle_hofs[bg]);
            return 1;
        }
    }
    if ((s_battle_hybrid_bg_mask & layer_bit) != 0 &&
        (hw_x < 0 || hw_x >= static_cast<int>(kNativeWidth))) {
        const unsigned map_width = s_battle_map_width_px[bg];
        const unsigned authored_width = s_battle_authored_width_px[bg];
        if (map_width != 0u && (map_width & (map_width - 1u)) == 0u) {
            const unsigned texture_x = static_cast<unsigned>(
                hw_x + static_cast<int>(s_battle_hofs[bg])) &
                (map_width - 1u);
            // Let the PPU consume the authentic tilemap entry. If this margin
            // falls in the game's repeated terminal fill, retain the reflected
            // edge fallback below so the arena remains filled without garbage.
            if (texture_x < authored_width) return 0;
        }
    }
    if (hw_x < 0) {
        *out_hw_x = -hw_x - 1;
        return 1;
    }
    if (hw_x >= static_cast<int>(kNativeWidth)) {
        *out_hw_x = static_cast<int>(kNativeWidth * 2u - 1u) - hw_x;
        return 1;
    }
    return 0;
}

void use_native_margin_policy() {
    s_battle_scenery = false;
    gba::g_ws_bg_margin_provider = nullptr;
    s_battle_hud_candidate = false;
    s_mirrored_bg_mask = 0;
    s_looped_bg_mask = 0;
    s_natural_battle_bg_mask = 0;
    s_wrap_ok_bg_mask = 0;
    s_true_map_bg_mask = 0;
    s_battle_hybrid_bg_mask = 0;
    s_battle_map_width_px = {};
    s_battle_authored_width_px = {};
    s_battle_hofs = {};
    s_extra_left = 0;
    s_extra_right = 0;
    s_expand_overworld_objects = false;
    s_expand_battle_objects = false;
    gba::g_ws_tilemap_provider = nullptr;
    gba::g_ws_bg_x_provider = nullptr;
    gba::g_ws_bg_x_provider_layers = 0;
    gba::g_ws_authored_margin_layers = 0;
    gba::g_ws_pillarbox = 0;
    gba::g_ws_pillarbox_left = 0;
    gba::g_ws_pillarbox_right = 0;
    gba::g_ws_obj_native_clip = 0;
    gba::g_ws_margin_occlusion_layers = 0;
}

void initialize_extended_view(std::uint32_t, std::uint32_t) {
    s_battle_scenery = false;
    s_battle_hud_candidate = false;
    // Fail closed until the first per-frame register snapshot proves that the
    // current scene has a reviewed margin continuation.
    s_mirrored_bg_mask = 0;
    s_looped_bg_mask = 0;
    s_natural_battle_bg_mask = 0;
    s_wrap_ok_bg_mask = 0;
    s_true_map_bg_mask = 0;
    s_battle_hybrid_bg_mask = 0;
    s_battle_map_width_px = {};
    s_battle_authored_width_px = {};
    s_battle_hofs = {};
    s_extra_left = 0;
    s_extra_right = 0;
    s_expand_overworld_objects = false;
    s_expand_battle_objects = false;
    // This callback is inert unless update_extended_view has positively
    // identified a reviewed world scene. Generated code consults it only at
    // the two exact game-owned culling instructions declared in the config.
    g_runtime_thumb_alu_imm_override =
        swordcraft3_world_alu_immediate;
    gba::g_ws_tilemap_provider = swordcraft3_margin_tilemap;
    gba::g_ws_bg_x_provider = swordcraft3_margin_x;
    gba::g_ws_bg_x_provider_layers = 0;
    gba::g_ws_authored_margin_layers = 0;
    gba::g_ws_pillarbox = 1;
    gba::g_ws_pillarbox_left = 0;
    gba::g_ws_pillarbox_right = 0;
    // Sprites the guest parked just off the native edge must never surface
    // in the margins (harmless at native width; the wide path checks it).
    gba::g_ws_obj_native_clip = 1;
    gba::g_ws_margin_occlusion_layers = 0;
}

void update_extended_view(const gbarecomp::ExtendedViewFrameInfo* frame) {
    s_battle_backdrop_period = 0;
    s_battle_span_valid = 0;
    s_battle_hud_candidate = false;
    s_presentation_frame = frame ? frame->frame_count + 1u : 0u;
    swordcraft3_true_map_update(frame);
    if (!frame || frame->view_width <= kNativeWidth) {
        use_native_margin_policy();
        return;
    }

    s_battle_map_width_px = {};
    s_battle_authored_width_px = {};
    s_battle_hofs = {};

    unsigned margin_layers = 0;
    unsigned mirrored_layers = 0;
    unsigned looped_layers = 0;
    unsigned natural_battle_layers = 0;
    unsigned wrap_ok_layers = 0;
    unsigned true_map_layers = 0;
    unsigned battle_hybrid_layers = 0;
    bool field_scene = false;
    bool battle_scene = false;
    const std::uint16_t dispcnt = read16(
        frame->io, frame->io_size, kDispcntOffset);
    const unsigned bg_mode = dispcnt & 0x7u;
    const bool forced_blank = (dispcnt & 0x80u) != 0;
    const char* critical_setting = std::getenv("SWORDCRAFT3_CRITICAL_WIDESCREEN");
    const bool critical_scene = (!critical_setting || std::strcmp(critical_setting, "0") != 0) &&
        swordcraft3::battle_critical_layout(dispcnt,
            read16(frame->io, frame->io_size, 8),
            read16(frame->io, frame->io_size, 10),
            read16(frame->io, frame->io_size, 12));

    // The game builds its field and battle scenes from single 256px screen
    // blocks used as scrolling ring buffers. A wrapped ring can never fill a
    // 262/284/320px view correctly: 240 visible columns leave at most 16 valid
    // off-screen columns in VRAM, so kept-wrapped field margins show the
    // opposite map seam plus stale streamer columns. Field BG descriptors also
    // retain the complete source map in guest memory; the Swordcraft adapter
    // resolves authentic neighboring tiles from that source. Layers without a
    // valid complete map keep the reviewed reflected-edge fallback. Battle
    // maps are measured from the live VRAM snapshot: terminal fill columns are
    // excluded. Natural mode consumes each finite map once in camera order;
    // the old loop and reflected policies remain explicit fallbacks.
    // Only the authenticated critical-hit Mode 1 family is also supported.
    // Other affine/bitmap layouts remain pillarboxed.
    if (!forced_blank && (bg_mode == 0 || critical_scene)) {
        std::uint16_t bgcnt[4]{};
        unsigned visible_layers = 0;
        for (unsigned bg = 0; bg < 4; ++bg) {
            if ((dispcnt & (0x0100u << bg)) == 0) continue;
            visible_layers |= 1u << bg;
            bgcnt[bg] = read16(
                frame->io, frame->io_size, kBgcntOffset + bg * 2u);
        }

        auto screen_block = [&](unsigned bg) {
            return static_cast<unsigned>((bgcnt[bg] >> 8) & 0x1Fu);
        };
        auto char_block = [&](unsigned bg) {
            return static_cast<unsigned>((bgcnt[bg] >> 2) & 0x3u);
        };

        // Field/cutscene family: four consecutive screen blocks 5..8. BG0 is
        // dialogue/status chrome and stays centered; BG1..BG3 form the
        // scrolling environment and prefer the game's complete source maps.
        field_scene = bg_mode == 0 && (visible_layers & 0xFu) == 0xFu &&
            screen_block(0) == 5u && screen_block(1) == 6u &&
            screen_block(2) == 7u && screen_block(3) == 8u;
        if (field_scene) {
            margin_layers |= 0xEu;
            true_map_layers = swordcraft3_true_map_layers() & 0xEu;
            mirrored_layers |= 0xEu & ~true_map_layers;
        }

        // Battle family: BG2 (char block 1, screen block 3) is the stable
        // signature. BG1/BG2 supply distant/effect planes, while BG0
        // multiplexes near arena art with the screen-space HUD.
        battle_scene = critical_scene || (bg_mode == 0 &&
            (visible_layers & (1u << 2)) != 0 &&
            char_block(2) == 1u && screen_block(2) == 3u);
        // Normal HUD uses BG0 screen/char block 0 at frame start. The final
        // frame still has to authenticate its blank gutters and separators;
        // this register family alone is not permission to paint over effects.
        s_battle_hud_candidate = battle_scene &&
            (visible_layers & 0x7u) == 0x7u && bgcnt[0] == 0u;
        if (battle_scene) {
            // Collect the complete battle layer stack first. BG0 multiplexes
            // the native HUD with the near forest/ground raster band; the
            // horizontal callback clips its HUD rows separately.
            if ((visible_layers & (1u << 0)) != 0) {
                margin_layers |= 1u << 0;
            }
            // The critical BG2 canvas already extrapolates signed screen X
            // through the shared affine renderer, with hardware wrap OFF.
            // Never measure it as a text map or apply the regular BG2 loop.
            if (!critical_scene && (visible_layers & (1u << 2)) != 0)
                margin_layers |= 1u << 2;
            if ((visible_layers & (1u << 1)) != 0 &&
                (bgcnt[1] & 0x4000u) != 0) {
                margin_layers |= 1u << 1;
            }

            // Measure the non-padding span of every battle plane. Natural mode
            // samples that finite span without wrapping; loop mode repeats it.
            // BG0's active arena descriptor is
            // installed by a VCOUNT write and is therefore not present in the
            // ordinary end-of-frame IO snapshot; use the verified descriptor
            // from that raster program when measuring it.
            const BattleMarginMode margin_mode = battle_margin_mode();
            for (unsigned bg = 0u; bg <= 2u; ++bg) {
                if ((margin_layers & (1u << bg)) == 0) continue;
                const std::uint16_t map_bgcnt =
                    bg == 0u ? kBattleRasterBg0Cnt : bgcnt[bg];
                const unsigned size = (map_bgcnt >> 14) & 0x3u;
                const unsigned map_width = (size & 1u) ? 512u : 256u;
                const unsigned measured_width = battle_authored_width(
                    frame, map_bgcnt);
                const unsigned authored_width = bg == 0u ?
                    (margin_mode == BattleMarginMode::Natural ?
                        map_width : kBattleRasterBg0LoopWidth) :
                    measured_width;
                if (authored_width == 0u || authored_width > map_width) {
                    mirrored_layers |= 1u << bg;
                    continue;
                }
                wrap_ok_layers |= 1u << bg;
                s_battle_map_width_px[bg] = map_width;
                s_battle_authored_width_px[bg] = authored_width;
                s_battle_hofs[bg] = read16(
                    frame->io, frame->io_size, 0x10u + bg * 4u) & 0x01FFu;
                if (margin_mode == BattleMarginMode::Natural) {
                    natural_battle_layers |= 1u << bg;
                } else if (margin_mode == BattleMarginMode::Loop) {
                    looped_layers |= 1u << bg;
                } else if (bg != 0u) {
                    // Preserve the previous authored-prefix/reflected-padding
                    // experiment exactly when the fallback setting is active.
                    battle_hybrid_layers |= 1u << bg;
                    mirrored_layers |= 1u << bg;
                } else {
                    mirrored_layers |= 1u << bg;
                }
            }
        }
    }

    // Scene-policy diagnostic: SWORDCRAFT3_WS_DEBUG=1 logs the register
    // layout whenever it changes, so unreviewed families can be identified
    // from a normal run instead of guessing at signatures.
    if (std::getenv("SWORDCRAFT3_WS_DEBUG")) {
        static std::uint32_t last_sig = 0xFFFFFFFFu;
        std::uint32_t sig = dispcnt;
        for (unsigned bg = 0; bg < 4; ++bg)
            sig ^= static_cast<std::uint32_t>(read16(
                frame->io, frame->io_size, kBgcntOffset + bg * 2u)) << (bg * 4);
        if (sig != last_sig) {
            last_sig = sig;
            std::fprintf(stderr,
                "[swordcraft3:ws] dispcnt=%04X bgcnt=%04X/%04X/%04X/%04X "
                "win0h=%04X win0v=%04X winin=%04X winout=%04X "
                "margins=%X true_map=%X mirrored=%X looped=%X natural=%X\n",
                dispcnt,
                read16(frame->io, frame->io_size, kBgcntOffset + 0),
                read16(frame->io, frame->io_size, kBgcntOffset + 2),
                read16(frame->io, frame->io_size, kBgcntOffset + 4),
                read16(frame->io, frame->io_size, kBgcntOffset + 6),
                read16(frame->io, frame->io_size, 0x40),
                read16(frame->io, frame->io_size, 0x44),
                read16(frame->io, frame->io_size, 0x48),
                read16(frame->io, frame->io_size, 0x4A),
                margin_layers, true_map_layers, mirrored_layers,
                looped_layers, natural_battle_layers);
        }
    }

    s_mirrored_bg_mask = mirrored_layers;
    s_looped_bg_mask = looped_layers;
    s_natural_battle_bg_mask = natural_battle_layers;
    s_wrap_ok_bg_mask = wrap_ok_layers;
    s_true_map_bg_mask = true_map_layers;
    s_battle_hybrid_bg_mask = battle_hybrid_layers;
    s_extra_left = frame->extra_left;
    s_extra_right = frame->extra_right;
    s_expand_overworld_objects = field_scene && true_map_layers != 0;
    s_battle_scenery = battle_scene;
    // This is a presentation-only critical-hit fix: preserve its prior guest
    // culling and native OBJ clip rather than changing simulation behavior.
    s_expand_battle_objects = battle_scene && !critical_scene;
    gba::g_ws_bg_margin_provider = battle_scene && battle_layer_fix_enabled() ?
        swordcraft3_battle_margin : nullptr;
    gba::g_ws_tilemap_provider = swordcraft3_margin_tilemap;
    gba::g_ws_bg_x_provider = swordcraft3_margin_x;
    // Natural battle layers keep unmodified coordinates. Only BG0 needs the
    // x callback so its screen-space HUD rows stay centered in the margins.
    gba::g_ws_bg_x_provider_layers = mirrored_layers | looped_layers |
        (natural_battle_layers & 1u);
    gba::g_ws_authored_margin_layers = margin_layers ? 1 : 0;
    gba::g_ws_pillarbox = margin_layers ? 0 : 1;
    gba::g_ws_pillarbox_left = 0;
    gba::g_ws_pillarbox_right = 0;
    // Authenticated field maps and reviewed battle arenas may show objects that
    // the widened guest OAM builder now authors. HUD-only and unsupported
    // layouts retain the conservative native-width clip.
    gba::g_ws_obj_native_clip =
        (s_expand_overworld_objects || s_expand_battle_objects) ? 0 : 1;
    // In true-map fields, transparent/absent pixels from the world layers are
    // deliberate black room/transition boundaries. Ask the shared compositor
    // to keep those boundaries above UI portraits and OBJ without treating BG0
    // screen-space artwork as world coverage.
    gba::g_ws_margin_occlusion_layers =
        s_expand_overworld_objects ? true_map_layers : 0;

    // The runtime publishes this snapshot at the completed-frame boundary.
    // The selected policy governs the framebuffer rendered next, whose dump
    // label is frame_count + 1. Attribute telemetry to that image explicitly
    // so one-frame display-mode transitions are not compared out of phase.
    const std::uint64_t audit_capture_frame = frame->frame_count + 1u;
    if (should_emit_audit_frame(audit_capture_frame)) {
        std::uint16_t bgcnt[4]{};
        std::uint16_t hofs[4]{};
        std::uint16_t vofs[4]{};
        unsigned visible_layers = 0;
        for (unsigned bg = 0; bg < 4; ++bg) {
            bgcnt[bg] = read16(
                frame->io, frame->io_size, kBgcntOffset + bg * 2u);
            hofs[bg] = read16(
                frame->io, frame->io_size, 0x10u + bg * 4u) & 0x01FFu;
            vofs[bg] = read16(
                frame->io, frame->io_size, 0x12u + bg * 4u) & 0x01FFu;
            if ((dispcnt & (0x0100u << bg)) != 0)
                visible_layers |= 1u << bg;
        }
        const char* policy = forced_blank ? "forced_blank" :
            critical_scene ? "battle_critical" :
            bg_mode != 0 ? "unsupported_mode" :
            battle_scene && natural_battle_layers ? "battle_natural" :
            battle_scene && looped_layers ? "battle_loop" :
            battle_scene && battle_hybrid_layers ? "battle_hybrid" :
            battle_scene ? "battle_reflect" :
            field_scene && true_map_layers ? "field_true_map" :
            field_scene ? "field_reflect" : "pillarbox";
        std::fprintf(stderr,
            "swordcraft3_widescreen_frame={\"frame\":%llu,"
            "\"policy_observed_frame\":%llu,"
            "\"view_width\":%u,\"extra_left\":%u,\"extra_right\":%u,"
            "\"policy\":\"%s\",\"dispcnt\":%u,\"bg_mode\":%u,"
            "\"forced_blank\":%s,\"visible_layers\":%u,"
            "\"margin_layers\":%u,\"mirrored_layers\":%u,"
            "\"looped_layers\":%u,"
            "\"natural_battle_layers\":%u,"
            "\"true_map_layers\":%u,\"wrapped_layers\":%u,"
            "\"battle_hybrid_layers\":%u,"
            "\"battle_authored_widths\":[%u,%u,%u,%u],"
            "\"pillarbox\":%s,"
            "\"obj_native_clip\":%s,\"overworld_objects_expanded\":%s,"
            "\"battle_objects_expanded\":%s,"
            "\"margin_occlusion_layers\":%u,"
            "\"bgcnt\":[%u,%u,%u,%u],\"hofs\":[%u,%u,%u,%u],"
            "\"vofs\":[%u,%u,%u,%u],"
            "\"winin\":%u,\"winout\":%u}\n",
            static_cast<unsigned long long>(audit_capture_frame),
            static_cast<unsigned long long>(frame->frame_count),
            frame->view_width, frame->extra_left, frame->extra_right,
            policy, dispcnt, bg_mode, forced_blank ? "true" : "false",
            visible_layers, margin_layers, mirrored_layers, looped_layers,
            natural_battle_layers,
            true_map_layers, wrap_ok_layers,
            battle_hybrid_layers,
            s_battle_authored_width_px[0], s_battle_authored_width_px[1],
            s_battle_authored_width_px[2], s_battle_authored_width_px[3],
            margin_layers ? "false" : "true",
            gba::g_ws_obj_native_clip ? "true" : "false",
            s_expand_overworld_objects ? "true" : "false",
            s_expand_battle_objects ? "true" : "false",
            gba::g_ws_margin_occlusion_layers,
            bgcnt[0], bgcnt[1], bgcnt[2], bgcnt[3],
            hofs[0], hofs[1], hofs[2], hofs[3],
            vofs[0], vofs[1], vofs[2], vofs[3],
            read16(frame->io, frame->io_size, 0x48),
            read16(frame->io, frame->io_size, 0x4A));
    }
}

}  // namespace

void configure_swordcraft3_adaptive_widescreen(gbarecomp::RunOptions& opts) {
    gba::g_ws_frame_postprocess = swordcraft3_postprocess_wide_frame;
    const bool whole_arena_view = use_whole_arena_view();
    opts.max_view_width = whole_arena_view ?
        kWholeArenaWidth : kWide2To1Width;
    opts.max_resize_view_width = opts.max_view_width;
    opts.resize_driven_view = true;
    opts.widescreen_view_width = kFixed16x9Width;
    opts.launcher_aspect_labels = whole_arena_view ?
        kAspectLabels : kLegacyAspectLabels;
    opts.launcher_aspect_view_widths = whole_arena_view ?
        kAspectWidths : kLegacyAspectWidths;
    opts.launcher_num_aspects = whole_arena_view ?
        static_cast<int>(sizeof(kAspectWidths) / sizeof(kAspectWidths[0])) :
        static_cast<int>(sizeof(kLegacyAspectWidths) /
                         sizeof(kLegacyAspectWidths[0]));
    opts.launcher_expose_widescreen = true;
    opts.launcher_expose_adaptive_view = true;
    opts.extended_view_init = initialize_extended_view;
    opts.extended_view_frame = update_extended_view;
}
