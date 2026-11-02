#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <string>
#include <stdexcept>
#include "remote_export.hpp"
#include "init_diagnostics.hpp"
static uintptr_t remoteModule(DWORD pid,const wchar_t* name,DWORD* imageBytes=nullptr) {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(snapshot==INVALID_HANDLE_VALUE) throw std::runtime_error("Module enumeration failed");
    MODULEENTRY32W m{}; m.dwSize=sizeof(m); uintptr_t base=0;
    for(BOOL ok=Module32FirstW(snapshot,&m);ok;ok=Module32NextW(snapshot,&m)) if(!_wcsicmp(m.szModule,name)) {base=reinterpret_cast<uintptr_t>(m.modBaseAddr);if(imageBytes)*imageBytes=m.modBaseSize;break;}
    CloseHandle(snapshot); return base;
}
static uintptr_t loadedExport(HANDLE process,uintptr_t module,DWORD bytes,const char* name){
    auto function=remoteExport::locate(module,bytes,name,[&](DWORD rva,void* output,size_t count){SIZE_T copied{};return ReadProcessMemory(process,reinterpret_cast<const void*>(module+rva),output,count,&copied)&&copied==count;});
    MEMORY_BASIC_INFORMATION page{};
    if(!function||!VirtualQueryEx(process,reinterpret_cast<const void*>(function),&page,sizeof page)||page.State!=MEM_COMMIT||!(page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))||(page.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return 0;
    return function;
}
static DWORD run(HANDLE process,uintptr_t function,void* argument) {
    initDiagnostics::write("remote_thread_begin address=%p",reinterpret_cast<void*>(function));
    HANDLE thread=CreateRemoteThread(process,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(function),argument,0,nullptr);
    if(!thread){initDiagnostics::os("CreateRemoteThread",GetLastError());throw std::runtime_error("Remote initialization thread failed");}
    DWORD wait=WaitForSingleObject(thread,30000);if(wait!=WAIT_OBJECT_0) {DWORD error=GetLastError();initDiagnostics::write("remote_wait_result=0x%08lx",wait);if(wait==WAIT_FAILED)initDiagnostics::os("WaitForSingleObject",error);CloseHandle(thread);throw std::runtime_error("Initialization incomplete; remote allocation retained");}
    DWORD result=0; if(!GetExitCodeThread(thread,&result)){DWORD error=GetLastError();CloseHandle(thread);initDiagnostics::os("GetExitCodeThread",error);throw std::runtime_error("Cannot read remote thread exit status");}CloseHandle(thread);initDiagnostics::write("remote_thread_exit=0x%08lx (not an OpenXR result)",result);return result;
}
struct ResumeOnExit {HANDLE thread{},process{};bool success{}; ~ResumeOnExit(){if(thread){if(success)ResumeThread(thread);else if(process)TerminateProcess(process,ERROR_DLL_INIT_FAILED);CloseHandle(thread);}if(process)CloseHandle(process);}};
int wmain(int argc,wchar_t** argv) {
    initDiagnostics::open(true);wprintf(L"initialization_log=%ls\n",initDiagnostics::path);fflush(stdout);
    initDiagnostics::module(nullptr);
    ResumeOnExit suspended;
    try {
        bool control=argc==3&&(!wcscmp(argv[1],L"--recenter")||!wcscmp(argv[1],L"--capture")||!wcscmp(argv[1],L"--capture-shot")||!wcsncmp(argv[1],L"--input=",8)||!wcsncmp(argv[1],L"--posture=",10));
        if(argc!=2&&!control) {puts("Usage: attach.exe --launch | --launch-desktop | <PID> | --recenter <PID> | --capture <PID>");return 1;}
        wchar_t configuredGame[32768]{};
        if(!GetEnvironmentVariableW(L"DEMENTIUM_GAME_EXE",configuredGame,32768))wcscpy_s(configuredGame,L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\Dementium.exe");
        std::wstring gameDirectory=configuredGame;auto separator=gameDirectory.find_last_of(L"\\/");
        if(separator==std::wstring::npos)throw std::runtime_error("Game executable must have an absolute path");gameDirectory.resize(separator);
        initDiagnostics::write("configured_game=%ls",configuredGame);
        const bool desktopLaunch=!wcscmp(argv[1],L"--launch-desktop");
        wchar_t* end{}; unsigned long parsed=wcstoul(argv[control?2:1],&end,10);
        if(!wcscmp(argv[1],L"--launch")||desktopLaunch) {
            if(desktopLaunch&&!SetEnvironmentVariableW(L"DEMENTIUM_DESKTOP_STEREO_TEST",L"1"))throw std::runtime_error("Cannot select desktop-only stereo test");
            wchar_t selectedRuntime[32768]{};if(!GetEnvironmentVariableW(L"DEMENTIUM_XR_RUNTIME_JSON",selectedRuntime,32768))wcscpy_s(selectedRuntime,L"C:\\Program Files\\Virtual Desktop Streamer\\OpenXR\\virtualdesktop-openxr.json");
            SetEnvironmentVariableW(L"XR_RUNTIME_JSON",selectedRuntime);initDiagnostics::write("launch_runtime_manifest=%ls",selectedRuntime);
            STARTUPINFOW startup{};startup.cb=sizeof(startup); PROCESS_INFORMATION info{};
            const wchar_t* game=configuredGame;
            if(!CreateProcessW(game,nullptr,nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,gameDirectory.c_str(),&startup,&info)) throw std::runtime_error("Game launch failed");
            parsed=info.dwProcessId; suspended.thread=info.hThread;suspended.process=info.hProcess;
        } else if(!parsed||*end) throw std::runtime_error("Invalid PID");
        DWORD pid=parsed; printf("game_pid=%lu\n",pid);
        HANDLE process=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ|(desktopLaunch?SYNCHRONIZE:0),FALSE,pid);
        if(!process) throw std::runtime_error("OpenProcess failed");
        wchar_t exe[32768]{}; DWORD chars=32768;
        if(!QueryFullProcessImageNameW(process,0,exe,&chars)||_wcsicmp(exe,configuredGame)) {
            CloseHandle(process);throw std::runtime_error("Refusing to attach: PID is not the specified Dementium installation");
        }
        DWORD existingBytes{};uintptr_t existing=suspended.thread?0:remoteModule(pid,L"DementiumOpenXR.dll",&existingBytes);
        if(!control&&existing) {CloseHandle(process);throw std::runtime_error("Bridge already loaded; restart game before attaching again");}
        if(control&&!existing) {CloseHandle(process);throw std::runtime_error("The VR bridge is not loaded in this game");}
        wchar_t path[32768]{}; GetModuleFileNameW(nullptr,path,32768); auto slash=wcsrchr(path,L'\\');
        if(!slash) throw std::runtime_error("Launcher path missing"); wcscpy_s(slash+1,32768-(slash+1-path),L"DementiumOpenXR.dll");
        unsigned testCode=0;bool testInput=control&&!wcsncmp(argv[1],L"--input=",8);
        bool posture=control&&!wcsncmp(argv[1],L"--posture=",10);
        if(posture){if(!wcscmp(argv[1]+10,L"seated"))testCode=1;else if(!wcscmp(argv[1]+10,L"standing"))testCode=2;else throw std::runtime_error("Unknown posture calibration");}
        if(testInput){const wchar_t* actions[]={L"release",L"confirm",L"back",L"weapons",L"map",L"left",L"right",L"up",L"down",L"forward",L"backward",L"turn-left",L"turn-right",L"fire",L"lamp-native",L"lamp-headset",L"lamp-click",L"clip-hands",L"geometry-native-pose",L"geometry-no-extras",L"geometry-no-clip",L"geometry-no-cache",L"geometry-normal",L"geometry-replay",L"geometry-replay-off",L"geometry-no-fog",L"lamp-enable",L"lamp-disable",L"desktop-look-left",L"desktop-look-down",L"desktop-lean",L"desktop-neutral",L"geometry-no-frustum",L"geometry-no-sectors",L"lighting-evidence",L"native-zero-constant-guard",L"native-zero-constant-off",L"pickup-history-arm",L"pickup-history-dump",L"support-grip-hold",L"support-grip-sweep",L"support-grip-release",L"support-grip-off",L"reload-native",L"confirm-native-pad",L"desktop-pickup-near",L"desktop-pickup-far",L"geometry-fog-on",L"desktop-inventory-toggle",L"desktop-inventory-left",L"desktop-inventory-right",L"desktop-inventory-up",L"desktop-inventory-down",L"desktop-inventory-confirm",L"desktop-inventory-back",L"reload-view-empty",L"reload-view-held",L"reload-view-inserted",L"reload-view-slide",L"reload-view-off",L"reload-test-neutral",L"reload-test-eject",L"reload-test-pouch",L"reload-test-insert",L"reload-test-grab",L"reload-test-pull",L"reload-test-release",L"reload-test-off",L"reload-test-held-shell",L"left-hand-align",L"ammo-pouch-align",L"reload-test-seat",L"support-pump-cycle",L"scope-test",L"scope-test-off",L"holster-left-grab",L"holster-right-grab",L"holster-release",L"medicine-heal-check",L"flashlight-switch",L"mounted-light-view",L"holster-hip-grab",L"holster-shoulder-grab",L"magazine-exit-capture",L"chamber-feed-capture",L"pistol-well-view",L"melee-contact-sweep",L"casing-exit-capture",L"ejection-side-view"};bool found=false;for(unsigned i=0;i<sizeof(actions)/sizeof(actions[0]);i++)if(!wcscmp(argv[1]+8,actions[i])){testCode=i;found=true;}if(!found)throw std::runtime_error("Unknown input action");}
        const char* entry=posture?"CalibratePosture":testInput?"TestInput":control?(!wcscmp(argv[1],L"--capture-shot")?"CaptureNextHuntingShot":!wcscmp(argv[1],L"--capture")?"CaptureEyes":"Recenter"):"Initialize";
        if(control) {
            auto function=loadedExport(process,existing,existingBytes,entry);
            if(!function){CloseHandle(process);throw std::runtime_error("Loaded bridge control export unavailable or not executable");}
            DWORD result=run(process,function,reinterpret_cast<void*>(static_cast<uintptr_t>(testCode)));CloseHandle(process);printf("control_status=%lu\n",result);return static_cast<int>(result);
        }
        if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("Bridge DLL missing");
        auto load=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");
        MEMORY_BASIC_INFORMATION mi{}; VirtualQuery(reinterpret_cast<void*>(load),&mi,sizeof(mi));
        wchar_t provider[MAX_PATH]{}; GetModuleFileNameW(static_cast<HMODULE>(mi.AllocationBase),provider,MAX_PATH);
        auto name=wcsrchr(provider,L'\\'); if(!name) throw std::runtime_error("LoadLibrary provider missing");
        // Windows system DLLs share their ASLR base within this boot/session. A newly
        // suspended process has not completed loader initialization, so Toolhelp cannot
        // enumerate its modules until the first remote thread has run the loader.
        uintptr_t remoteBase=suspended.thread?reinterpret_cast<uintptr_t>(mi.AllocationBase):remoteModule(pid,name+1);
        if(!remoteBase) throw std::runtime_error("Remote loader provider missing");
        uintptr_t remoteLoad=remoteBase+reinterpret_cast<uintptr_t>(load)-reinterpret_cast<uintptr_t>(mi.AllocationBase);
        SIZE_T bytes=(wcslen(path)+1)*sizeof(wchar_t);
        void* memory=VirtualAllocEx(process,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if(!memory) throw std::runtime_error("Remote allocation failed");
        if(!WriteProcessMemory(process,memory,path,bytes,nullptr)) {VirtualFreeEx(process,memory,0,MEM_RELEASE);throw std::runtime_error("Remote DLL path write failed");}
        initDiagnostics::mark("remote LoadLibraryW");
        DWORD loaderStatus=run(process,remoteLoad,memory);initDiagnostics::write("loader_thread_exit=0x%08lx module_enumeration_is_authoritative",loaderStatus); VirtualFreeEx(process,memory,0,MEM_RELEASE);
        DWORD moduleBytes{};uintptr_t module=remoteModule(pid,L"DementiumOpenXR.dll",&moduleBytes); if(!module) throw std::runtime_error("Bridge DLL did not load");
        auto initialize=loadedExport(process,module,moduleBytes,entry);if(!initialize)throw std::runtime_error("Loaded bridge initialization export unavailable or not executable");
        initDiagnostics::mark("remote Initialize");
        DWORD status=run(process,initialize,nullptr);
        if(status){initDiagnostics::printTail();printf("bridge_initialize_status=%lu hex=0x%08lx source=remote_thread_exit%s\n",status,status,status==0xe06d7363?" classification=MSVC_CPP_EXCEPTION_NOT_NORMAL_RETURN":"");wprintf(L"See initialization_log=%ls\n",initDiagnostics::path);
            if(suspended.thread){TerminateProcess(process,status);CloseHandle(suspended.thread);suspended.thread=nullptr;}
            CloseHandle(process);return static_cast<int>(status);}
        suspended.success=true;
        if(desktopLaunch&&suspended.thread){
            // Initialization success only confirms injection. Observe startup
            // separately so an immediate native exit cannot masquerade as a test.
            if(ResumeThread(suspended.thread)==DWORD(-1)){CloseHandle(process);throw std::runtime_error("Cannot resume desktop test");}
            CloseHandle(suspended.thread);suspended.thread=nullptr;
            const DWORD wait=WaitForSingleObject(process,5000);
            if(wait==WAIT_OBJECT_0){DWORD exitCode{};GetExitCodeProcess(process,&exitCode);CloseHandle(process);printf("bridge_initialize_status=%lu\ndesktop_startup_exited=1 exit_code=0x%08lx\n",status,exitCode);return 1;}
            if(wait==WAIT_FAILED){CloseHandle(process);throw std::runtime_error("Cannot observe desktop startup");}
            puts("desktop_startup_alive=1 rendering_verified=0");
        }
        CloseHandle(process);
        printf("bridge_initialize_status=%lu\n",status); return static_cast<int>(status);
    } catch(const std::exception& e) {initDiagnostics::write("launcher_exception=%s win32=%lu",e.what(),GetLastError());initDiagnostics::printTail();printf("error=%s (win32=%lu)\n",e.what(),GetLastError());return 1;}
}





