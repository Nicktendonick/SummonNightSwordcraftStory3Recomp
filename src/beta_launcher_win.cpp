// Local beta entry point. No shell, PowerShell, compiler or global PATH changes.
// All writable data stays beside this executable. Release Runtime is relocatable.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include "beta_boxart.h"

namespace fs = std::filesystem;
static std::wstring quote(const fs::path& path) {
    // Paths are constructed from module location, never free-form command text.
    return L"\"" + path.wstring() + L"\"";
}
static fs::path owner_root() {
    std::vector<wchar_t> buffer(32768);
    const auto n = GetModuleFileNameW(nullptr,buffer.data(),DWORD(buffer.size()));
    if (!n || n >= buffer.size()) throw std::runtime_error("Cannot locate launcher.");
    auto dir=fs::path(std::wstring(buffer.data(),n)).parent_path();
    return dir;
}
static fs::path development_root(fs::path dir) {
    // Root-deployed executable and CMake build-tree executable are both usable.
    for (int i=0;i<5;++i,dir=dir.parent_path())
        if(fs::is_regular_file(dir/L"experiments/full-viewport-renderer/native-test.toml")) return dir;
    throw std::runtime_error("Missing Runtime folder. Extract the complete portable release beside the launcher.");
}
static void copy_once(const fs::path& source, const fs::path& target) {
    if (fs::exists(target) || !fs::is_regular_file(source)) return;
    const auto temp = target.string() + ".migrating";
    fs::copy_file(source, temp, fs::copy_options::overwrite_existing);
    fs::rename(temp, target); // publish only a complete copy
}
static void clean_experiment_environment() {
    auto block=GetEnvironmentStringsW();
    if(!block) throw std::runtime_error("Cannot read process environment.");
    std::vector<std::wstring> names;
    for(auto p=block;*p;p+=wcslen(p)+1) {
        std::wstring value(p);
        if(value.rfind(L"SWORDCRAFT3_",0)==0 || value.rfind(L"GBARECOMP_",0)==0 || value.rfind(L"SDL_",0)==0)
            names.push_back(value.substr(0,value.find(L'=')));
    }
    FreeEnvironmentStringsW(block);
    for(const auto& key:names) SetEnvironmentVariableW(key.c_str(),nullptr);
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR command,int) {
    const bool check=std::wstring(command)==L"--check";
    HANDLE mutex=nullptr;
    try {
        const auto root=owner_root();
        const bool packaged=fs::is_regular_file(root/L"Runtime/Swordcraft3CustomRendererBeta.exe");
        const auto owner=packaged ? root : development_root(root);
        const auto lab=owner/L"experiments/full-viewport-renderer";
        const auto runtime=packaged ? root/L"Runtime" : lab/L"build-native";
        const auto exe=runtime/L"Swordcraft3CustomRendererBeta.exe";
        const auto play=root/L"Logs";
        const auto rom=root/L"ROMs/swordcraft3_beta.gba";
        const auto bios=root/L"BIOS/gba_bios.bin";
        const auto config=packaged ? runtime/L"game.toml" : lab/L"native-test.toml";
        const auto save=root/L"Saves/battery.eep";
        for(const auto& path:{exe,config,exe.parent_path()/L"SDL2.dll",
                              exe.parent_path()/L"libstdc++-6.dll",exe.parent_path()/L"libgcc_s_seh-1.dll",
                              exe.parent_path()/L"libwinpthread-1.dll"})
            if(!fs::is_regular_file(path)) throw std::runtime_error("Missing beta input or runtime file: "+path.string());
        const auto logroot=root/L"Captures";
        if(check) {
            return 0; // read-only preflight: no data or preferences written
        }
        mutex=CreateMutexW(nullptr,FALSE,L"Local\\Swordcraft3FullFrameBetaLauncher");
        if(!mutex) throw std::runtime_error("Cannot create launcher instance guard.");
        if(GetLastError()==ERROR_ALREADY_EXISTS) {
            MessageBoxW(nullptr,L"The beta launcher or game is already open. Close it before starting another copy.",L"Swordcraft Story 3 Beta",MB_OK|MB_ICONINFORMATION);
            CloseHandle(mutex); return 0;
        }
        clean_experiment_environment();
        for (const auto* folder : {L"Settings",L"Saves",L"Save States",L"ROMs",L"BIOS",L"Mods",L"Captures",L"Logs"})
            fs::create_directories(root/folder);
        // Fail visibly if the selected portable directory is read-only.
        const auto probe=root/L"Settings/write-check.tmp";
        { std::ofstream test(probe); test << "portable"; test.flush();
          if (!test) throw std::runtime_error("The portable folder is not writable. Move it to a writable location."); }
        fs::remove(probe);
        const auto settings=root/L"Settings";
        const auto marker=settings/L"legacy-import.done";
        if (!packaged && !fs::exists(marker)) {
            const auto oldplay=lab/L"build-native/full-combat-playtest";
            copy_once(oldplay/L"native-renderer.eep",save);
            copy_once(oldplay/L"swordcraft3_beta.gba",rom);
            copy_once(owner/L"gbarecomp/bios/gba_bios.bin",bios);
            for (int slot=1;slot<=10;++slot)
                copy_once(oldplay/("swordcraft3_beta.state"+std::to_string(slot)),
                          root/L"Save States"/("beta.state"+std::to_string(slot)));
            copy_once(runtime/L"config-beta-preview.ini",settings/L"launcher.ini");
            copy_once(runtime/L"swordcraft3-display.ini",settings/L"display.ini");
            copy_once(runtime/L"beta-boxart-state.txt",settings/L"boxart.txt");
            std::ofstream done(marker); done << "Existing files copied once; originals unchanged.\n";
            if (!done) throw std::runtime_error("Cannot record save migration.");
        }
        SetEnvironmentVariableW(L"SWORDCRAFT3_BETA_LAUNCHER",L"1");
        SetEnvironmentVariableW(L"SWORDCRAFT3_PORTABLE_ROOT",root.c_str());
        const auto artworkState=settings/L"boxart.txt";
        std::string previousArtwork;
        std::ifstream(artworkState) >> previousArtwork;
        const std::string selectedArtwork(swordcraft3::next_beta_boxart(previousArtwork));
        const std::wstring selectedArtworkWide(selectedArtwork.begin(), selectedArtwork.end());
        SetEnvironmentVariableW(L"SWORDCRAFT3_BETA_BOXART",
                                selectedArtworkWide.c_str());
        // Do not pass --rom: the current generic seam treats that as a request
        // to bypass the launcher. Seed its sidecars only on the first run.
        for(const auto& entry:std::vector<std::pair<fs::path,fs::path>>{
                {settings/L"rom.cfg",rom},
                {settings/L"bios.cfg",bios}}) {
            if(!fs::exists(entry.first) && fs::is_regular_file(entry.second)) {
                std::ofstream cache(entry.first);
                cache << entry.second.lexically_relative(settings).generic_string() << '\n';
                if(!cache) throw std::runtime_error("Cannot seed beta launcher file selection.");
            }
        }
        for(auto name:{L"SWORDCRAFT3_CUSTOM_RENDERER",L"SWORDCRAFT3_FULL_FIELD_RENDERER",
                      L"SWORDCRAFT3_FULL_COMBAT_RENDERER",L"SWORDCRAFT3_CUSTOM_BATTLES",
                      L"SWORDCRAFT3_CUSTOM_GENERAL_FIELDS",L"SWORDCRAFT3_CUSTOM_OBJECTS",
                      L"SWORDCRAFT3_CUSTOM_ROCKY",L"SWORDCRAFT3_CUSTOM_ADDITIONAL_AREAS"})
            SetEnvironmentVariableW(name,L"1");
        SetEnvironmentVariableW(L"SWORDCRAFT3_CUSTOM_HOST_WIDTH",L"384");
        SYSTEMTIME time{}; GetLocalTime(&time);
        wchar_t stamp[80];
        swprintf(stamp,80,L"%04u%02u%02u-%02u%02u%02u-%03u-%lu",time.wYear,time.wMonth,time.wDay,
                 time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,GetCurrentProcessId());
        const auto capture=logroot/stamp;
        fs::create_directories(capture);
        SetEnvironmentVariableW(L"GBARECOMP_DEBUG_CAPTURE_DIR",capture.c_str());
        SetEnvironmentVariableW(L"GBARECOMP_INPUT_RECORD",(capture/L"session-input.trace").c_str());
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
        HANDLE log=CreateFileW((capture/L"session.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(log==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create beta session log.");
        HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
        STARTUPINFOW startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESTDHANDLES;
        startup.hStdOutput=log; startup.hStdError=log; startup.hStdInput=input;
        PROCESS_INFORMATION process{};
        auto args=quote(exe)+L" --window --launcher --view-width 240 --save "
                 +quote(save)+L" "+quote(config);
        const BOOL started=CreateProcessW(exe.c_str(),args.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,
                                          nullptr,play.c_str(),&startup,&process);
        const auto error=GetLastError();
        CloseHandle(log); if(input!=INVALID_HANDLE_VALUE) CloseHandle(input);
        if(!started) throw std::runtime_error("Could not start beta game. Windows error "+std::to_string(error));
        // Do not advance on preflight, duplicate instances, or failed starts.
        // This independent preference never rewrites game settings or saves.
        { std::ofstream artwork(artworkState); artwork << selectedArtwork << '\n'; }
        CloseHandle(process.hThread);
        WaitForSingleObject(process.hProcess,INFINITE);
        DWORD result=1; GetExitCodeProcess(process.hProcess,&result); CloseHandle(process.hProcess);
        CloseHandle(mutex); mutex=nullptr;
        if(result) {
            const auto message=L"The beta closed with an error. Its diagnostic log is here:\n\n"+(capture/L"session.log").wstring();
            MessageBoxW(nullptr,message.c_str(),L"Swordcraft Story 3 Beta",MB_OK|MB_ICONERROR);
        }
        return int(result);
    } catch(const std::exception& error) {
        if(mutex) CloseHandle(mutex);
        if(!check) MessageBoxA(nullptr,error.what(),"Swordcraft Story 3 Beta - Cannot start",MB_OK|MB_ICONERROR);
        return 2;
    }
}
