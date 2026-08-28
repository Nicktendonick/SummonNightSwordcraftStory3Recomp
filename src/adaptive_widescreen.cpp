#include "adaptive_widescreen.h"

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
constexpr std::size_t kDispcntOffset = 0x00;
constexpr std::size_t kBgcntOffset = 0x08;

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
unsigned s_wrap_ok_bg_mask = 0;
unsigned s_true_map_bg_mask = 0;
unsigned s_extra_left = 0;
unsigned s_extra_right = 0;
bool s_expand_overworld_objects = false;

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

int swordcraft3_overworld_alu_immediate(
        std::uint32_t instruction_pc, std::uint32_t original_value,
        std::uint32_t* out_value) {
    if (!s_expand_overworld_objects || !out_value) return 0;
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
    if (near_black * 10u < kNativePixels * 9u) return;

    for (std::uint32_t y = 0; y < 160u; ++y) {
        std::uint8_t* row = rgb + static_cast<std::size_t>(y) * width * 3u;
        std::memset(row, 0, static_cast<std::size_t>(extra_left) * 3u);
        std::memset(row + static_cast<std::size_t>(extra_left + kNativeWidth) * 3u,
                    0, static_cast<std::size_t>(extra_right) * 3u);
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

    // Mirrored layers are remapped into the native viewport by
    // swordcraft3_margin_x before tilemap lookup and never reach this hook.
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

int swordcraft3_margin_x(int bg, int output_x, int, int* out_hw_x) {
    if (!out_hw_x || bg < 0 || bg > 3 ||
        (s_mirrored_bg_mask & (1u << static_cast<unsigned>(bg))) == 0) {
        return 0;
    }

    const int hw_x = output_x - static_cast<int>(s_extra_left);
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
    s_mirrored_bg_mask = 0;
    s_wrap_ok_bg_mask = 0;
    s_true_map_bg_mask = 0;
    s_extra_left = 0;
    s_extra_right = 0;
    s_expand_overworld_objects = false;
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
    // Fail closed until the first per-frame register snapshot proves that the
    // current scene has a reviewed margin continuation.
    s_mirrored_bg_mask = 0;
    s_wrap_ok_bg_mask = 0;
    s_true_map_bg_mask = 0;
    s_extra_left = 0;
    s_extra_right = 0;
    s_expand_overworld_objects = false;
    // This callback is inert unless update_extended_view has positively
    // identified a true-map overworld. Generated code consults it only at the
    // two exact game-owned culling instructions declared in the config.
    g_runtime_thumb_alu_imm_override =
        swordcraft3_overworld_alu_immediate;
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
    swordcraft3_true_map_update(frame);
    if (!frame || frame->view_width <= kNativeWidth) {
        use_native_margin_policy();
        return;
    }

    unsigned margin_layers = 0;
    unsigned mirrored_layers = 0;
    unsigned wrap_ok_layers = 0;
    unsigned true_map_layers = 0;
    bool field_scene = false;
    bool battle_scene = false;
    const std::uint16_t dispcnt = read16(
        frame->io, frame->io_size, kDispcntOffset);
    const unsigned bg_mode = dispcnt & 0x7u;
    const bool forced_blank = (dispcnt & 0x80u) != 0;

    // The game builds its field and battle scenes from single 256px screen
    // blocks used as scrolling ring buffers. A wrapped ring can never fill a
    // 262/284px view correctly: 240 visible columns leave at most 16 valid
    // off-screen columns in VRAM, so kept-wrapped field margins show the
    // opposite map seam plus stale streamer columns. Field BG descriptors also
    // retain the complete source map in guest memory; the Swordcraft adapter
    // resolves authentic neighboring tiles from that source. Layers without a
    // valid complete map keep the reviewed reflected-edge fallback. Battle uses
    // the same safe reflection fallback. Route-scale BG isolation
    // proved that its nominally 512px BG1 still exposes empty columns as the
    // arena camera moves; map dimensions alone are not proof that both margins
    // contain authored scenery. Affine/bitmap modes remain pillarboxed.
    if (!forced_blank && bg_mode == 0) {
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
        field_scene = (visible_layers & 0xFu) == 0xFu &&
            screen_block(0) == 5u && screen_block(1) == 6u &&
            screen_block(2) == 7u && screen_block(3) == 8u;
        if (field_scene) {
            margin_layers |= 0xEu;
            true_map_layers = swordcraft3_true_map_layers() & 0xEu;
            mirrored_layers |= 0xEu & ~true_map_layers;
        }

        // Battle family: BG2 (char block 1, screen block 3) is the arena.
        // Its ring buffer leaves the columns just outside the 240px viewport
        // empty, so it reflects the nearest arena pixels the same way.
        // BG0/BG1 carry the native-width HUD and remain centered.
        battle_scene = (visible_layers & (1u << 2)) != 0 &&
            char_block(2) == 1u && screen_block(2) == 3u;
        if (battle_scene) {
            margin_layers |= 1u << 2;
            mirrored_layers |= 1u << 2;
            // BG1 carries the distant arena art on a 512px tilemap, but the
            // deterministic battle route found undrawn columns at the left
            // native boundary after camera motion. Reflect it instead of
            // treating nominal map width as evidence of valid world content.
            if ((visible_layers & (1u << 1)) != 0 &&
                (bgcnt[1] & 0x4000u) != 0) {
                margin_layers |= 1u << 1;
                mirrored_layers |= 1u << 1;
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
                "margins=%X true_map=%X mirrored=%X\n",
                dispcnt,
                read16(frame->io, frame->io_size, kBgcntOffset + 0),
                read16(frame->io, frame->io_size, kBgcntOffset + 2),
                read16(frame->io, frame->io_size, kBgcntOffset + 4),
                read16(frame->io, frame->io_size, kBgcntOffset + 6),
                read16(frame->io, frame->io_size, 0x40),
                read16(frame->io, frame->io_size, 0x44),
                read16(frame->io, frame->io_size, 0x48),
                read16(frame->io, frame->io_size, 0x4A),
                margin_layers, true_map_layers, mirrored_layers);
        }
    }

    s_mirrored_bg_mask = mirrored_layers;
    s_wrap_ok_bg_mask = wrap_ok_layers;
    s_true_map_bg_mask = true_map_layers;
    s_extra_left = frame->extra_left;
    s_extra_right = frame->extra_right;
    s_expand_overworld_objects = field_scene && true_map_layers != 0;
    gba::g_ws_tilemap_provider = swordcraft3_margin_tilemap;
    gba::g_ws_bg_x_provider = swordcraft3_margin_x;
    gba::g_ws_bg_x_provider_layers = mirrored_layers;
    gba::g_ws_authored_margin_layers = margin_layers ? 1 : 0;
    gba::g_ws_pillarbox = margin_layers ? 0 : 1;
    gba::g_ws_pillarbox_left = 0;
    gba::g_ws_pillarbox_right = 0;
    // Authenticated overworld source maps may show the objects that the widened
    // guest OAM builder now authors. Every other scene keeps the conservative
    // native-width clip, including battles that share this renderer.
    gba::g_ws_obj_native_clip = s_expand_overworld_objects ? 0 : 1;
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
            bg_mode != 0 ? "unsupported_mode" :
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
            "\"true_map_layers\":%u,\"wrapped_layers\":%u,"
            "\"pillarbox\":%s,"
            "\"obj_native_clip\":%s,\"overworld_objects_expanded\":%s,"
            "\"margin_occlusion_layers\":%u,"
            "\"bgcnt\":[%u,%u,%u,%u],\"hofs\":[%u,%u,%u,%u],"
            "\"vofs\":[%u,%u,%u,%u],"
            "\"winin\":%u,\"winout\":%u}\n",
            static_cast<unsigned long long>(audit_capture_frame),
            static_cast<unsigned long long>(frame->frame_count),
            frame->view_width, frame->extra_left, frame->extra_right,
            policy, dispcnt, bg_mode, forced_blank ? "true" : "false",
            visible_layers, margin_layers, mirrored_layers, true_map_layers,
            wrap_ok_layers,
            margin_layers ? "false" : "true",
            gba::g_ws_obj_native_clip ? "true" : "false",
            s_expand_overworld_objects ? "true" : "false",
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
    opts.max_view_width = kFixed16x9Width;
    opts.max_resize_view_width = kFixed16x9Width;
    opts.resize_driven_view = true;
    opts.widescreen_view_width = kFixed16x9Width;
    opts.launcher_expose_widescreen = true;
    opts.launcher_expose_adaptive_view = true;
    opts.extended_view_init = initialize_extended_view;
    opts.extended_view_frame = update_extended_view;
}
