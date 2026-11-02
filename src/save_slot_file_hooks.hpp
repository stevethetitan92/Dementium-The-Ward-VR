#pragma once
#include "save_slot_session.hpp"
#include "save_slot_legacy_metadata.hpp"
#include "medicine_saves.hpp"
#include <memory>
namespace saveSlotFiles {
inline std::unique_ptr<saveSlots::Sessions> sessions;
inline std::mutex mutex;
inline bool enabled{};
inline const char* installError="";
inline bool route(LPCWSTR input,std::wstring& output){if(!enabled||!input||medicineSaves::internal)return false;std::lock_guard<std::mutex> lock(mutex);auto path=sessions->route(input);if(!path)return false;output=path->wstring();return true;}
struct Path {std::wstring storage;LPCWSTR value;explicit Path(LPCWSTR p):value(p){if(route(p,storage))value=storage.c_str();}};
using Attributes=DWORD(WINAPI*)(LPCWSTR);inline Attributes originalAttributes{};
inline DWORD WINAPI attributes(LPCWSTR p){Path path(p);return originalAttributes(path.value);}
using AttributesEx=BOOL(WINAPI*)(LPCWSTR,GET_FILEEX_INFO_LEVELS,LPVOID);inline AttributesEx originalAttributesEx{};
inline BOOL WINAPI attributesEx(LPCWSTR p,GET_FILEEX_INFO_LEVELS level,LPVOID data){Path path(p);return originalAttributesEx(path.value,level,data);}
using Find=HANDLE(WINAPI*)(LPCWSTR,LPWIN32_FIND_DATAW);inline Find originalFind{};
inline HANDLE WINAPI find(LPCWSTR p,LPWIN32_FIND_DATAW data){Path path(p);return originalFind(path.value,data);}
using FindEx=HANDLE(WINAPI*)(LPCWSTR,FINDEX_INFO_LEVELS,LPVOID,FINDEX_SEARCH_OPS,LPVOID,DWORD);inline FindEx originalFindEx{};
inline HANDLE WINAPI findEx(LPCWSTR p,FINDEX_INFO_LEVELS level,LPVOID data,FINDEX_SEARCH_OPS search,LPVOID filter,DWORD flags){Path path(p);return originalFindEx(path.value,level,data,search,filter,flags);}
using Delete=BOOL(WINAPI*)(LPCWSTR);inline Delete originalDelete{};
inline BOOL WINAPI erase(LPCWSTR p){Path path(p);return originalDelete(path.value);}
using Move=BOOL(WINAPI*)(LPCWSTR,LPCWSTR);inline Move originalMove{};
inline BOOL WINAPI move(LPCWSTR a,LPCWSTR b){Path from(a),to(b);return originalMove(from.value,to.value);}
using MoveEx=BOOL(WINAPI*)(LPCWSTR,LPCWSTR,DWORD);inline MoveEx originalMoveEx{};
inline BOOL WINAPI moveEx(LPCWSTR a,LPCWSTR b,DWORD flags){Path from(a),to(b);return originalMoveEx(from.value,to.value,flags);}
using Replace=BOOL(WINAPI*)(LPCWSTR,LPCWSTR,LPCWSTR,DWORD,LPVOID,LPVOID);inline Replace originalReplace{};
inline BOOL WINAPI replace(LPCWSTR a,LPCWSTR b,LPCWSTR backup,DWORD flags,LPVOID x,LPVOID y){Path to(a),from(b),old(backup);return originalReplace(to.value,from.value,old.value,flags,x,y);}
struct Internal {bool previous=medicineSaves::internal;Internal(){medicineSaves::internal=true;}~Internal(){medicineSaves::internal=previous;}};
inline bool install(const std::filesystem::path& root,const std::filesystem::path& native){
 Internal bypass;std::lock_guard<std::mutex> lock(mutex);auto candidate=std::make_unique<saveSlots::Sessions>(root,native);if(!candidate->initialize())return false;
 // Preserve the existing fingerprint-bound medicine/equipment records for Slot1.
 auto metadata=candidate->metadata();std::error_code ec;std::filesystem::create_directories(metadata,ec);if(ec)return false;
 if(candidate->selected()==0)saveSlots::importLegacyMedicine(medicineSaves::folder,metadata);
 struct Hook {const char* name;void* replacement;void** original;};
 Hook hooks[]={{"GetFileAttributesW",(void*)attributes,(void**)&originalAttributes},{"GetFileAttributesExW",(void*)attributesEx,(void**)&originalAttributesEx},{"FindFirstFileW",(void*)find,(void**)&originalFind},{"FindFirstFileExW",(void*)findEx,(void**)&originalFindEx},{"DeleteFileW",(void*)erase,(void**)&originalDelete},{"MoveFileW",(void*)move,(void**)&originalMove},{"MoveFileExW",(void*)moveEx,(void**)&originalMoveEx},{"ReplaceFileW",(void*)replace,(void**)&originalReplace}};
 auto module=GetModuleHandleW(L"KernelBase.dll");std::vector<void*> installed;
 for(auto& h:hooks){auto address=(void*)GetProcAddress(module,h.name);if(!address)address=(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),h.name);if(!address||MH_CreateHook(address,h.replacement,h.original)!=MH_OK){installError=h.name;for(auto p:installed)MH_RemoveHook(p);return false;}installed.push_back(address);}
 sessions=std::move(candidate);medicineSaves::folder=metadata;medicineSaves::routePath=route;enabled=true;return true;
}
inline bool choose(unsigned slot,bool fresh=false,bool overwrite=false){
 Internal bypass;std::lock_guard<std::mutex> lock(mutex);if(!sessions)return false;
 bool ok=fresh?sessions->newGame(slot,overwrite):sessions->hasProgress(slot)&&sessions->select(slot);if(!ok)return false;
 medicineSaves::folder=sessions->metadata();std::error_code ec;std::filesystem::create_directories(medicineSaves::folder,ec);if(ec)return false;
 {std::lock_guard<std::mutex> restore(medicineSaves::restoreMutex);medicineSaves::loadoutQueued=false;medicineSaves::restoreQueued=false;medicineSaves::pendingStowWeapon=-2;}
 if(!fresh){medicine::Inventory supply;medicineSaves::load(sessions->directory()/L"InGame00.sav",supply);medicineSaves::stageRestore(supply);medicineSaves::stageLoadout(sessions->directory()/L"InGame00.sav");}
 if(fresh){medicineSaves::stageRestore({});std::lock_guard<std::mutex> restore(medicineSaves::restoreMutex);medicineSaves::queuedLoadout={};medicineSaves::loadoutQueued=true;medicineSaves::startupLoading=true;}
 return true;
}
}

