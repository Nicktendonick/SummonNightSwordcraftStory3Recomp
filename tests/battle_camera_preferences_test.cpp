#include "battle_camera_preferences.h"
#include <chrono>
#include <iostream>

#define CHECK(x) do { if(!(x)) throw std::runtime_error("Failed: " #x); } while(0)

static int applied=-1;
static int apply_count=0;
void set_swordcraft3_battle_camera_mode(int value) { applied=value; ++apply_count; }

static std::string read(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    CHECK(input.good());
    return {std::istreambuf_iterator<char>(input),{}};
}
static void write(const std::filesystem::path& path,const std::string& text) {
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    output<<text; output.close(); CHECK(output.good());
}

int main() {
    using swordcraft3::BattleCameraPreferences;
    BattleCameraPreferences unconfigured;
    CHECK(!unconfigured.save(2) && unconfigured.mode==0 && apply_count==0);
    const auto root=std::filesystem::current_path()/(
        "camera-mode-prefs-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(std::filesystem::create_directory(root));
    BattleCameraPreferences p; p.load(root);
    CHECK(p.mode==0 && p.error.empty() && apply_count==0);
    for(int mode:{0,1,2,3,1,0}) {
        CHECK(p.save(mode) && p.mode==mode && applied==mode && p.error.empty());
        BattleCameraPreferences q; q.load(root);
        CHECK(q.mode==mode && q.error.empty());
        const auto text=read(p.path);
        CHECK(text.find("battle_camera_mode = "+std::to_string(mode))!=std::string::npos);
        CHECK(text.find("bounded_battle_camera = "+std::to_string(mode==1?1:0))!=std::string::npos);
    }

    // Legacy preferences load without rewriting any byte; saving migrates only
    // the camera keys, retaining other values, sections, comments and CRLF.
    for(int legacy:{0,1}) {
        const std::string original="\xef\xbb\xbf; kept\r\n[Launcher]\r\n"
            "bounded_battle_camera = "+std::to_string(legacy)+" ; old setting\r\n"
            "select_guard=1\r\n[Other]\r\nbattle_camera_mode=99\r\n";
        write(p.path,original); p.load(root);
        CHECK(p.mode==legacy && p.error.empty() && read(p.path)==original);
        CHECK(p.save(2));
        const auto saved=read(p.path);
        CHECK(saved.find("\xef\xbb\xbf; kept\r\n[Launcher]\r\n") == 0);
        CHECK(saved.find("bounded_battle_camera = 0 ; old setting\r\n")!=std::string::npos);
        CHECK(saved.find("select_guard=1\r\n")!=std::string::npos);
        CHECK(saved.find("battle_camera_mode = 2\r\n[Other]\r\nbattle_camera_mode=99\r\n")!=std::string::npos);
    }
    write(p.path,"[Launcher]\nbounded_battle_camera=1\nbattle_camera_mode=2\n");
    p.load(root); CHECK(p.mode==2 && p.error.empty());
    write(p.path,"[Launcher]\nbattle_camera_mode=2\nbounded_battle_camera=0\n");
    p.load(root); CHECK(p.mode==2 && p.error.empty());

    // Malformed input and invalid API values leave both stored bytes and the
    // currently applied renderer choice intact; load always fails to Current.
    const int applied_before=applied, count_before=apply_count;
    for(const std::string& bad:{
            "[Launcher]\nbattle_camera_mode=-1\n",
            "[Launcher]\nbattle_camera_mode=4\n",
            "[Launcher]\nbattle_camera_mode=02\n",
            "[Launcher]\nbattle_camera_mode=true\n",
            "[Launcher]\nbattle_camera_mode=2 junk\n",
            "[Launcher]\nbattle_camera_mode=\n",
            "[Launcher]\nbattle_camera_mode=1\nbattle_camera_mode=1\n",
            "[Launcher]\nbounded_battle_camera=1\nbounded_battle_camera=0\n",
            "[Launcher]\nbattle_camera_mode=2\nbounded_battle_camera=2\n",
            "[Launcher]\nbounded_battle_camera=2\n",
            "[Launcher]\nbattle_camera_mode=2\n[broken\n",
            "[Other]\nbattle_camera_mode=2\n"}) {
        write(p.path,bad); p.load(root);
        CHECK(p.mode==0 && !p.error.empty());
        CHECK(!p.save(1) && p.mode==0 && read(p.path)==bad);
        CHECK(applied==applied_before && apply_count==count_before);
    }
    std::string nul="[Launcher]\nbattle_camera_mode=2\n"; nul.push_back('\0');
    write(p.path,nul); p.load(root);
    CHECK(p.mode==0 && !p.error.empty() && !p.save(2) && read(p.path)==nul);
    write(p.path,std::string(65537,' ')); p.load(root);
    CHECK(p.mode==0 && !p.error.empty() && !p.save(2));
    CHECK(std::filesystem::file_size(p.path)==65537);

    write(p.path,"[Launcher]\nbattle_camera_mode=2\n"); p.load(root);
    const auto valid=read(p.path);
    CHECK(p.mode==2 && p.error.empty());
    CHECK(!p.save(-1) && !p.save(4));
    CHECK(p.mode==2 && read(p.path)==valid && apply_count==count_before);
    CHECK(BattleCameraPreferences::get(&p)==2);
    CHECK(!BattleCameraPreferences::set(&p,99));
    CHECK(BattleCameraPreferences::set(&p,1) && p.mode==1 && applied==1);
    CHECK(BattleCameraPreferences::get_choice<1>(&p) && !BattleCameraPreferences::get_choice<2>(&p));
    CHECK(BattleCameraPreferences::set_choice<2>(&p,1) && p.mode==2 && applied==2);
    CHECK(!BattleCameraPreferences::get_choice<1>(&p) && BattleCameraPreferences::get_choice<2>(&p));
    CHECK(BattleCameraPreferences::set_choice<1>(&p,0) && p.mode==2);
    CHECK(BattleCameraPreferences::set_choice<2>(&p,0) && p.mode==0 && applied==0);
    CHECK(!BattleCameraPreferences::get_choice<1>(&p) && !BattleCameraPreferences::get_choice<2>(&p));
    CHECK(BattleCameraPreferences::set_choice<1>(&p,1) && p.mode==1 && applied==1);
    CHECK(!BattleCameraPreferences::set_choice<2>(&p,2) && p.mode==1 && applied==1);
    // The retired per-arena mask is ignored, with old files preserved.
    write(p.path,"[Launcher]\nbattle_camera_mode=2\nbattle_edge_cover=1\n");
    p.load(root); CHECK(p.mode==2 && p.error.empty());
    CHECK(p.save(1) && read(p.path).find("battle_edge_cover=1")!=std::string::npos);

    // A directory in place of the settings file is rejected, never replaced.
    CHECK(std::filesystem::remove(p.path));
    CHECK(std::filesystem::create_directory(p.path)); p.load(root);
    CHECK(p.mode==0 && !p.error.empty() && !p.save(2));
    CHECK(std::filesystem::is_directory(p.path));
    CHECK(std::filesystem::remove(p.path));
    CHECK(std::filesystem::remove(root/"Settings"));
    CHECK(std::filesystem::remove(root));
    std::cout<<"PASS: camera mode migration, four-mode persistence, unrelated settings preservation, invalid inputs and no failed apply\n";
}
