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
static void test_custom_choices() {
    struct ChoiceState { int value = -1; int saves = 0; bool accept = true; } state;
    const char* names[] = {"First", "Second", "Third"};
    const char* descriptions[] = {"First explanation", "Second explanation", "Third explanation"};
    RecompRuntimeUiItem item{};
    item.key = "graphics.preset"; item.section = "Graphics"; item.label = "Graphics preset";
    item.description = "Custom combination"; item.type = RECOMP_RUNTIME_UI_CHOICE;
    item.maximum = 2; item.step = 1; item.choices = names; item.choice_count = 3;
    item.unknown_choice_label = "Custom"; item.choice_descriptions = descriptions;
    RecompRuntimeUiConfig config{};
    config.items = &item; config.item_count = 1; config.callbacks.context = &state;
    config.callbacks.get_value = [](void* p, const RecompRuntimeUiItem*, int* out) {
        *out = static_cast<ChoiceState*>(p)->value; return 1;
    };
    config.callbacks.set_value = [](void* p, const RecompRuntimeUiItem*, int v) {
        auto& s = *static_cast<ChoiceState*>(p);
        if (!s.accept) return 0;
        s.value = v; return 1;
    };
    config.callbacks.save = [](void* p) { ++static_cast<ChoiceState*>(p)->saves; };
    auto* ui = recomp_runtime_ui_create(&config);
    CHECK(ui);
    recomp_runtime_ui_open(ui); recomp_runtime_ui_enter_section(ui, 0);
    CHECK(std::string(recomp_runtime_ui_item_description(ui, &item)) == "Custom combination");
    key(ui, RECOMP_RUNTIME_UI_INPUT_RIGHT);
    CHECK(state.value == 0 && state.saves == 1);
    CHECK(std::string(recomp_runtime_ui_item_description(ui, &item)) == descriptions[0]);
    key(ui, RECOMP_RUNTIME_UI_INPUT_LEFT); CHECK(state.value == 2);
    state.value = -1;
    key(ui, RECOMP_RUNTIME_UI_INPUT_LEFT); CHECK(state.value == 2);
    key(ui, RECOMP_RUNTIME_UI_INPUT_RIGHT); CHECK(state.value == 0);
    state.value = 100;
    key(ui, RECOMP_RUNTIME_UI_INPUT_RIGHT); CHECK(state.value == 0);
    const int saved = state.saves;
    state.accept = false;
    key(ui, RECOMP_RUNTIME_UI_INPUT_RIGHT);
    CHECK(state.value == 0 && state.saves == saved);
    const int sparse[] = {10, 20, 30};
    item.choice_values = sparse; state.value = -1; state.accept = true;
    key(ui, RECOMP_RUNTIME_UI_INPUT_LEFT); CHECK(state.value == 30);
    CHECK(std::string(recomp_runtime_ui_item_description(ui, &item)) == descriptions[2]);
    key(ui, RECOMP_RUNTIME_UI_INPUT_RIGHT); CHECK(state.value == 10);
    recomp_runtime_ui_destroy(ui);
    std::cout << "PASS: Custom choice navigation, wrap, dynamic descriptions, sparse choices and rejected saves\n";
}

static void test_organized_menu() {
    State state;
    const char* choices[]={"Current","Bounded","Follow + edge stops"};
    RecompRuntimeUiItem extras[3]{};
    extras[0].key="test.camera"; extras[0].section="Graphics";
    extras[0].label="Battle framing"; extras[0].group="Battle View";
    extras[0].type=RECOMP_RUNTIME_UI_CHOICE; extras[0].choices=choices; extras[0].choice_count=3;
    extras[0].maximum=2; extras[0].step=1;
    extras[1]=extras[0]; extras[1].key="test.picture"; extras[1].group="Picture";
    extras[2]=extras[0]; extras[2].key="test.aspect"; extras[2].group="Window & Screen";
    RecompRuntimeUiStandardConfig c{};
    c.menu.theme="storybook";
    c.menu.title="Swordcraft Story 3";
    c.menu.subtitle="Craftknights!\nArtisans of weapons!\nMasters of the sword!";
    c.menu.presentation_flags=RECOMP_RUNTIME_UI_PRESENTATION_ORGANIZED;
    c.menu.callbacks.context=&state; c.menu.callbacks.run_action=action;
    c.menu.callbacks.get_value=[](void*,const RecompRuntimeUiItem*,int* out){*out=2;return 1;};
    c.features=RECOMP_RUNTIME_UI_STANDARD_PAUSE | RECOMP_RUNTIME_UI_STANDARD_RESUME |
        RECOMP_RUNTIME_UI_STANDARD_RESET | RECOMP_RUNTIME_UI_STANDARD_CLOSE |
        RECOMP_RUNTIME_UI_STANDARD_FULLSCREEN | RECOMP_RUNTIME_UI_STANDARD_WINDOW_SCALE |
        RECOMP_RUNTIME_UI_STANDARD_VOLUME;
    c.extra_items=extras; c.extra_item_count=3;
    auto* ui=recomp_runtime_ui_create_standard(&c); CHECK(ui);
    CHECK(ui->section_count==3 && std::string(ui->sections[1])=="Graphics");
    CHECK(std::string(recomp_runtime_ui_section_item(ui,0,0)->key)==RECOMP_RUNTIME_UI_KEY_PAUSE);
    CHECK(std::string(recomp_runtime_ui_section_item(ui,0,1)->key)==RECOMP_RUNTIME_UI_KEY_RESUME);
    CHECK(std::string(recomp_runtime_ui_section_item(ui,0,2)->group)=="End session");
    CHECK(std::string(recomp_runtime_ui_section_item(ui,0,3)->label)=="Quit game");
    const char* expected[]={RECOMP_RUNTIME_UI_KEY_WINDOW_SCALE,RECOMP_RUNTIME_UI_KEY_FULLSCREEN,
                            "test.aspect","test.picture","test.camera"};
    for(size_t i=0;i<5;++i) CHECK(std::string(recomp_runtime_ui_section_item(ui,1,i)->key)==expected[i]);
    ImGui::CreateContext();
    auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DeltaTime=1.f/60;
    unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    recomp_runtime_ui_open(ui);
    for(auto size : {ImVec2(1280,720),ImVec2(960,640),ImVec2(640,480)}) {
        io.DisplaySize=size;
        for(size_t section=0;section<ui->section_count;++section) {
            recomp_runtime_ui_enter_section(ui,section);
            for(int frame=0;frame<3;++frame) {
                ImGui::NewFrame(); recomp_runtime_ui_render_imgui(ui);
                auto* window=ImGui::FindWindowByName("##recomp-runtime-ui");
                CHECK(window && window->Pos.x>=0 && window->Pos.x+window->Size.x<=size.x);
                if(frame>0) CHECK(window->ContentSize.x<=window->Size.x);
                ImGui::Render();
            }
        }
    }
    // Click the actual persistent footer, not the Game-section Resume row.
    auto* window=ImGui::FindWindowByName("##recomp-runtime-ui");
    // The retained ImGui layout cursor identifies the footer's actual row;
    // do not assume an inset from the bottom (themes have different padding).
    ImVec2 footer(window->Pos.x+window->Size.x-55,window->DC.CursorPosPrevLine.y+8);
    io.AddMousePosEvent(footer.x,footer.y);
    for(int step=0;step<3;++step) {
        if(step==1) io.AddMouseButtonEvent(0,true);
        if(step==2) io.AddMouseButtonEvent(0,false);
        ImGui::NewFrame(); recomp_runtime_ui_render_imgui(ui); ImGui::Render();
    }
    CHECK(state.last==RECOMP_RUNTIME_UI_KEY_RESUME && state.actions==1);
    ImGui::DestroyContext(); recomp_runtime_ui_destroy(ui);
    std::cout << "PASS: organized sections, ordering, three layout sizes, persistent footer invokes Resume\n";
}

int main() {
    test_organized_menu();
    test_custom_choices();
    State s;
    RecompRuntimeUiStandardConfig c{};
    c.menu.theme = "storybook";
    c.menu.title = "Swordcraft Story 3";
    c.menu.subtitle = "Craftknights!\nArtisans of weapons!\nMasters of the sword!";
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
    auto screen_args = last_aspect_args;
    screen_args.insert(screen_args.end(), {"--screen", "raw", "--screen=classic"});
    const auto unchanged_screen_args = screen_args;
    for (const std::string malformed : {"[Launcher]\nscreen=unknown\n", "[Launcher]\nscreen=raw\nscreen=unlit\n",
                                         "[Launcher]\nvolume=50\n", "[Launcher\nscreen=backlit\n"}) {
        write_filters(malformed);
        CHECK(!swordcraft3::refresh_reset_screen_model_arguments(screen_args, filters));
        CHECK(screen_args == unchanged_screen_args);
    }
    for (int model : {0, 1, 2, 3, 4, 0}) {
        write_filters(std::string("\xef\xbb\xbf[Launcher]\r\nscreen = ") +
                      gbarecomp::screen_model_tokens[model] + " ; chosen live\r\n");
        CHECK(swordcraft3::refresh_reset_screen_model_arguments(screen_args, filters));
        auto expected = last_aspect_args;
        expected.insert(expected.end(), {"--screen", gbarecomp::screen_model_tokens[model]});
        CHECK(screen_args == expected);
    }
    std::cout << "PASS: colour model reset refresh, canonical tokens, duplicate/malformed rejection and independent filter/aspect arguments\n";
    std::cout << "PASS: reset refreshes only complete valid presentation settings, preserves media/other args, rereads later changes\n";
    std::cout << "PASS: action catalog, Cancel-first confirmations, repeats, disabled recheck, Resume, layout bounds, reset arguments\n";
}
