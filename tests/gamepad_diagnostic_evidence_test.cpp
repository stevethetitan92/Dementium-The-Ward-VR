#include "../src/gamepad_bridge.hpp"
#include <cstdio>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    // Keep this test independent of the game executable and native bindings.
    gamepadBridge::engineUnsupported=true;
    check(gamepadBridge::testDuration(9)==1500&&gamepadBridge::testDuration(10)==1500,"walk diagnostics have bounded travel time");
    check(gamepadBridge::testDuration(13)==350&&gamepadBridge::testDuration(15)==350,"fire and interaction pulses stay short");
    vrControls::Input input{};vrControls::Output output{};XINPUT_STATE state{};
    gamepadBridge::queueTest(9);gamepadBridge::publish(input,output,false,false);
    auto evidence=gamepadBridge::readTestEvidence();
    check(evidence.action==9&&evidence.published==1&&evidence.polled==0,"publishing is not evidence of polling");
    gamepadBridge::getState(0,&state);evidence=gamepadBridge::readTestEvidence();
    check(evidence.polled==1&&evidence.nonzeroPolls==1&&state.Gamepad.sThumbLY>0,"walking payload must be counted when read");
    gamepadBridge::testUntil=0;
    gamepadBridge::publish(input,output,false,false);
    gamepadBridge::getState(0,&state);
    check(state.Gamepad.sThumbLY==0&&gamepadBridge::testAction==0,"expired movement releases without another command");
    gamepadBridge::queueTest(9);gamepadBridge::publish(input,output,false,true);
    gamepadBridge::getState(0,&state);evidence=gamepadBridge::readTestEvidence();
    check(evidence.sequence==2&&evidence.suppressedByMenu==1&&evidence.polled==1&&evidence.nonzeroPolls==0,"menu-blocked polling must not count as movement delivery");
    gamepadBridge::queueTest(0);gamepadBridge::publish(input,output,false,false);
    gamepadBridge::getState(0,&state);evidence=gamepadBridge::readTestEvidence();
    check(evidence.action==0&&evidence.polled==0&&state.Gamepad.sThumbLY==0,"release clears test state");
    input.active=true;output.walk=.7f;
    gamepadBridge::publish(input,output,true,false);
    gamepadBridge::getState(0,&state);
    const auto movingPacket=state.dwPacketNumber;
    check(state.Gamepad.sThumbLY>0,"physical route walking payload");
    // Simulate a publisher stall without waiting or launching the game.
    gamepadBridge::updated=GetTickCount64()-501;
    gamepadBridge::getState(0,&state);
    check(state.Gamepad.sThumbLY==0&&state.dwPacketNumber!=movingPacket,"stale release must advance packet number");
    const auto stoppedPacket=state.dwPacketNumber;
    gamepadBridge::getState(0,&state);
    check(state.dwPacketNumber==stoppedPacket,"unchanged stale polls must not advance packet number");
    gamepadBridge::publish(input,output,true,false);
    gamepadBridge::getState(0,&state);
    check(state.Gamepad.sThumbLY>0&&state.dwPacketNumber!=stoppedPacket,"tracking recovery with same held axis must be a fresh packet");
    output.fire=true;gamepadBridge::queueTest(13);
    gamepadBridge::publish(input,output,true,false,true,true);gamepadBridge::getState(0,&state);
    check(state.Gamepad.bRightTrigger==0&&state.Gamepad.sThumbLY>0,"reload blocks physical and diagnostic fire but preserves walking");
    gamepadBridge::publish(input,output,true,false,true,false);gamepadBridge::getState(0,&state);
    check(state.Gamepad.bRightTrigger==255,"closed reload state permits fire");
    gamepadBridge::queueTest(0);gamepadBridge::publish(input,output,true,false,true,true);gamepadBridge::getState(0,&state);
    check(state.Gamepad.bRightTrigger==0,"physical held trigger also respects reload lock");
    puts("PASS: diagnostic delivery, suppression, release, stale packet, recovery and final reload fire lock");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
