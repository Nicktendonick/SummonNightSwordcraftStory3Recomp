#include "recomp_runtime_ui_internal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "beta_session.h"
#include "runtime_menu_preferences.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (0)

struct State { int actions = 0; bool enabled = true; std::string last; int auto_pause = 1; };
int action(void* p, const RecompRuntimeUiItem* item) {
    auto& s = *static_cast<State*>(p); ++s.actions; s.last = item->key; return 1;
}
int enabled(void* p, const RecompRuntimeUiItem*) { return static_cast<State*>(p)->enabled; }
void key(RecompRuntimeUi* ui, RecompRuntimeUiInput input, int repeat = 0) {
    recomp_runtime_ui_handle_input(ui, input, 1, repeat);
}
int main() {
    State s;
    RecompRuntimeUiStandardConfig c{};
    c.menu.theme = "storybook";
    c.menu.title = "Swordcraft Story 3";
    c.menu.subtitle = "Craftknights! Artisans of weapons. Masters of the sword.";
    c.menu.callbacks.context = &s;
    c.menu.callbacks.run_action = action;
    c.menu.callbacks.is_enabled = enabled;
    c.menu.callbacks.get_value = [](void* p, const RecompRuntimeUiItem* item, int* out) {
        if (std::string(item->key) != RECOMP_RUNTIME_UI_KEY_PAUSE_ON_OPEN) return 0;
        *out = static_cast<State*>(p)->auto_pause; return 1;
    };
    c.menu.callbacks.set_value = [](void* p, const RecompRuntimeUiItem* item, int value) {
        if (std::string(item->key) != RECOMP_RUNTIME_UI_KEY_PAUSE_ON_OPEN) return 0;
        static_cast<State*>(p)->auto_pause = value; return 1;
    };
    c.features = RECOMP_RUNTIME_UI_STANDARD_PAUSE | RECOMP_RUNTIME_UI_STANDARD_RESET |
        RECOMP_RUNTIME_UI_STANDARD_CLOSE | RECOMP_RUNTIME_UI_STANDARD_RESUME |
        RECOMP_RUNTIME_UI_STANDARD_FULLSCREEN | RECOMP_RUNTIME_UI_STANDARD_VOLUME |
        RECOMP_RUNTIME_UI_STANDARD_PAUSE_ON_OPEN;
    auto* ui = recomp_runtime_ui_create_standard(&c);
    CHECK(ui && ui->config.item_count == 7);
    CHECK(std::string(ui->sections[0]) == "Game");
    CHECK(!recomp_runtime_ui_activate(ui, "system.reset")); // closed
    recomp_runtime_ui_open(ui);
    recomp_runtime_ui_enter_section(ui, 0);
    CHECK(std::string(recomp_runtime_ui_section_item(ui,0,0)->key) == "system.pause");
    CHECK(std::string(recomp_runtime_ui_section_item(ui,0,1)->key) == "system.resume");
    key(ui, RECOMP_RUNTIME_UI_INPUT_RIGHT); CHECK(ui->row_index == 1 && s.actions == 0);
    key(ui, RECOMP_RUNTIME_UI_INPUT_LEFT); CHECK(ui->row_index == 0 && s.actions == 0);
    ui->row_index = 2;
    key(ui, RECOMP_RUNTIME_UI_INPUT_ACCEPT); CHECK(!s.auto_pause);
    key(ui, RECOMP_RUNTIME_UI_INPUT_ACCEPT); CHECK(s.auto_pause);
    CHECK(recomp_runtime_ui_activate(ui, "system.pause") && s.actions == 1);
    for (const char* command : {"system.reset", "system.close"}) {
        int before = s.actions;
        CHECK(recomp_runtime_ui_activate(ui, command));
        CHECK(recomp_runtime_ui_confirmation_pending(ui) && !ui->confirm_accept);
        CHECK(!recomp_runtime_ui_activate(ui, "system.resume"));
        key(ui, RECOMP_RUNTIME_UI_INPUT_ACCEPT); // safe default Cancel
        CHECK(s.actions == before && !ui->pending_action);
        recomp_runtime_ui_activate(ui, command);
        key(ui, RECOMP_RUNTIME_UI_INPUT_RIGHT);
        key(ui, RECOMP_RUNTIME_UI_INPUT_ACCEPT, 1); // held Enter cannot confirm
        CHECK(s.actions == before && ui->pending_action);
        key(ui, RECOMP_RUNTIME_UI_INPUT_BACK);
        CHECK(!ui->pending_action && ui->open);
        recomp_runtime_ui_activate(ui, command);
        s.enabled = false;
        recomp_runtime_ui_confirm(ui, 1); // recheck permission at confirmation
        CHECK(s.actions == before);
        s.enabled = true;
        recomp_runtime_ui_activate(ui, command);
        key(ui, RECOMP_RUNTIME_UI_INPUT_RIGHT);
        key(ui, RECOMP_RUNTIME_UI_INPUT_ACCEPT);
        CHECK(s.actions == before + 1 && s.last == command);
    }
    recomp_runtime_ui_activate(ui, "system.reset");
    recomp_runtime_ui_close(ui);
    CHECK(!ui->pending_action);
    recomp_runtime_ui_open(ui);
    CHECK(recomp_runtime_ui_activate(ui, "system.resume"));
    CHECK(s.last == "system.resume");
    CHECK(!recomp_runtime_ui_activate(ui, "system.missing"));

    // Check layout and menu model, never rendered pixels/game classification.
    ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DeltaTime = 1.f/60;
    unsigned char* data; int w,h; io.Fonts->GetTexDataAsRGBA32(&data,&w,&h);
    for (auto size : {ImVec2(1280,720), ImVec2(960,640), ImVec2(640,480)}) {
        io.DisplaySize = size;
        recomp_runtime_ui_enter_section(ui, 0); // render the pair on narrow windows too
        for (int confirm = 0; confirm < 2; ++confirm) {
            if (confirm) recomp_runtime_ui_activate(ui, "system.reset");
            for (int frame=0; frame<3; ++frame) {
                ImGui::NewFrame(); recomp_runtime_ui_render_imgui(ui);
                auto* window = ImGui::FindWindowByName("##recomp-runtime-ui");
                CHECK(window && window->Pos.x >= 0 && window->Pos.y >= 0);
                CHECK(window->Pos.x+window->Size.x <= size.x);
                CHECK(window->Pos.y+window->Size.y <= size.y);
                // ImGui ContentSize describes the previous frame (including
                // the old dimensions immediately after a resize).
                if (frame > 0) CHECK(window->ContentSize.x <= window->Size.x);
                ImGui::Render();
            }
            recomp_runtime_ui_confirm(ui, 0);
        }
        recomp_runtime_ui_close(ui);
        ImGui::NewFrame(); recomp_runtime_ui_render_paused_imgui(ui);
        auto* badge = ImGui::FindWindowByName("##recomp-paused");
        CHECK(badge && badge->Pos.x >= 0 && badge->Pos.x + badge->Size.x <= size.x);
        ImGui::Render();
        recomp_runtime_ui_open(ui);
    }
    ImGui::DestroyContext();
    recomp_runtime_ui_destroy(ui);
    auto reset = swordcraft3::reset_arguments({"game.exe","--launcher","--rom","space path/patch.gba",
        "--load-state","save.state","--save","battery.eep"});
    CHECK(reset == std::vector<std::string>({"game.exe","--rom","space path/patch.gba","--save","battery.eep","--no-launcher"}));
    CHECK(swordcraft3::quote_process_arg(L"C:\\game folder\\") == L"\"C:\\game folder\\\\\"");
    CHECK(swordcraft3::quote_process_arg(L"a\"b") == L"\"a\\\"b\"");
    auto path = std::filesystem::current_path() / "validation/runtime-menu-preferences/settings.ini";
    gbarecomp::RuntimeMenuPreferences prefs;
    prefs.load({}, true);
    CHECK(prefs.paused(false,true) && !prefs.paused(false,false));
    CHECK(prefs.set_pause_on_open(false));
    CHECK(!prefs.paused(false,true) && prefs.paused(true,true) && prefs.paused(true,false));
    prefs.load(path, true);
    CHECK(prefs.set_pause_on_open(false));
    gbarecomp::RuntimeMenuPreferences reload;
    reload.load(path,true); CHECK(!reload.pause_on_open());
    CHECK(reload.set_pause_on_open(true));
    prefs.load(path,false); CHECK(prefs.pause_on_open());
    { std::ofstream out(path); out << "[RuntimeMenu]\npause_on_open=10\n"; }
    prefs.load(path,true); CHECK(prefs.pause_on_open());
    prefs.load(path.parent_path(),true); // directory cannot be replaced by a file
    CHECK(!prefs.set_pause_on_open(false) && prefs.pause_on_open());
    CHECK(std::filesystem::is_directory(path.parent_path()));
    const auto filters = path.parent_path() / "filter-reset.ini";
    const std::vector<std::string> initial_reset = {"game.exe", "--rom", "space path/patch.gba",
        "--bios", "bios.bin", "--save", "battery.eep", "--view-width", "384",
        "--linear-filter", "0", "--sharp-filter", "0", "--smooth-filter", "0",
        "--screen-effect", "off", "--screen-effect-strength", "35", "--no-launcher"};
    const auto write_filters = [&](const std::string& contents) {
        std::ofstream out(filters, std::ios::binary); out << contents; out.close(); CHECK(out.good());
    };
    auto refreshed = initial_reset;
    CHECK(!swordcraft3::refresh_reset_presentation_arguments(refreshed, path.parent_path()/"missing-filters.ini"));
    CHECK(refreshed == initial_reset);
    for (const std::string& malformed : {
        std::string("[Launcher]\nlinear_filter=1\n"), // old or incomplete file
        std::string("[Launcher]\nlinear_filter=0\nsharp_filter=0\nsmooth_filter=1\nscreen_effect=3\nscreen_effect_strength=35\n"),
        std::string("[Launcher]\nlinear_filter=1\nsharp_filter=0\nsmooth_filter=1\nscreen_effect=2\nscreen_effect_strength=35\n"),
        std::string("[Launcher]\nlinear_filter=0\nsharp_filter=0\nsmooth_filter=1\nscreen_effect=2\nscreen_effect_strength=35oops\n"),
        std::string("[Launcher]\nlinear_filter=0\nsharp_filter=0\nsmooth_filter=1\nscreen_effect=2\nscreen_effect_strength=-1\n"),
        std::string("[Launcher]\nlinear_filter=0\nsharp_filter=0\nsmooth_filter=1\nscreen_effect=2\nscreen_effect_strength=101\n"),
        std::string("[Launcher]\nlinear_filter=0\nsharp_filter=0\nsmooth_filter=1\nscreen_effect=2\nscreen_effect_strength=35\nscreen_effect=0\n"),
        std::string("[Launcher]\nlinear_filter=0\nsharp_filter=0\nsmooth_filter=1\nscreen_effect=2\nscreen_effect_strength=35\n[broken\n")}) {
        write_filters(malformed);
        CHECK(!swordcraft3::refresh_reset_presentation_arguments(refreshed, filters));
        CHECK(refreshed == initial_reset);
    }
    write_filters("\xef\xbb\xbf[Launcher]\r\nlinear_filter = 0\r\nsharp_filter = 0\r\n"
                  "smooth_filter = 1 ; chosen during play\r\nscreen_effect = 2\r\nscreen_effect_strength = 61\r\n"
                  "[Other]\r\nlinear_filter = 1\r\n");
    CHECK(swordcraft3::refresh_reset_presentation_arguments(refreshed, filters));
    CHECK(refreshed == std::vector<std::string>({"game.exe", "--rom", "space path/patch.gba",
        "--bios", "bios.bin", "--save", "battery.eep", "--view-width", "384", "--no-launcher",
        "--linear-filter", "0", "--sharp-filter", "0", "--smooth-filter", "1",
        "--screen-effect", "crt", "--screen-effect-strength", "61"}));
    // A subsequent reset must reread the settings changed in the reset child,
    // not replay the first session's or first reset's presentation flags.
    write_filters("[Launcher]\nlinear_filter=0\nsharp_filter=1\nsmooth_filter=0\n"
                  "screen_effect=1\nscreen_effect_strength=20\n");
    CHECK(swordcraft3::refresh_reset_presentation_arguments(refreshed, filters));
    CHECK(refreshed == std::vector<std::string>({"game.exe", "--rom", "space path/patch.gba",
        "--bios", "bios.bin", "--save", "battery.eep", "--view-width", "384", "--no-launcher",
        "--linear-filter", "0", "--sharp-filter", "1", "--smooth-filter", "0",
        "--screen-effect", "lcd", "--screen-effect-strength", "20"}));
    const auto original_aspect_args = refreshed;
    for (int index : {0, 1, 2, 1}) {
        write_filters("[Launcher]\nhost_aspect_index=" + std::to_string(index) + "\n");
        CHECK(swordcraft3::refresh_reset_host_aspect_arguments(refreshed, filters));
        auto expected = original_aspect_args;
        expected.insert(expected.end(), {"--host-aspect", std::to_string(index)});
        CHECK(refreshed == expected);
    }
    const auto last_aspect_args = refreshed;
    write_filters("[Launcher]\nhost_aspect_index=9\n");
    CHECK(!swordcraft3::refresh_reset_host_aspect_arguments(refreshed, filters));
    CHECK(refreshed == last_aspect_args);
    std::cout << "PASS: reset refreshes valid host aspects without changing media, filters or guest view arguments\n";
    std::cout << "PASS: reset refreshes only complete valid presentation settings, preserves media/other args, rereads later changes\n";
    std::cout << "PASS: action catalog, Cancel-first confirmations, repeats, disabled recheck, Resume, layout bounds, reset arguments\n";
}
