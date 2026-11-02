#include "../src/gamepad_bridge.hpp"
#include <cstdio>
#include <stdexcept>
#include <fstream>
#include <iterator>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void clear(){gamepadBridge::pulses.clear();gamepadBridge::current={};}
static std::vector<int> downs;
static void down(int key){downs.push_back(key);}
static void up(int){}
static bool examine(){return true;}
static int requested=-1;static bool request(gamepadBridge::MenuTarget target){requested=target==gamepadBridge::MenuTarget::weapons?0:1;return true;}
static int nativeKey(const std::vector<unsigned char>& exe,unsigned char vk){
    for(size_t i=0;i+11<exe.size();i++)if(exe[i]==0x83&&exe[i+1]==0xf9&&exe[i+2]==vk&&exe[i+3]==0x75&&exe[i+4]==6&&exe[i+5]==0xb8&&exe[i+7]==0&&exe[i+8]==0&&exe[i+9]==0&&exe[i+10]==0xc3)return exe[i+6];
    throw std::runtime_error("native key decoder sequence not found");
}
int main(){try{
    std::ifstream file("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\Dementium.exe",std::ios::binary);
    std::vector<unsigned char> exe((std::istreambuf_iterator<char>(file)),{});
    require(nativeKey(exe,VK_SPACE)==gamepadBridge::ConfirmKey&&nativeKey(exe,'E')==gamepadBridge::InteractKey&&nativeKey(exe,VK_ESCAPE)==gamepadBridge::BackKey,"bindings must match the actual game's native key decoder");
    require(nativeKey(exe,VK_LEFT)==14&&nativeKey(exe,VK_RIGHT)==16&&nativeKey(exe,VK_UP)==15&&nativeKey(exe,VK_DOWN)==17,"menu arrows must match the actual decoder");
    require(gamepadBridge::install()==0&&gamepadBridge::menuState()==-1,"engine binding must reject an unrelated executable");
    gamepadBridge::keyDown=down;gamepadBridge::keyUp=up;gamepadBridge::requestMenu=request;

    vrControls::Input input{};input.active=true;vrControls::Router router;
    input.leftX=.5f;input.leftY=1;input.rightX=-.75f;input.rightTrigger=1;
    auto out=router.step(input,0);gamepadBridge::publish(input,out,true,false);
    XINPUT_STATE state{};require(gamepadBridge::getState(0,&state)==ERROR_SUCCESS,"controller must enumerate");
    require(state.Gamepad.sThumbLX>16000&&state.Gamepad.sThumbLY==32767&&state.Gamepad.sThumbRX<-24000,"native walking and turning axes");
    require(state.Gamepad.bRightTrigger==255,"native firing trigger");
    auto packet=state.dwPacketNumber;gamepadBridge::publish(input,out,true,false);gamepadBridge::getState(0,&state);
    require(packet==state.dwPacketNumber,"unchanged state must not create fake changes");
    gamepadBridge::publish(input,out,false,false);gamepadBridge::getState(0,&state);
    XINPUT_GAMEPAD neutral{};require(!memcmp(&state.Gamepad,&neutral,sizeof(neutral)),"tracking loss must release all input");
    clear();input.y=true;out=router.step(input,20);gamepadBridge::publish(input,out,true,false);gamepadBridge::getState(0,&state);
    require(requested==0&&!(state.Gamepad.wButtons&XINPUT_GAMEPAD_START),"Y must open weapons directly instead of generic pause");
    require(!state.Gamepad.bRightTrigger&&!state.Gamepad.sThumbRX,"inventory must suppress firing and turning");
    clear();input={};input.active=true;input.a=true;router.reset();out=router.step(input,40);gamepadBridge::publish(input,out,true,true);gamepadBridge::getState(0,&state);
    require(downs.back()==gamepadBridge::ConfirmKey,"menu A must reach native Space confirm");
    gamepadBridge::keys.clear();gamepadBridge::examineActive=examine;
    gamepadBridge::publish(input,out,true,true);
    require(downs.back()==gamepadBridge::InteractKey,"examine A must reach Take, not ordinary menu Space");
    gamepadBridge::examineActive=gamepadBridge::examineMenuActive;
    input.active=false;out=router.step(input,50);gamepadBridge::publish(input,out,false,true);gamepadBridge::getState(0,&state);
    require(!memcmp(&state.Gamepad,&neutral,sizeof(neutral)),"session loss must clear pending button pulses");
    clear();router.reset();input={};input.active=true;input.a=true;out=router.step(input,60);gamepadBridge::publish(input,out,true,false);
    gamepadBridge::getState(0,&state);require(downs.back()==109,"gameplay A must reach native E interaction");
    input.a=false;input.y=false;router.setContext(vrControls::Context::inventory);input.leftGrip=1;input.rightGrip=1;input.leftY=1;out=router.step(input,70);gamepadBridge::publish(input,out,true,true);gamepadBridge::getState(0,&state);
    require((state.Gamepad.wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER)&&(state.Gamepad.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER),"menu grips must reach native page shoulders");
    require(!state.Gamepad.sThumbLY&&std::find(downs.begin(),downs.end(),15)!=downs.end(),"menu stick must navigate without locomotion");
    input.y=false;input.b=true;out=router.step(input,80);gamepadBridge::publish(input,out,true,true);
    gamepadBridge::getState(0,&state);require(downs.back()==1&&out.context==vrControls::Context::gameplay,"B must emit native cancel even on closing frame");
    input.active=false;out=router.step(input,90);gamepadBridge::publish(input,out,false,false);gamepadBridge::getState(0,&state);require(!state.Gamepad.wButtons,"tracking loss must release native buttons");
    router.reset();input={};input.active=true;input.x=true;out=router.step(input,100);gamepadBridge::publish(input,out,true,false);require(requested==1,"X must open map directly");
    input={};out={};clear();
    require(gamepadBridge::queueTest(99)==ERROR_INVALID_PARAMETER,"reject unknown automation command");
    gamepadBridge::queueTest(1);gamepadBridge::publish(input,out,false,true);require(downs.back()==gamepadBridge::ConfirmKey,"remote menu confirm without controllers");
    gamepadBridge::queueTest(9);gamepadBridge::publish(input,out,false,false);gamepadBridge::getState(0,&state);require(state.Gamepad.sThumbLY>19000,"remote bounded walk");
    gamepadBridge::publish(input,out,false,true);gamepadBridge::getState(0,&state);require(!state.Gamepad.sThumbLY&&!state.Gamepad.bRightTrigger,"remote motion suppressed in menus");
    gamepadBridge::queueTest(0);gamepadBridge::publish(input,out,false,false);gamepadBridge::getState(0,&state);require(!memcmp(&state.Gamepad,&neutral,sizeof(neutral)),"remote release clears input");
    puts("PASS: native controller discovery, walking/turning/fire, packet changes, menu confirmation, inventory suppression, tracking-loss release");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}






