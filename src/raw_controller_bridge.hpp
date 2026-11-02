#pragma once
#include <windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <mutex>
#include <deque>
#include <map>
#include <array>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cstring>
#include "control_router.hpp"
// Product input adapter: physical OpenXR controller actions are delivered to the
// game's existing raw-input backend. No desktop input or system device is injected.
namespace rawControllerBridge {
using Peek=BOOL(WINAPI*)(LPMSG,HWND,UINT,UINT,UINT);
using Read=UINT(WINAPI*)(HRAWINPUT,UINT,LPVOID,PUINT,UINT);
static Peek originalPeek{};static Read originalRead{};
static std::mutex mutex;static HWND window{};static DWORD windowThread{};
static HANDLE mouse{},keyboard{};static bool enabled=true;
static std::deque<MSG> events;static std::map<LPARAM,RAWINPUT> packets;
static std::array<bool,256> keys{};static bool fireHeld{};
static std::map<UINT,ULONGLONG> keyPulses;
static ULONGLONG previousTime{};static double mouseRemainder{};static uintptr_t serial=1;
static uintptr_t gameBegin{},gameEnd{};
static std::atomic<unsigned> delivered{},readPackets{};
static bool nativeCaller(uintptr_t caller){return caller>=gameBegin&&caller<gameEnd;}
static void setWindow(HWND value){std::lock_guard<std::mutex> guard(mutex);window=value;windowThread=GetWindowThreadProcessId(value,nullptr);}
static void findDevices(){
    UINT count=0;if(GetRawInputDeviceList(nullptr,&count,sizeof(RAWINPUTDEVICELIST))!=0)return;
    std::vector<RAWINPUTDEVICELIST> devices(count);if(GetRawInputDeviceList(devices.data(),&count,sizeof(RAWINPUTDEVICELIST))==UINT(-1))return;
    for(const auto& device:devices){if(!mouse&&device.dwType==RIM_TYPEMOUSE)mouse=device.hDevice;if(!keyboard&&device.dwType==RIM_TYPEKEYBOARD)keyboard=device.hDevice;}
}
static void enqueue(RAWINPUT packet){
    if(!window)return;
    LPARAM id=static_cast<LPARAM>(0x444d000000000000ull|serial++);packet.header.dwSize=sizeof(RAWINPUT);packet.header.wParam=RIM_INPUT;
    packets[id]=packet;MSG message{};message.hwnd=window;message.message=WM_INPUT;message.wParam=RIM_INPUT;message.lParam=id;message.time=GetTickCount();events.push_back(message);
    if(packets.size()>512){packets.erase(packets.begin());if(!events.empty())events.pop_front();}
}
static void key(UINT code,bool down){
    if(code>=keys.size()||keys[code]==down)return;keys[code]=down;
    RAWINPUT packet{};packet.header.dwType=RIM_TYPEKEYBOARD;packet.header.hDevice=keyboard;
    auto& data=packet.data.keyboard;data.VKey=static_cast<USHORT>(code);data.MakeCode=static_cast<USHORT>(MapVirtualKeyW(code,MAPVK_VK_TO_VSC));data.Flags=down?RI_KEY_MAKE:RI_KEY_BREAK;
    if(code==VK_LEFT||code==VK_RIGHT||code==VK_UP||code==VK_DOWN)data.Flags|=RI_KEY_E0;
    data.Message=down?WM_KEYDOWN:WM_KEYUP;enqueue(packet);
}
static void tap(UINT code){key(code,true);keyPulses[code]=GetTickCount64()+100;}
static void mouseEvent(LONG dx,USHORT buttons=0,SHORT wheel=0){RAWINPUT packet{};packet.header.dwType=RIM_TYPEMOUSE;packet.header.hDevice=mouse;packet.data.mouse.lLastX=dx;packet.data.mouse.usButtonFlags=buttons;packet.data.mouse.usButtonData=static_cast<USHORT>(wheel);enqueue(packet);}
static void publish(const vrControls::Input& input,const vrControls::Output& output,bool tracked,bool nativeMenu){
    std::lock_guard<std::mutex> guard(mutex);if(!window)return;if(!mouse||!keyboard)findDevices();
    auto now=GetTickCount64();double seconds=previousTime?(std::min)(.05,(now-previousTime)/1000.):0;previousTime=now;
    for(auto pulse=keyPulses.begin();pulse!=keyPulses.end();)if(pulse->second<=now){key(pulse->first,false);pulse=keyPulses.erase(pulse);}else ++pulse;
    bool active=enabled&&input.active&&tracked;
    key('W',active&&!nativeMenu&&output.walk>.25f);key('S',active&&!nativeMenu&&output.walk<-.25f);key('A',active&&!nativeMenu&&output.strafe<-.25f);key('D',active&&!nativeMenu&&output.strafe>.25f);
    float navigateX=nativeMenu?input.leftX:output.navigateX,navigateY=nativeMenu?input.leftY:output.navigateY;
    key(VK_LEFT,active&&navigateX<-.35f);key(VK_RIGHT,active&&navigateX>.35f);key(VK_UP,active&&navigateY>.35f);key(VK_DOWN,active&&navigateY<-.35f);
    bool fire=active&&!nativeMenu&&output.fire;if(fire!=fireHeld){mouseEvent(0,fire?RI_MOUSE_LEFT_BUTTON_DOWN:RI_MOUSE_LEFT_BUTTON_UP);fireHeld=fire;}
    if(!active)return;
    mouseRemainder+=(nativeMenu?0:output.turn)*900*seconds;LONG dx=static_cast<LONG>(mouseRemainder);mouseRemainder-=dx;if(dx)mouseEvent(dx);
    if(output.confirm)tap(nativeMenu||output.context==vrControls::Context::inventory?VK_SPACE:'E');
    if(output.back)tap(VK_ESCAPE);if(output.weaponAction)tap('R');if(output.flashlight)tap('F');
    if(output.toggleInventory)tap(output.context==vrControls::Context::inventory?'I':VK_ESCAPE);
    if(output.nextItem)mouseEvent(0,RI_MOUSE_WHEEL,-WHEEL_DELTA);
    // Physical map and the dedicated mod menu are handled by the VR object/UI
    // layer; they deliberately do not fall back to the native flat map.
}
static BOOL WINAPI peek(LPMSG message,HWND target,UINT minimum,UINT maximum,UINT flags){
    if(message&&nativeCaller(reinterpret_cast<uintptr_t>(_ReturnAddress()))&&GetCurrentThreadId()==windowThread&&(!target||target==window)){
        std::lock_guard<std::mutex> guard(mutex);
        if(!events.empty()&&((!minimum&&!maximum)||(WM_INPUT>=minimum&&WM_INPUT<=maximum))){*message=events.front();if(flags&PM_REMOVE){events.pop_front();++delivered;}return TRUE;}
    }
    return originalPeek(message,target,minimum,maximum,flags);
}
static UINT WINAPI read(HRAWINPUT handle,UINT command,LPVOID output,PUINT bytes,UINT headerSize){
    {std::lock_guard<std::mutex> guard(mutex);auto found=packets.find(reinterpret_cast<LPARAM>(handle));if(found!=packets.end()){
        if(!bytes||headerSize!=sizeof(RAWINPUTHEADER)){SetLastError(ERROR_INVALID_PARAMETER);return UINT(-1);}UINT needed=command==RID_HEADER?sizeof(RAWINPUTHEADER):sizeof(RAWINPUT);
        if(!output){*bytes=needed;return 0;}if(*bytes<needed){*bytes=needed;SetLastError(ERROR_INSUFFICIENT_BUFFER);return UINT(-1);}
        memcpy(output,&found->second,needed);*bytes=needed;if(command==RID_INPUT){packets.erase(found);++readPackets;}return needed;
    }}return originalRead(handle,command,output,bytes,headerSize);
}
static bool install(){
    auto module=GetModuleHandleW(nullptr);auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(module);auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<unsigned char*>(module)+dos->e_lfanew);gameBegin=reinterpret_cast<uintptr_t>(module);gameEnd=gameBegin+nt->OptionalHeader.SizeOfImage;
    auto user=GetModuleHandleW(L"user32.dll");return MH_CreateHook(reinterpret_cast<void*>(GetProcAddress(user,"PeekMessageA")),reinterpret_cast<void*>(peek),reinterpret_cast<void**>(&originalPeek))==MH_OK&&MH_CreateHook(reinterpret_cast<void*>(GetProcAddress(user,"GetRawInputData")),reinterpret_cast<void*>(read),reinterpret_cast<void**>(&originalRead))==MH_OK;
}
}
