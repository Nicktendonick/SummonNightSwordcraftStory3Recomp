#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "host_window.h"
#include "recomp_runtime_ui.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
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
    CHECK(window.open(1, 240, 160, "Synthetic input test"));
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
        CHECK(SDL_PushEvent(&e)==1); sample();
        e.type=SDL_KEYUP; e.key.repeat=0; CHECK(SDL_PushEvent(&e)==1); sample();
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
    window.set_audio_enabled(true); CHECK(window.audio_enabled());
    window.set_game_paused(true); CHECK(window.audio_enabled()); // pause is not a mute preference
    window.set_audio_enabled(false); window.set_game_paused(false); CHECK(!window.audio_enabled());
    window.set_game_paused(true); window.set_audio_enabled(true); CHECK(window.audio_enabled());
    window.set_game_paused(false); CHECK(window.audio_enabled());
    window.set_runtime_ui(nullptr);
    recomp_runtime_ui_destroy(ui);
    window.close();
    SDL_JoystickClose(joystick);
    SDL_JoystickDetachVirtual(index);
    SDL_Quit();
    std::cout << "PASS: runtime input config, controller remap/release, assist trigger, Esc/Guide navigation, keyboard/controller confirmation, pause/audio preference\n";
    return 0;
}
