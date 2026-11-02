#include "../src/head_lamp.hpp"
#include "../src/body_holsters.hpp"
#include "../src/medicine_saves.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <limits>
static BOOL WINAPI readSpy(HANDLE h,LPVOID b,DWORD n,LPDWORD got,LPOVERLAPPED o){auto result=ReadFile(h,b,n,got,o);SetLastError(0x1234);return result;}
static BOOL WINAPI writeSpy(HANDLE h,LPCVOID b,DWORD n,LPDWORD got,LPOVERLAPPED o){auto result=WriteFile(h,b,n,got,o);SetLastError(0x2345);return result;}
static BOOL WINAPI closeSpy(HANDLE h){auto result=CloseHandle(h);SetLastError(0x3456);return result;}
static unsigned nativeToggleCalls{};static void toggleSpy(void*){++nativeToggleCalls;}
int main(){
 std::array<unsigned char,0x158> lampEntity{};headLamp::base=0x10000000;
 *reinterpret_cast<std::uintptr_t*>(lampEntity.data())=headLamp::base+0x30cdd0;
 auto lampFlags=reinterpret_cast<unsigned*>(lampEntity.data()+0x44);*lampFlags=0x400;
 assert(headLamp::forceOn(lampEntity.data())&&*lampFlags==0x420);
 headLamp::installed=true;headLamp::frame.valid=true;headLamp::frame.time=GetTickCount64();headLamp::originalToggle=toggleSpy;
 headLamp::toggleHook(lampEntity.data()+0xb8);assert(nativeToggleCalls==0&&*lampFlags==0x420&&headLamp::blockedToggles==1);
 for(auto table:headLamp::weaponTables){*reinterpret_cast<std::uintptr_t*>(lampEntity.data())=headLamp::base+table;assert(headLamp::illuminationQuery(lampEntity.data()));}
 *reinterpret_cast<std::uintptr_t*>(lampEntity.data())=headLamp::base+0x30cdd0;
 headLamp::diagnosticBypass=true;headLamp::toggleHook(lampEntity.data()+0xb8);assert(nativeToggleCalls==1);
 headLamp::controllerClick(true,false,true);assert(headLamp::lightEnabled);
 headLamp::controllerClick(true,false,false);assert(!headLamp::lightEnabled);
 assert(headLamp::forceOn(lampEntity.data())&&*lampFlags==0x420);
 headLamp::controllerClick(true,true,true);headLamp::controllerClick(true,false,false);assert(!headLamp::lightEnabled);
 headLamp::controllerClick(true,false,true);headLamp::controllerClick(false,false,false);assert(!headLamp::lightEnabled);
 headLamp::controllerClick(true,false,true);headLamp::controllerClick(true,false,false);assert(headLamp::lightEnabled);
 headLamp::diagnosticBypass=false;headLamp::installed=false;headLamp::frame={};
 *reinterpret_cast<std::uintptr_t*>(lampEntity.data())=0;assert(!headLamp::forceOn(lampEntity.data()));
 bodyHolsters::GripLatch latch;assert(latch.update(.7f)&&latch.update(.6f)&&latch.update(.4f)&&!latch.update(.3f)&&latch.update(.7f));
 assert(weaponGroups::next(-1,255)==-1&&weaponGroups::next(7,255)==7);
 weaponGroups::Requests mailbox;assert(mailbox.submit(-1,0,1));assert(mailbox.take().target==0);
 std::atomic<bool> done{};std::thread writer([&]{for(unsigned i=1;i<=20000;i++)assert(mailbox.submit(i%7,(i+1)%7,i));done=true;});
 do{auto command=mailbox.take();if(command.current>=0){assert(command.current==command.time%7&&command.target==(command.time+1)%7);}}while(!done);writer.join();
 using namespace bodyHolsters;State holsters;
 auto r=holsters.step(true,false,anchors[0],1,255);assert(!r.hidden);
 r=holsters.step(true,true,anchors[0],1,255);assert(r.hidden&&r.changed);
 assert(!holsters.step(true,true,anchors[0],1,255).changed);
 holsters.step(false,false,anchors[0],1,255);assert(!holsters.step(true,true,anchors[0],1,255).changed);
 holsters.step(true,false,anchors[0],1,255);r=holsters.step(true,true,anchors[0],1,255);assert(!r.hidden&&r.equip==1);
 holsters.step(true,false,anchors[2],1,255);r=holsters.step(true,true,anchors[2],1,255);assert(r.equip==6&&!r.hidden);
 State invalid;invalid.step(true,false,anchors[0],1,255);assert(!invalid.step(true,true,anchors[0],-2,255).changed);
 invalid.step(true,false,anchors[0],1,255);auto nan=anchors[0];nan.x=std::numeric_limits<float>::quiet_NaN();assert(!invalid.step(true,true,nan,1,255).changed);
 assert(slot(0)==Slot::Hip&&slot(4)==Slot::Shoulder&&slot(5)==Slot::LeftShoulder&&slot(7)==Slot::None);
 headLamp::Frame lamp;lamp.valid=true;lamp.time=100;float body[12]={1,0,0,0,0,1,0,0,0,0,1,0},matrix[12];
 assert(headLamp::mount(lamp,101,body,{0,0,0},matrix));assert(std::abs(matrix[7]-.035f)<.00001f&&std::abs(matrix[11]+.045f)<.00001f);
 lamp.head.orientation={0,.70710678f,0,.70710678f};assert(headLamp::mount(lamp,101,body,{0,0,0},matrix));assert(matrix[2]>.99f);assert(!headLamp::mount(lamp,251,body,{0,0,0},matrix));
 auto folder=std::filesystem::absolute("../../work/equipment-save-test");std::filesystem::create_directories(folder);medicineSaves::folder=folder/"sidecars";
 auto file=folder/"InGame00.sav";std::vector<char> game(61440,'a');{std::ofstream f(file,std::ios::binary);f.write(game.data(),game.size());}
 medicine::Inventory bank;assert(bank.collect(600)&&bank.collect(601)&&!bank.collect(600));assert(bank.consume(true));
 assert(medicineSaves::save(file,bank));medicine::Inventory restored;assert(medicineSaves::load(file,restored)&&restored.count()==1&&restored.next()==600&&restored.contains(601));
 {std::fstream f(file,std::ios::in|std::ios::out|std::ios::binary);f.put('b');}assert(!medicineSaves::load(file,restored)&&restored.count()==0);
 medicine::Inventory staged;assert(staged.collect(620));pillBottle::carried=0;medicineSaves::stageRestore(staged);assert(pillBottle::carried==0);medicineSaves::commitRestore();assert(pillBottle::carried==1&&pillBottle::supply.next()==620);medicineSaves::commitRestore();assert(pillBottle::carried==1);
 assert(!medicineSaves::nativeSave(L"C:\\Dementium - The Ward\\saves\\nested\\InGame00.sav"));
 assert(medicineSaves::nativeSave(L"C:\\Users\\x\\Dementium - The Ward\\saves\\InGame00.sav"));assert(!medicineSaves::nativeSave(L"C:\\other\\InGame00.sav"));
 // Exercise the actual filtered file-I/O wrappers on a workspace-only mock save.
 auto saveFolder=folder/L"Dementium - The Ward"/L"saves";std::filesystem::create_directories(saveFolder);auto nativeFile=saveFolder/L"InGame00.sav";
 medicineSaves::originalRead=readSpy;medicineSaves::originalWrite=writeSpy;medicineSaves::originalClose=closeSpy;
 pillBottle::supply={};assert(pillBottle::supply.collect(640));pillBottle::carried=1;
 weaponGroups::nativeEquipped=1;weaponGroups::nativeEquipmentValid=true;headLightConstants::enabled=false;bodyHolsters::hidden=true;
 HANDLE fileHandle=CreateFileW(nativeFile.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);assert(fileHandle!=INVALID_HANDLE_VALUE);DWORD transferred{};
 assert(medicineSaves::writeHook(fileHandle,game.data(),static_cast<DWORD>(game.size()),&transferred,nullptr)&&transferred==game.size()&&GetLastError()==0x2345);
 wchar_t diagnosticPath[2048]{};auto pathLength=GetFinalPathNameByHandleW(fileHandle,diagnosticPath,2048,FILE_NAME_NORMALIZED);auto pathError=GetLastError();std::cerr<<"path length="<<pathLength<<" error="<<pathError<<"\n";std::wcerr<<L"save path="<<diagnosticPath<<L" tracked="<<medicineSaves::handles.size()<<L"\n";auto diagnosticClose=medicineSaves::closeHook(fileHandle);auto diagnosticError=GetLastError();std::cerr<<"close="<<diagnosticClose<<" error="<<diagnosticError<<" saves="<<medicineSaves::saves<<" failures="<<medicineSaves::failures<<"\n";assert(diagnosticClose&&diagnosticError==0x3456&&medicineSaves::saves==1);
 assert(pillBottle::supply.collect(641));pillBottle::carried=2;
 weaponGroups::nativeEquipped=2;headLightConstants::enabled=true;bodyHolsters::reveal();
 fileHandle=CreateFileW(nativeFile.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(fileHandle!=INVALID_HANDLE_VALUE);
 assert(medicineSaves::readHook(fileHandle,game.data(),static_cast<DWORD>(game.size()),&transferred,nullptr)&&GetLastError()==0x1234);
 assert(medicineSaves::closeHook(fileHandle)&&GetLastError()==0x3456&&medicineSaves::loads==1);assert(pillBottle::carried==2);
 medicineSaves::equipmentReady=[]{return false;};medicineSaves::commitRestore();assert(pillBottle::carried==1&&pillBottle::supply.next()==640);
 assert(medicineSaves::loadoutQueued&&headLightConstants::enabled&&weaponGroups::requests.take().target==-1);
 medicineSaves::equipmentReady=[]{return true;};medicineSaves::commitRestore();
 assert(!headLightConstants::enabled&&!bodyHolsters::hidden);auto equipment=weaponGroups::requests.take();assert(equipment.inventory&&equipment.target==1);
 weaponGroups::nativeEquipped=1;medicineSaves::commitRestore();assert(bodyHolsters::hidden);bodyHolsters::reveal();
 // Re-loading a matching loadout must not issue another holster/equip transition.
 medicineSaves::stageLoadout(nativeFile);medicineSaves::commitRestore();
 assert(weaponGroups::requests.take().target==-1&&bodyHolsters::hidden&&!headLightConstants::enabled);bodyHolsters::reveal();
 // Corrupted metadata must never restore an invalid count or pickup ledger.
 std::uint64_t fingerprint{};assert(medicineSaves::fingerprint(nativeFile,fingerprint));auto metadata=medicineSaves::sidecar(nativeFile,fingerprint);
 {std::fstream f(metadata,std::ios::in|std::ios::out|std::ios::binary);f.put('X');}assert(!medicineSaves::load(nativeFile,restored)&&restored.count()==0);
 std::cout<<"PASS: holster edges/loss/group ownership, head-mounted light pose/staleness, medicine save roundtrip, pickup ledger and save fingerprint isolation\n";
}


