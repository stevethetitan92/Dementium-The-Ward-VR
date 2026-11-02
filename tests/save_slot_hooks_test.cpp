#include "../src/head_lamp.hpp"
#include "../src/save_slot_menu_adapter.hpp"
#include <cassert>
#include <iostream>
int main(){namespace fs=std::filesystem;auto root=fs::absolute("../../work/save-hook-tests")/std::to_string(GetTickCount64());auto native=root/L"Dementium - The Ward"/L"saves";fs::create_directories(native);assert(!fs::exists(root/"metadata"));std::string bytes(61440,'A');
 {std::ofstream out(native/L"InGame00.sav",std::ios::binary);out.write(bytes.data(),bytes.size());}
 assert(MH_Initialize()==MH_OK);assert(medicineSaves::install(root/"metadata"));auto installed=saveSlotFiles::install(root/"slots",native);if(!installed)std::cerr<<"install error="<<saveSlotFiles::installError<<"\n";assert(installed);assert(MH_EnableHook(MH_ALL_HOOKS)==MH_OK);
 // Match the native index reader: CRT _wfopen must use the same slot as Win32 reads.
 auto index=native/L"-saveindex";
 {FILE* f=_wfopen(index.c_str(),L"wb");assert(f);assert(fwrite("slot-one",1,8,f)==8);assert(fclose(f)==0);}
 {FILE* f=_wfopen(index.c_str(),L"rb");assert(f);char value[8]{};assert(fread(value,1,8,f)==8);assert(std::string(value,8)=="slot-one");assert(fclose(f)==0);}
 auto file=native/L"InGame00.sav";assert(GetFileAttributesW(file.c_str())!=INVALID_FILE_ATTRIBUTES);
 HANDLE h=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);assert(h!=INVALID_HANDLE_VALUE);char a{};DWORD n{};assert(ReadFile(h,&a,1,&n,nullptr)&&a=='A');CloseHandle(h);
 assert(saveSlotFiles::choose(1,true));assert(GetFileAttributesW(file.c_str())==INVALID_FILE_ATTRIBUTES);
 assert(_wfopen(index.c_str(),L"rb")==nullptr); // No native/previous-slot fallback.

 h=CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);assert(h!=INVALID_HANDLE_VALUE);bytes.assign(61440,'B');assert(WriteFile(h,bytes.data(),DWORD(bytes.size()),&n,nullptr));CloseHandle(h);
 WIN32_FILE_ATTRIBUTE_DATA attrs{};assert(GetFileAttributesExW(file.c_str(),GetFileExInfoStandard,&attrs)&&attrs.nFileSizeLow==61440);
 WIN32_FIND_DATAW found{};h=FindFirstFileW((native/L"InGame*.sav").c_str(),&found);assert(h!=INVALID_HANDLE_VALUE&&std::wstring(found.cFileName)==L"InGame00.sav");FindClose(h);
 assert(!saveSlotFiles::choose(1,true));auto moved=native/L"InGame01.sav";assert(MoveFileExW(file.c_str(),moved.c_str(),0));assert(GetFileAttributesW(file.c_str())==INVALID_FILE_ATTRIBUTES);assert(DeleteFileW(moved.c_str()));
 assert(saveSlotFiles::choose(0));h=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);assert(h!=INVALID_HANDLE_VALUE);assert(ReadFile(h,&a,1,&n,nullptr)&&a=='A');CloseHandle(h);
 {FILE* f=_wfopen(index.c_str(),L"rb");assert(f);char value[8]{};assert(fread(value,1,8,f)==8);assert(std::string(value,8)=="slot-one");assert(fclose(f)==0);}
 assert(MH_DisableHook(MH_ALL_HOOKS)==MH_OK);MH_Uninitialize();saveSlotFiles::enabled=false;
 assert(!fs::exists(index)); // CRT writes did not alter the native save directory.
 {std::ifstream original(file,std::ios::binary);assert(original.get(a)&&a=='A');}
 std::cout<<"PASS: real Windows create/read/write/stat/enumerate/rename/delete redirected per slot; original save preserved\n";
}

