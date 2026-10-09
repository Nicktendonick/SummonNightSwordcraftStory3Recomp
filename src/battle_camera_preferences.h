#pragma once
#include "../gbarecomp/src/runtime/presentation_preferences.h"
#include "custom_renderer.h"
#include <stdexcept>

namespace swordcraft3 {
class BattleCameraPreferences {
public:
    std::filesystem::path path;
    // 0: Current; 1: Bounded; 2: Follow + edge stops (experimental).
    int mode=0;
    bool cover_edges=false;
    std::string error;
    void load(const std::filesystem::path& root) {
        path=root/"Settings/battle-camera.ini"; mode=0; cover_edges=false; error.clear();
        try {
            const auto status=std::filesystem::symlink_status(path);
            if(!std::filesystem::exists(status)) return;
            if(!std::filesystem::is_regular_file(status) || std::filesystem::file_size(path)>65536)
                throw std::runtime_error("Invalid battle-camera settings file.");
            std::ifstream input(path,std::ios::binary);
            if(!input) throw std::runtime_error("Cannot read battle-camera settings.");
            bool section=false,found_mode=false,found_legacy=false,found_cover=false,first=true;
            bool cover_value=false;
            int value=0,legacy_value=0;
            std::string line;
            while(std::getline(input,line)) {
                if(first && line.compare(0,3,"\xef\xbb\xbf")==0) line.erase(0,3);
                first=false;
                if(line.find('\0')!=std::string::npos) throw std::runtime_error("Invalid battle-camera settings.");
                using gbarecomp::presentation_preferences_detail::trim;
                line=trim(line.substr(0,line.find_first_of(";#")));
                if(line.empty()) continue;
                if(line[0]=='[') {
                    if(line.back()!=']' || line.find(']')!=line.size()-1)
                        throw std::runtime_error("Invalid battle-camera settings section.");
                    section=line=="[Launcher]"; continue;
                }
                const auto eq=line.find('=');
                if(!section || eq==std::string::npos) continue;
                const auto key=trim(line.substr(0,eq));
                if(key!="battle_camera_mode" && key!="bounded_battle_camera" && key!="battle_edge_cover") continue;
                const auto text=trim(line.substr(eq+1));
                if(key=="battle_camera_mode") {
                    if(found_mode || (text!="0" && text!="1" && text!="2"))
                        throw std::runtime_error("Invalid battle_camera_mode setting.");
                    found_mode=true; value=text[0]-'0';
                } else if(key=="bounded_battle_camera") {
                    if(found_legacy || (text!="0" && text!="1"))
                        throw std::runtime_error("Invalid bounded_battle_camera setting.");
                    found_legacy=true; legacy_value=text[0]-'0';
                } else {
                    if(found_cover || (text!="0" && text!="1"))
                        throw std::runtime_error("Invalid battle_edge_cover setting.");
                    found_cover=true; cover_value=text=="1";
                }
            }
            if(input.bad() || (!found_mode && !found_legacy)) throw std::runtime_error("Cannot load battle-camera setting.");
            // The explicit mode wins over the compatibility key. Old files load
            // unchanged, and load never rewrites a user's existing settings.
            mode=found_mode?value:legacy_value;
            cover_edges=cover_value;
        } catch(const std::exception& e) { error=std::string(e.what())+" Current framing retained."; }
    }
    bool save(int value) { return persist(value,cover_edges); }
    bool save_cover(int value) {
        if(value!=0 && value!=1) { error="Invalid edge cover setting. Previous choice retained."; return false; }
        return persist(mode,value!=0);
    }
    bool persist(int value,bool cover) {
        if(value<0 || value>2) {
            error="Invalid battle camera mode. Previous choice retained."; return false;
        }
        if(!path.empty()) {
            BattleCameraPreferences current;
            current.load(path.parent_path().parent_path());
            if(!current.error.empty()) { error=current.error+" Repair Settings/battle-camera.ini before changing this option."; return false; }
        }
        std::error_code directory_error;
        if(!path.empty()) std::filesystem::create_directories(path.parent_path(),directory_error);
        // Old engines do not understand Follow; they safely use Current. Write
        // camera compatibility and edge-cover keys atomically, preserving other settings.
        if(path.empty() || directory_error || !gbarecomp::update_launcher_preferences(path,
                {"battle_camera_mode","bounded_battle_camera","battle_edge_cover"},{value,value==1?1:0,cover?1:0})) {
            error="Could not save Settings/battle-camera.ini. Previous choice retained."; return false;
        }
        mode=value; cover_edges=cover; error.clear();
        set_swordcraft3_battle_camera_mode(value); set_swordcraft3_battle_edge_cover(cover); return true;
    }
    static int get(void* ctx) { return static_cast<BattleCameraPreferences*>(ctx)->mode; }
    static int set(void* ctx,int value) { return static_cast<BattleCameraPreferences*>(ctx)->save(value); }
    static int get_cover(void* ctx) { return static_cast<BattleCameraPreferences*>(ctx)->cover_edges; }
    static int set_cover(void* ctx,int value) { return static_cast<BattleCameraPreferences*>(ctx)->save_cover(value); }
    // The existing launcher supports switches, not choice controls. Each
    // alternative selects the same single mode, so both can never be active.
    template<int Choice> static int get_choice(void* ctx) {
        static_assert(Choice==1 || Choice==2);
        return static_cast<BattleCameraPreferences*>(ctx)->mode==Choice;
    }
    template<int Choice> static int set_choice(void* ctx,int enabled) {
        static_assert(Choice==1 || Choice==2);
        auto* preferences=static_cast<BattleCameraPreferences*>(ctx);
        if(enabled!=0 && enabled!=1) return preferences->save(-1);
        return preferences->save(enabled?Choice:(preferences->mode==Choice?0:preferences->mode));
    }
    static const char* last_error(void* ctx) { return static_cast<BattleCameraPreferences*>(ctx)->error.c_str(); }
};
}
