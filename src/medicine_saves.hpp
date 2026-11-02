#pragma once
#include "pill_bottle.hpp"
#include "weapon_group_runtime.hpp"
#include "body_holsters.hpp"
#include "head_light_constants.hpp"
#include <cwctype>
#include <map>
namespace medicineSaves {
inline std::filesystem::path folder;
inline std::uint64_t hash(const void* data,size_t size){auto bytes=static_cast<const unsigned char*>(data);std::uint64_t h=14695981039346656037ull;for(size_t i=0;i<size;i++){h^=bytes[i];h*=1099511628211ull;}return h;}
inline bool nativeSave(const std::wstring& name){
    std::wstring lower=name;for(auto& c:lower)c=static_cast<wchar_t>(std::towlower(c));
    auto at=lower.rfind(L"\\dementium - the ward\\saves\\");if(at==std::wstring::npos)return false;
    auto file=std::filesystem::path(lower).filename().wstring();if(lower.substr(at+std::wstring(L"\\dementium - the ward\\saves\\").size())!=file)return false;return file.size()==12&&file.substr(0,6)==L"ingame"&&file[6]>=L'0'&&file[6]<=L'9'&&file[7]>=L'0'&&file[7]<=L'9'&&file.substr(8)==L".sav";
}
struct Record {std::uint32_t magic=0x44454d44,version=2;std::uint64_t fingerprint{};std::uint32_t count{};std::array<int,3> ids{};std::array<std::uint64_t,7> ledger{};std::uint64_t checksum{};};
static_assert(sizeof(Record)==96,"Fixed sidecar layout");
inline bool fingerprint(const std::filesystem::path& file,std::uint64_t& value){
    std::ifstream f(file,std::ios::binary|std::ios::ate);if(!f||f.tellg()!=61440)return false;f.seekg(0);std::vector<char> bytes(61440);if(!f.read(bytes.data(),bytes.size()))return false;value=hash(bytes.data(),bytes.size());return true;
}
inline std::filesystem::path sidecar(const std::filesystem::path& file,std::uint64_t value){
    auto slot=file.filename().wstring();for(auto& c:slot)c=static_cast<wchar_t>(std::towlower(c));
    wchar_t suffix[40];swprintf_s(suffix,L"-%016llx.vrmeds",static_cast<unsigned long long>(value));return folder/(slot+suffix);
}
inline bool save(const std::filesystem::path& file,const medicine::Inventory& supply,std::uint64_t known=0){
    Record r;r.fingerprint=known;if((!known&&!fingerprint(file,r.fingerprint))||folder.empty())return false;r.count=supply.count();r.ids=supply.items();r.ledger=supply.ledger();r.checksum=hash(&r,offsetof(Record,checksum));
    std::error_code error;std::filesystem::create_directories(folder,error);if(error)return false;
    auto target=sidecar(file,r.fingerprint),temp=target;temp+=L".tmp";
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out.write(reinterpret_cast<const char*>(&r),sizeof r)||!out.flush())return false;}
    if(!MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::filesystem::remove(temp,error);return false;}return true;
}
inline bool load(const std::filesystem::path& file,medicine::Inventory& supply,std::uint64_t known=0){
    supply={};std::uint64_t expected=known;if((!known&&!fingerprint(file,expected))||folder.empty())return false;
    auto target=sidecar(file,expected);std::ifstream f(target,std::ios::binary|std::ios::ate);if(!f||f.tellg()!=static_cast<std::streamoff>(sizeof(Record)))return false;f.seekg(0);Record r{};if(!f.read(reinterpret_cast<char*>(&r),sizeof r))return false;
    if(r.magic!=0x44454d44||r.version!=2||r.fingerprint!=expected||r.checksum!=hash(&r,offsetof(Record,checksum)))return false;
    return supply.restore(r.count,r.ids,r.ledger);
}
inline std::mutex restoreMutex;inline medicine::Inventory queuedInventory;inline bool restoreQueued{};
// Equipment belongs to the exact native save, not the most recently played session.
struct Loadout {std::uint32_t magic=0x31514c44;int weapon=-1;std::uint32_t light=1,stowed=0;std::uint64_t fingerprint{},checksum{};};
inline Loadout queuedLoadout;inline bool loadoutQueued{};
inline std::atomic<bool> startupLoading{};
inline int pendingStowWeapon=-2;
inline bool(*equipmentReady)(){};
inline void(*loadoutRestored)(int,int,bool){};
inline bool saveLoadout(const std::filesystem::path& file,Loadout state,std::uint64_t known=0){
    state.fingerprint=known;if((!known&&!fingerprint(file,state.fingerprint))||folder.empty())return false;
    state.checksum=hash(&state,offsetof(Loadout,checksum));auto path=sidecar(file,state.fingerprint);path+=L".equipment";auto temp=path;temp+=L".tmp";
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out.write(reinterpret_cast<const char*>(&state),sizeof state)||!out.flush())return false;}
    return MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
inline void stageLoadout(const std::filesystem::path& file,std::uint64_t known=0){
    Loadout state;std::uint64_t expected=known;if(!known&&!fingerprint(file,expected))return;
    auto path=sidecar(file,expected);path+=L".equipment";std::ifstream in(path,std::ios::binary|std::ios::ate);
    bool valid=in&&in.tellg()==sizeof state;if(valid){in.seekg(0);valid=bool(in.read(reinterpret_cast<char*>(&state),sizeof state))&&state.magic==0x31514c44&&state.weapon>=-1&&state.weapon<=6&&state.light<=1&&state.stowed<=1&&state.fingerprint==expected&&state.checksum==hash(&state,offsetof(Loadout,checksum));}
    std::lock_guard<std::mutex> lock(restoreMutex);queuedLoadout=valid?state:Loadout{};loadoutQueued=true;startupLoading=true;
}
inline void stageRestore(const medicine::Inventory& next){std::lock_guard<std::mutex> lock(restoreMutex);queuedInventory=next;restoreQueued=true;}
inline void commitRestore(){
    {Loadout state;bool apply=false;const bool ready=!equipmentReady||equipmentReady();{std::lock_guard<std::mutex> lock(restoreMutex);if(loadoutQueued&&ready){state=queuedLoadout;loadoutQueued=false;apply=true;}}
     if(apply){bodyHolsters::reveal();headLightConstants::enabled=state.light!=0;pendingStowWeapon=state.stowed?state.weapon:-2;
        // Do not restart the native holster/equip animation for an already restored weapon.
        const int current=weaponGroups::nativeEquipmentValid?weaponGroups::nativeEquipped.load():-2;
        const bool request=state.weapon>=0&&current!=state.weapon;
        if(request)weaponGroups::equipFromInventory(-1,state.weapon,GetTickCount64());
        if(loadoutRestored)loadoutRestored(state.weapon,current,request);
     }}
    if(pendingStowWeapon>=-1&&weaponGroups::nativeEquipmentValid&&weaponGroups::nativeEquipped==pendingStowWeapon){std::lock_guard<std::mutex> lock(bodyHolsters::stateMutex);bodyHolsters::state.restoreStowed(pendingStowWeapon);bodyHolsters::hidden=true;bodyHolsters::display.stowed=true;pendingStowWeapon=-2;}
    medicine::Inventory next;{std::lock_guard<std::mutex> lock(restoreMutex);if(!restoreQueued)return;next=queuedInventory;restoreQueued=false;}
    std::lock_guard<std::mutex> lock(pillBottle::mutex);pillBottle::supply=next;pillBottle::carried=next.count();pillBottle::hold={};pillBottle::holding=false;pillBottle::heldSide=-1;pillBottle::pocketHeld=false;pillBottle::records.clear();pillBottle::useRequested=false;pillBottle::previousGrip=true;pillBottle::previousRightGrip=true;pillBottle::controls={};
}
using IO=BOOL(WINAPI*)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
using Write=BOOL(WINAPI*)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
using Close=BOOL(WINAPI*)(HANDLE);
using Create=HANDLE(WINAPI*)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
inline IO originalRead{};inline Write originalWrite{};inline Close originalClose{};
inline Create originalCreate{};
inline bool(*routePath)(LPCWSTR,std::wstring&){};
struct Pending {std::filesystem::path file;bool write{};medicine::Inventory supply;Loadout equipment;};
inline std::mutex mutex;inline std::map<HANDLE,Pending> handles;inline thread_local bool internal{};
inline std::atomic<unsigned> saves{},loads{},missing{},failures{};
inline void(*fileObserved)(const wchar_t*,bool){};
inline void observe(HANDLE handle,bool write){
    wchar_t path[2048];DWORD length=GetFinalPathNameByHandleW(handle,path,2048,FILE_NAME_NORMALIZED);if(!length||length>=2048)return;
    if(fileObserved&&nativeSave(path))fileObserved(path,write);
    if(!nativeSave(path))return;
    Pending item;item.file=path;item.write=write;
    if(write){std::lock_guard<std::mutex> lock(pillBottle::mutex);item.supply=pillBottle::supply;item.equipment.weapon=weaponGroups::nativeEquipped.load();item.equipment.light=headLightConstants::enabled.load();item.equipment.stowed=bodyHolsters::hidden.load();}
    std::lock_guard<std::mutex> lock(mutex);if(handles.size()<16||handles.count(handle)){auto it=handles.find(handle);if(it==handles.end()||write)handles[handle]=std::move(item);}
}
inline BOOL WINAPI readHook(HANDLE h,LPVOID b,DWORD n,LPDWORD got,LPOVERLAPPED overlap){auto result=originalRead(h,b,n,got,overlap);auto error=GetLastError();try{if(!internal&&result&&!overlap&&got&&*got)observe(h,false);}catch(...){++failures;}SetLastError(error);return result;}
inline HANDLE WINAPI createHook(LPCWSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,DWORD creation,DWORD flags,HANDLE templ){
    std::wstring mapped;if(!internal&&routePath&&path&&routePath(path,mapped))path=mapped.c_str();
    auto result=originalCreate(path,access,share,security,creation,flags,templ);auto error=GetLastError();
    try{if(!internal&&result!=INVALID_HANDLE_VALUE&&(access&(GENERIC_READ|GENERIC_WRITE|FILE_READ_DATA|FILE_WRITE_DATA)))observe(result,(access&(GENERIC_WRITE|FILE_WRITE_DATA))!=0);}catch(...){++failures;}
    SetLastError(error);return result;
}
inline BOOL WINAPI writeHook(HANDLE h,LPCVOID b,DWORD n,LPDWORD got,LPOVERLAPPED overlap){auto result=originalWrite(h,b,n,got,overlap);auto error=GetLastError();try{if(!internal&&result&&!overlap&&got&&*got)observe(h,true);}catch(...){++failures;}SetLastError(error);return result;}
inline BOOL WINAPI closeHook(HANDLE h){
    if(internal)return originalClose(h);Pending pending;bool found=false;
    {std::lock_guard<std::mutex> lock(mutex);auto i=handles.find(h);if(i!=handles.end()){pending=std::move(i->second);handles.erase(i);found=true;}}
    auto result=originalClose(h);auto error=GetLastError();if(!found||!result)return result;
    struct Guard{Guard(){internal=true;}~Guard(){internal=false;}} guard;
    try{if(pending.write){if(save(pending.file,pending.supply)&&saveLoadout(pending.file,pending.equipment))++saves;else ++failures;}
    else {medicine::Inventory next;bool restored=load(pending.file,next);stageRestore(next);stageLoadout(pending.file);
        if(restored)++loads;else ++missing;
    }
    }catch(...){++failures;}SetLastError(error);return result;
}
inline bool install(const std::filesystem::path& directory){
    equipmentReady=[]{return manualReload::menuState()==0&&weaponGroups::nativeEquipmentValid.load();};
    folder=directory;pillBottle::restorePending=commitRestore;auto module=GetModuleHandleW(L"KernelBase.dll");if(!module)return false;
    auto read=GetProcAddress(module,"ReadFile"),write=GetProcAddress(module,"WriteFile"),close=GetProcAddress(module,"CloseHandle");
    if(!read||!write||!close)return false;
    if(MH_CreateHook(read,reinterpret_cast<void*>(readHook),reinterpret_cast<void**>(&originalRead))!=MH_OK)return false;
    if(MH_CreateHook(write,reinterpret_cast<void*>(writeHook),reinterpret_cast<void**>(&originalWrite))!=MH_OK){MH_RemoveHook(read);return false;}
    if(MH_CreateHook(close,reinterpret_cast<void*>(closeHook),reinterpret_cast<void**>(&originalClose))!=MH_OK){MH_RemoveHook(write);MH_RemoveHook(read);return false;}
    auto create=GetProcAddress(module,"CreateFileW");if(!create||MH_CreateHook(create,reinterpret_cast<void*>(createHook),reinterpret_cast<void**>(&originalCreate))!=MH_OK){MH_RemoveHook(close);MH_RemoveHook(write);MH_RemoveHook(read);return false;}return true;
}
}
