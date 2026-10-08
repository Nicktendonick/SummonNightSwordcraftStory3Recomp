#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "host_window.h"
#include "debug_capture_policy.h"
#include "recomp_runtime_ui.h"
#include "../recomp-ui/src/common/recomp_runtime_ui_internal.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (0)
// No guest CPU is run by this host-input test.
struct DispatchEntry { uint32_t addr; uint8_t thumb, resume; void (*fn)(void); };
extern "C" const DispatchEntry kDispatchTable[1] = {};
extern "C" const unsigned kDispatchTableLen = 0;
extern "C" const DispatchEntry kBiosDispatchTable[1] = {};
extern "C" const unsigned kBiosDispatchTableLen = 0;
int main() {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    // Isolate this process from real controllers; never send input to them.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
    SDL_SetHint(SDL_HINT_DIRECTINPUT_ENABLED, "0");
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_RENDER_DRIVER", "software", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0);
    const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 0);
    CHECK(index >= 0 && SDL_IsGameController(index));
    SDL_Joystick* joystick = SDL_JoystickOpen(index);
    CHECK(joystick);
    gbarecomp::HostWindow window;
    // HostWindow uses the C-runtime environment; SDL's DLL may use a different
    // Windows CRT and its SDL_setenv does not update this module's getenv cache.
#ifdef _WIN32
    _putenv_s("GBARECOMP_SCREEN", "frontlit");
#else
    setenv("GBARECOMP_SCREEN", "frontlit", 1);
#endif
    CHECK(window.open(1, 240, 160, "Synthetic input test"));
    CHECK(window.screen_model() == 2); // getter reports the actual boot/environment model
#ifdef _WIN32
    _putenv_s("GBARECOMP_SCREEN", "");
#else
    unsetenv("GBARECOMP_SCREEN");
#endif
    CHECK(window.set_screen_model(0));
    CHECK(window.screen_model() == 0);
    int screen_writes = 0;
    CHECK(!window.set_screen_model(4, [&] { ++screen_writes; return false; }));
    CHECK(screen_writes == 1 && window.screen_model() == 0);
    CHECK(!window.set_screen_model(99, [&] { ++screen_writes; return true; }));
    CHECK(screen_writes == 1 && window.screen_model() == 0);
    CHECK(!window.set_screen_model(3, []() -> bool { throw std::runtime_error("save failure"); }));
    CHECK(window.screen_model() == 0);
    CHECK(window.set_screen_model(0, [&] { ++screen_writes; return true; }));
    CHECK(screen_writes == 2); // same-model choice can still persist
    auto root = std::filesystem::current_path() / "validation/portable-runtime-input";
    std::filesystem::create_directories(root);
    {
        std::ofstream f(root/"custom.ini");
        // Deliberately not config.ini: test the actual runtime override.
        f << "[Launcher]\nplayer_pad_0=3\nassist_rewind_pad=0\nassist_fast_pad=111\n";
    }
    window.load_input_config(root.string().c_str(), true, 4, "custom.ini", "missing.ini");
    auto sample = [&]() {
        SDL_JoystickUpdate(); return window.pump();
    };
    // F10 must work without enabling debugger F2..F9 or taking over save slots.
    auto capture_env = [](const char* name, const char* value) {
#ifdef _WIN32
        _putenv_s(name, value);
#else
        if (*value) setenv(name, value, 1); else unsetenv(name);
#endif
    };
    auto hotkey = [&](SDL_Keycode key, Uint16 mods = 0, int repeat = 0) {
        SDL_Event e{};
        e.type = SDL_KEYDOWN;
        e.key.keysym.sym = key;
        e.key.keysym.scancode = SDL_GetScancodeFromKey(key);
        e.key.keysym.mod = mods;
        e.key.repeat = repeat;
        CHECK(SDL_PushEvent(&e) == 1);
        auto event = sample();
        e.type = SDL_KEYUP;
        CHECK(SDL_PushEvent(&e) == 1);
        sample();
        return event;
    };
    CHECK(!gbarecomp::debug_capture_enabled(false, nullptr));
    CHECK(!gbarecomp::debug_capture_enabled(false, ""));
    CHECK(gbarecomp::debug_capture_enabled(false, "Captures/test"));
    CHECK(gbarecomp::debug_capture_enabled(true, nullptr));
    CHECK(gbarecomp::debug_capture_enabled(true, ""));
    capture_env("GBARECOMP_VISIBLE_DEBUGGER", "");
    capture_env("GBARECOMP_DEBUG_CAPTURE_DIR", "");
    CHECK(!hotkey(SDLK_F10).debug_export);
    capture_env("GBARECOMP_DEBUG_CAPTURE_DIR", "Captures/test");
    CHECK(hotkey(SDLK_F10).debug_export);
    CHECK(!hotkey(SDLK_F10, 0, 1).debug_export); // holding F10 is not a capture flood
    CHECK(!sample().debug_export);
    for (SDL_Keycode key = SDLK_F1; key <= SDLK_F9; ++key) {
        const int slot = key - SDLK_F1 + 1;
        auto load = hotkey(key);
        CHECK(load.load_slot == slot && load.save_slot == 0);
        CHECK(load.debug_layer_mask == -1 && !load.debug_step_frame && !load.toggle_pause);
        auto save = hotkey(key, KMOD_SHIFT);
        CHECK(save.save_slot == slot && save.load_slot == 0);
    }
    capture_env("GBARECOMP_VISIBLE_DEBUGGER", "0");
    CHECK(hotkey(SDLK_F10).debug_export);
    capture_env("GBARECOMP_DEBUG_CAPTURE_DIR", "");
    CHECK(!hotkey(SDLK_F10).debug_export);
    capture_env("GBARECOMP_VISIBLE_DEBUGGER", "1");
    CHECK(hotkey(SDLK_F10).debug_export);
    CHECK(hotkey(SDLK_F3).debug_layer_mask == 1);
    CHECK(hotkey(SDLK_F9).debug_step_frame);
    capture_env("GBARECOMP_VISIBLE_DEBUGGER", "");
    SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, 1);
    CHECK((sample().keyinput & 1) != 0); // original A mapping no longer active
    SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, 0);
    SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_X, 1);
    CHECK((sample().keyinput & 1) == 0); // remapped X now presses GBA A
    SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_X, 0);
    CHECK((sample().keyinput & 1) != 0);
    SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767);
    CHECK(sample().fast_forward);
    SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
    CHECK(!sample().fast_forward);
    // Real SDL event translation, with no desktop input or game execution.
    int actions = 0;
    RecompRuntimeUiStandardConfig menu{};
    menu.features = RECOMP_RUNTIME_UI_STANDARD_CLOSE;
    menu.menu.callbacks.context = &actions;
    menu.menu.callbacks.run_action = [](void* p, const RecompRuntimeUiItem*) {
        ++*static_cast<int*>(p); return 1;
    };
    auto* ui = recomp_runtime_ui_create_standard(&menu);
    CHECK(ui);
    window.set_runtime_ui(ui);
    auto keyboard = [&](SDL_Scancode code, int repeat = 0) {
        SDL_Event e{}; e.type=SDL_KEYDOWN; e.key.keysym.scancode=code; e.key.repeat=repeat;
        CHECK(SDL_PushEvent(&e)==1); CHECK(!sample().quit);
        e.type=SDL_KEYUP; e.key.repeat=0; CHECK(SDL_PushEvent(&e)==1); CHECK(!sample().quit);
    };
    auto controller = [&](Uint8 button) {
        SDL_Event e{}; e.type=SDL_CONTROLLERBUTTONDOWN; e.cbutton.button=button;
        CHECK(SDL_PushEvent(&e)==1); sample();
        e.type=SDL_CONTROLLERBUTTONUP; CHECK(SDL_PushEvent(&e)==1); sample();
    };
    keyboard(SDL_SCANCODE_ESCAPE, 1); CHECK(!recomp_runtime_ui_is_open(ui));
    keyboard(SDL_SCANCODE_ESCAPE); CHECK(recomp_runtime_ui_is_open(ui));
    keyboard(SDL_SCANCODE_RETURN); keyboard(SDL_SCANCODE_RETURN);
    CHECK(recomp_runtime_ui_confirmation_pending(ui) && actions==0);
    keyboard(SDL_SCANCODE_RETURN); CHECK(!recomp_runtime_ui_confirmation_pending(ui) && actions==0);
    controller(SDL_CONTROLLER_BUTTON_A);
    CHECK(recomp_runtime_ui_confirmation_pending(ui));
    controller(SDL_CONTROLLER_BUTTON_DPAD_RIGHT); controller(SDL_CONTROLLER_BUTTON_A);
    CHECK(actions==1 && !recomp_runtime_ui_confirmation_pending(ui));
    controller(SDL_CONTROLLER_BUTTON_A); controller(SDL_CONTROLLER_BUTTON_GUIDE);
    CHECK(!recomp_runtime_ui_confirmation_pending(ui) && recomp_runtime_ui_is_open(ui));
    controller(SDL_CONTROLLER_BUTTON_GUIDE); CHECK(!recomp_runtime_ui_is_open(ui));
    // No Resume item: Escape still closes straight from a nested section.
    keyboard(SDL_SCANCODE_ESCAPE); keyboard(SDL_SCANCODE_RETURN);
    CHECK(ui->in_section);
    keyboard(SDL_SCANCODE_ESCAPE); CHECK(!recomp_runtime_ui_is_open(ui));
    window.set_runtime_ui(nullptr);
    recomp_runtime_ui_destroy(ui);
    struct MenuState { RecompRuntimeUi* ui{}; bool paused = false; int resumed = 0; int destructive = 0; } state;
    menu.features = RECOMP_RUNTIME_UI_STANDARD_RESUME | RECOMP_RUNTIME_UI_STANDARD_PAUSE |
        RECOMP_RUNTIME_UI_STANDARD_RESET | RECOMP_RUNTIME_UI_STANDARD_CLOSE |
        RECOMP_RUNTIME_UI_STANDARD_FULLSCREEN | RECOMP_RUNTIME_UI_STANDARD_VOLUME;
    menu.menu.callbacks.context = &state;
    menu.menu.callbacks.run_action = [](void* p, const RecompRuntimeUiItem* item) {
        auto& s = *static_cast<MenuState*>(p);
        const std::string key = item->key;
        if (key == RECOMP_RUNTIME_UI_KEY_RESUME) {
            s.paused = false; ++s.resumed; recomp_runtime_ui_close(s.ui);
        } else if (key == RECOMP_RUNTIME_UI_KEY_PAUSE) s.paused = true;
        else ++s.destructive;
        return 1;
    };
    ui = recomp_runtime_ui_create_standard(&menu); CHECK(ui); state.ui = ui;
    window.set_runtime_ui(ui);
    // Top level and every actual section, both ordinary and explicit Pause.
    for (int section = -1; section < static_cast<int>(ui->section_count); ++section) {
        for (bool paused : {false, true}) {
            keyboard(SDL_SCANCODE_ESCAPE); CHECK(recomp_runtime_ui_is_open(ui));
            if (section >= 0) recomp_runtime_ui_enter_section(ui, section);
            if (paused) CHECK(recomp_runtime_ui_activate(ui, RECOMP_RUNTIME_UI_KEY_PAUSE));
            CHECK(state.paused == paused);
            const int resumed = state.resumed;
            keyboard(SDL_SCANCODE_ESCAPE, 1);
            CHECK(recomp_runtime_ui_is_open(ui) && state.resumed == resumed);
            keyboard(SDL_SCANCODE_ESCAPE);
            CHECK(!recomp_runtime_ui_is_open(ui) && !state.paused && state.resumed == resumed + 1);
            keyboard(SDL_SCANCODE_ESCAPE, 1);
            CHECK(!recomp_runtime_ui_is_open(ui) && state.resumed == resumed + 1);
        }
    }
    for (const char* key : {RECOMP_RUNTIME_UI_KEY_RESET, RECOMP_RUNTIME_UI_KEY_CLOSE}) {
        keyboard(SDL_SCANCODE_ESCAPE);
        CHECK(recomp_runtime_ui_activate(ui, RECOMP_RUNTIME_UI_KEY_PAUSE));
        CHECK(recomp_runtime_ui_activate(ui, key));
        CHECK(recomp_runtime_ui_confirmation_pending(ui));
        keyboard(SDL_SCANCODE_ESCAPE);
        CHECK(!recomp_runtime_ui_confirmation_pending(ui) && recomp_runtime_ui_is_open(ui));
        CHECK(state.paused && state.destructive == 0);
        keyboard(SDL_SCANCODE_ESCAPE);
        CHECK(!state.paused && !recomp_runtime_ui_is_open(ui));
    }
    keyboard(SDL_SCANCODE_ESCAPE); recomp_runtime_ui_enter_section(ui, 0);
    controller(SDL_CONTROLLER_BUTTON_B);
    CHECK(recomp_runtime_ui_is_open(ui) && !ui->in_section); // controller Back unchanged
#if defined(RECOMP_RUNTIME_UI_HAS_TEXT)
    ui->editing_text = 1; // model the text backend owning this key
    const int resumed = state.resumed;
    keyboard(SDL_SCANCODE_ESCAPE);
    CHECK(recomp_runtime_ui_is_open(ui) && state.resumed == resumed);
    ui->editing_text = 0;
#endif
    keyboard(SDL_SCANCODE_ESCAPE); CHECK(!recomp_runtime_ui_is_open(ui));
    window.set_audio_enabled(true); CHECK(window.audio_enabled());
    window.set_game_paused(true); CHECK(window.audio_enabled()); // pause is not a mute preference
    window.set_audio_enabled(false); window.set_game_paused(false); CHECK(!window.audio_enabled());
    window.set_game_paused(true); window.set_audio_enabled(true); CHECK(window.audio_enabled());
    window.set_game_paused(false); CHECK(window.audio_enabled());
    // Exercise actual presentation resources, not framebuffer/pixel assertions.
    // The input buffer is synthetic; no guest execution or real player save.
    std::vector<uint8_t> frame(384 * 160 * 3, 64);
    CHECK(window.scaling_filter() == 0 && window.screen_effect() == 0);
    CHECK(window.screen_effect_strength() == 35);
    for (int width : {240, 284, 384}) {
        CHECK(window.set_surface_size(width, 160));
        for (int scaling = 0; scaling < 4; ++scaling) {
            window.set_scaling_filter(scaling);
            CHECK(window.scaling_filter() == scaling);
            CHECK(window.linear_filter() == (scaling == 1));
            for (int effect = 0; effect < 3; ++effect) {
                window.set_screen_effect(effect, 50);
                CHECK(window.screen_effect() == effect && window.screen_effect_strength() == 50);
                for (int model : {0, 1, 2, 3, 4}) {
                    CHECK(window.set_screen_model(model));
                    CHECK(window.screen_model() == model);
                    CHECK(window.scaling_filter() == scaling && window.screen_effect() == effect);
                    CHECK(window.screen_effect_strength() == 50);
                    const auto before = window.filter_stats();
                    window.present(frame.data());
                    const auto after = window.filter_stats();
                    CHECK(after.frames == before.frames + 1);
                    CHECK(after.colour_frames == before.colour_frames + (model != 0));
                    CHECK(after.smooth_frames == before.smooth_frames + (scaling == 3));
                    CHECK(after.effect_frames == before.effect_frames + (effect != 0));
                    CHECK(after.fallback_frames == before.fallback_frames);
                }
            }
        }
    }
    CHECK(window.set_surface_size(240, 160));
    window.set_scaling_filter(2);
    window.set_screen_effect(0, 35);
    SDL_Window* test_window = SDL_GetWindowFromID(1);
    CHECK(test_window);
    SDL_SetWindowSize(test_window, 800, 600); // non-integer presentation scale
    const auto before_sharp = window.filter_stats();
    window.present(frame.data());
    CHECK(window.filter_stats().sharp_frames == before_sharp.sharp_frames + 1);
    window.set_screen_effect(2, 0);
    const auto zero_strength = window.filter_stats();
    window.present(frame.data());
    CHECK(window.filter_stats().effect_frames == zero_strength.effect_frames);
    window.set_linear_filter(true); CHECK(window.scaling_filter() == 1);
    window.set_linear_filter(false); CHECK(window.scaling_filter() == 0);
    recomp_runtime_ui_open(ui);
    window.present(frame.data()); CHECK(recomp_runtime_ui_is_open(ui));
    window.set_scaling_filter(3); window.set_screen_effect(1, 35);
    for (Uint32 kind : {SDL_RENDER_TARGETS_RESET, SDL_RENDER_DEVICE_RESET}) {
        SDL_Event reset{}; reset.type = kind;
        CHECK(SDL_PushEvent(&reset) == 1); sample();
        const auto before = window.filter_stats();
        window.present(frame.data());
        const auto after = window.filter_stats();
        CHECK(after.smooth_frames == before.smooth_frames + 1);
        CHECK(after.effect_frames == before.effect_frames + 1);
        CHECK(window.screen_model() == 4 && after.colour_frames == before.colour_frames + 1);
        CHECK(after.fallback_frames == before.fallback_frames);
        CHECK(recomp_runtime_ui_is_open(ui));
    }
    window.set_runtime_ui(nullptr);
    recomp_runtime_ui_destroy(ui);
    window.close();
    SDL_JoystickClose(joystick);
    SDL_JoystickDetachVirtual(index);
    SDL_Quit();
    CHECK(!window.set_screen_model(3));
    std::cout << "PASS: runtime input, menu/audio, 180 colour/scaler/effect/width combinations, transaction rejection, device reset, fractional Sharp, zero strength, menu overlay\n";
    return 0;
}
