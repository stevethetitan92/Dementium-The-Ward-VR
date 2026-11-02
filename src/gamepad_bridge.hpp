#pragma once
#include "native_map_transition.hpp"
#include <windows.h>
#include <Xinput.h>
#include <MinHook.h>
#include <mutex>
#include <vector>
#include <algorithm>
#include <atomic>
#include "control_router.hpp"
// Native in-process gamepad interface driven by physical OpenXR controllers.
// Steam's overlay can forward these calls, so caller-module filtering is unsafe.
namespace gamepadBridge {
using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);


static std::mutex mutex;static XINPUT_STATE current{};
static bool connected{},enabled=true;static ULONGLONG updated{};
// Bounded test commands are consumed on the normal publish thread, never on
// the launcher's remote control thread. Zero means release all test input.
static std::atomic<unsigned> testRequest{};
static unsigned testAction{};static ULONGLONG testUntil{};
// Walking needs enough travel to compare scene positions. Other diagnostic
// actions retain their short pulse, especially fire and interaction buttons.
static constexpr ULONGLONG testDuration(unsigned action){return action==9||action==10?1500:350;}
struct TestEvidence { unsigned sequence{},action{},published{},suppressedByMenu{},polled{},nonzeroPolls{}; };
static TestEvidence testEvidence{};
static TestEvidence readTestEvidence(){std::lock_guard<std::mutex> guard(mutex);return testEvidence;}
static DWORD queueTest(unsigned command){if(command>15)return ERROR_INVALID_PARAMETER;testRequest=command+1;return 0;}
static std::atomic<unsigned> nativePolls{},vrPolls{};
static std::atomic<unsigned> enginePolls{};
static std::atomic<std::uint64_t> firePressSerial{};
static GetState engineOriginal{};static bool engineBound{},engineUnsupported{};
static DWORD WINAPI engineGetState(DWORD index,XINPUT_STATE* state);
using KeyEvent=void(*)(int);
static constexpr int ConfirmKey=73,InteractKey=109,BackKey=1;
static KeyEvent keyDown{},keyUp{};
struct KeyPulse{int key;ULONGLONG until;};static std::vector<KeyPulse> keys;
static int menuState(){
    if(!engineBound)return -1;
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto root=*reinterpret_cast<unsigned char**>(base+0x3fc468);if(!root)return -1;
    MEMORY_BASIC_INFORMATION region{};if(!VirtualQuery(root,&region,sizeof(region))||region.State!=MEM_COMMIT||(region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return -1;
    auto screen=*reinterpret_cast<void**>(root+8);if(!screen)return -1;
    // UI::impl current screen versus its embedded UIMSInGame screen.
    return screen!=root+0x15a8;
}
enum class MenuTarget { weapons,map };
static bool examineMenuActive(){
    if(menuState()!=1)return false;
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto root=*reinterpret_cast<unsigned char**>(base+0x3fc468);
    auto screen=root?*reinterpret_cast<unsigned char**>(root+8):nullptr;
    MEMORY_BASIC_INFORMATION region{};
    if(!screen||!VirtualQuery(screen,&region,sizeof(region))||region.State!=MEM_COMMIT||(region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
    // Retail UIMSExamine accepts the gameplay Take action (E), not menu Space.
    return *reinterpret_cast<void**>(screen)==base+0x310950;
}
static bool(*examineActive)()=examineMenuActive;
static int confirmKey(bool modal){return !modal||examineActive()?InteractKey:ConfirmKey;}
static bool mapMenuActive(){
    if(menuState()!=1)return false;
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto root=*reinterpret_cast<unsigned char**>(base+0x3fc468);
    return root&&*reinterpret_cast<void**>(root+8)==root+0xe0;
}
static bool openNativeMenu(MenuTarget menu){
    if(!engineBound)return false;
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto root=*reinterpret_cast<unsigned char**>(base+0x3fc468);if(!root)return false;
    const unsigned char signature[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0xfa};
    if(memcmp(base+0xbbcb0,signature,sizeof(signature)))return false;
    auto target=root+(menu==MenuTarget::weapons?0xcd0:0xe0);
    auto expected=base+(menu==MenuTarget::weapons?0x312f78:0x312570);
    if(*reinterpret_cast<void**>(target)!=expected||*reinterpret_cast<void**>(root+0x15a8)!=base+0x310ae0)return false;
    if(*reinterpret_cast<void**>(root+8)==target)target=root+0x15a8;
    // Use the game's normal deferred transition rather than replacing UI pointers.
    reinterpret_cast<void(*)(void*,void*)>(base+0xbbcb0)(root,target);return true;
}
using MenuRequest=bool(*)(MenuTarget);static MenuRequest requestMenu=openNativeMenu;
static void shortenInventoryMapWait(){
    if(!engineBound)return;
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if(!nativeMapTransition::supported(base))return;
    auto root=*reinterpret_cast<unsigned char**>(base+0x3fc468);
    if(root)nativeMapTransition::shorten(root,true);
}
static void bindKeys(){
    if(!engineBound||keyDown)return;
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto down=*reinterpret_cast<void**>(base+0xc15a00),up=*reinterpret_cast<void**>(base+0xc15a08);
    if(down==base+0x1264d0&&up==base+0x1264f0){keyDown=reinterpret_cast<KeyEvent>(down);keyUp=reinterpret_cast<KeyEvent>(up);}
}
static void keyPulse(bool event,int key,ULONGLONG now){
    if(!event||!keyDown||!keyUp)return;
    for(auto& held:keys)if(held.key==key){held.until=now+120;return;}
    keyDown(key);keys.push_back({key,now+120});
}
static void keyHold(bool held,int key){
    if(!keyDown||!keyUp)return;
    auto found=std::find_if(keys.begin(),keys.end(),[&](const KeyPulse& value){return value.key==key;});
    if(held&&found==keys.end()){keyDown(key);keys.push_back({key,~ULONGLONG(0)});}
    else if(!held&&found!=keys.end()){keyUp(key);keys.erase(found);}
}
static void bindEngine(){
    if(!enabled||engineBound||engineUnsupported)return;
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    // Steam v1.1.10: validate both the polling load and initialization store.
    // Steam overlay installs its XInput detour after ours and bypasses it.
    // Bind only this game's private function slot, retaining other-port behavior.
    const unsigned char load[]={0x48,0x8b,0x05,0xd4,0x2b,0x9f,0x00,0x48,0x85,0xc0};
    const unsigned char store[]={0x48,0x89,0x05,0x0f,0x1e,0x9f,0x00};
    if(nt->OptionalHeader.SizeOfImage<0xc1b1a8||memcmp(base+0x2285c5,load,sizeof(load))||memcmp(base+0x22938a,store,sizeof(store))){engineUnsupported=true;return;}
    auto slot=reinterpret_cast<void* volatile*>(base+0xc1b1a0);
    auto previous=InterlockedCompareExchangePointer(slot,nullptr,nullptr);
    if(!previous)return; // Native initialization has not loaded XInput yet.
    engineOriginal=reinterpret_cast<GetState>(previous);
    engineBound=InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(engineGetState),previous)==previous;
}
struct Pulse{WORD button;ULONGLONG until;};static std::vector<Pulse> pulses;
static SHORT axis(float value){return static_cast<SHORT>((std::max)(-1.f,(std::min)(1.f,value))*32767);}
static bool(*captureSaveMenu)(const vrControls::Input&,const vrControls::Output&,unsigned){};
static void publish(const vrControls::Input& raw,const vrControls::Output& routed,bool poseValid,bool nativeMenu,bool gameplayStarted=true,bool suppressFire=false){
    bindEngine();
    bindKeys();

    std::lock_guard<std::mutex> guard(mutex);auto now=GetTickCount64();updated=now;
    auto request=testRequest.exchange(0);if(request){testAction=request-1;testUntil=now+testDuration(testAction);auto sequence=testEvidence.sequence+1;testEvidence={};testEvidence.sequence=sequence;testEvidence.action=testAction;}
    if(now>=testUntil)testAction=0;
    const bool testing=testAction!=0;
    connected=enabled&&((raw.active&&poseValid)||testing);
    if(captureSaveMenu&&captureSaveMenu(raw,routed,request?testAction:0)){
        for(auto i=keys.begin();i!=keys.end();)if(i->until<now){keyUp(i->key);i=keys.erase(i);}else ++i;
        current.Gamepad={};pulses.clear();return;
    }
    if(request&&testAction){
        if(testAction==1)keyPulse(true,confirmKey(nativeMenu),now);
        else if(testAction==2)keyPulse(true,BackKey,now);
        else if(testAction==3&&gameplayStarted)requestMenu(MenuTarget::weapons);
        else if(testAction==4&&gameplayStarted)requestMenu(MenuTarget::map);
    }

    const bool modal=nativeMenu;
    if(testing){++testEvidence.published;if(modal)++testEvidence.suppressedByMenu;}
    // Physical OpenXR edges drive this game's registered native actions only.
    keyPulse(connected&&routed.confirm,confirmKey(modal),now);
    keyPulse(connected&&modal&&(routed.back||routed.weaponAction),BackKey,now);
    if(connected&&gameplayStarted){if(routed.toggleInventory)requestMenu(MenuTarget::weapons);else if(routed.openMap)requestMenu(MenuTarget::map);}
    keyHold((testing&&testAction==5)||(connected&&modal&&!routed.menuCapture&&raw.leftX<-.5f),14);keyHold((testing&&testAction==6)||(connected&&modal&&!routed.menuCapture&&raw.leftX>.5f),16);
    keyHold((testing&&testAction==7)||(connected&&modal&&!routed.menuCapture&&raw.leftY>.5f),15);keyHold((testing&&testAction==8)||(connected&&modal&&!routed.menuCapture&&raw.leftY<-.5f),17);
    for(auto i=keys.begin();i!=keys.end();)if(!connected||i->until<now){keyUp(i->key);i=keys.erase(i);}else ++i;
    auto pulse=[&](bool event,WORD button){if(event)pulses.push_back({button,now+120});};
    pulse(routed.weaponAction&&!modal,XINPUT_GAMEPAD_X);
    pulse(routed.nextItem,XINPUT_GAMEPAD_RIGHT_SHOULDER);
    pulse(routed.previousPage,XINPUT_GAMEPAD_LEFT_SHOULDER);pulse(routed.nextPage,XINPUT_GAMEPAD_RIGHT_SHOULDER);
    XINPUT_GAMEPAD pad{};pad.sThumbLX=axis(nativeMenu?raw.leftX:routed.strafe);pad.sThumbLY=axis(nativeMenu?raw.leftY:routed.walk);pad.sThumbRX=axis(nativeMenu?0:routed.turn);
    if(routed.context==vrControls::Context::inventory){pad.sThumbLX=axis(routed.navigateX);pad.sThumbLY=axis(routed.navigateY);}
    if(modal)pad.sThumbLX=pad.sThumbLY=pad.sThumbRX=0;
    pad.bRightTrigger=!nativeMenu&&routed.fire?255:0;
    for(auto i=pulses.begin();i!=pulses.end();)if(i->until<now)i=pulses.erase(i);else{pad.wButtons|=i->button;++i;}
    if(testing&&!modal){
        if(testAction==9)pad.sThumbLY=axis(.6f);
        if(testAction==10)pad.sThumbLY=axis(-.6f);
        if(testAction==11)pad.sThumbRX=axis(-.5f);
        if(testAction==12)pad.sThumbRX=axis(.5f);
        if(testAction==13)pad.bRightTrigger=255;
        if(testAction==14)pad.wButtons|=XINPUT_GAMEPAD_X;
        if(testAction==15)pad.wButtons|=XINPUT_GAMEPAD_A;
    }
    // Apply the reload lock after every source, including diagnostic pulses.
    // The native shared firing routine is too late to prevent ammo deduction.
    if(suppressFire)pad.bRightTrigger=0;
    if(!connected){pad={};pulses.clear();}
    if(pad.bRightTrigger>127&&current.Gamepad.bRightTrigger<=127)++firePressSerial;
    if(memcmp(&current.Gamepad,&pad,sizeof(pad)))++current.dwPacketNumber;current.Gamepad=pad;
}
static DWORD WINAPI getState(DWORD index,XINPUT_STATE* state){
    if(index==0&&state){++nativePolls;std::lock_guard<std::mutex> guard(mutex);if(enabled){auto now=GetTickCount64();if(!connected||now-updated>=500){const XINPUT_GAMEPAD neutral{};if(memcmp(&current.Gamepad,&neutral,sizeof(neutral))){current.Gamepad={};++current.dwPacketNumber;}}*state=current;if(testAction&&now<testUntil){++testEvidence.polled;const XINPUT_GAMEPAD neutral{};if(memcmp(&state->Gamepad,&neutral,sizeof(neutral)))++testEvidence.nonzeroPolls;}++vrPolls;return ERROR_SUCCESS;}}
    return engineOriginal?engineOriginal(index,state):ERROR_DEVICE_NOT_CONNECTED;
}
static DWORD WINAPI engineGetState(DWORD index,XINPUT_STATE* state){
    ++enginePolls;
    if(enabled&&index==0)return getState(index,state);
    return engineOriginal?engineOriginal(index,state):ERROR_DEVICE_NOT_CONNECTED;
}
static unsigned install(){
    // Public XInput detours lose to Steam's later overlay hooks. Only the
    // signature-checked private slot is bound, after native initialization.
    bindEngine();return engineUnsupported?0:1;
}
}



