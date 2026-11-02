#pragma once
#include "tracking_math.hpp"
#include <mutex>
#include <atomic>
namespace handheldLight {
inline std::atomic<bool> held{};
inline bool beamAvailable{};
// Temporary reload suppression never changes the user's saved switch state.
inline std::atomic<bool> reloadSuppressed{};
inline std::atomic<std::uint64_t> clickedAt{};
inline std::atomic<bool> soundRequested{};
inline std::atomic<std::uint64_t> diagnosticUntil{};
inline std::atomic<unsigned> switchCount{},soundCount{};
inline std::atomic<bool> captureContact{};
inline void(*capture)(){};
class Switch {
 bool previous{},pending{};std::uint64_t started{};
public:
 bool update(bool valid,bool pressed,std::uint64_t now){
  if(!valid){previous=pressed;pending=false;return false;}
  if(pressed&&!previous){started=now;pending=true;clickedAt=now;}
  previous=pressed;
  if(pending&&now>=started&&now-started>=70){pending=false;return true;}
  return false;
 }
};
inline Switch button;
class ReloadHandoff {
 bool waiting{};
public:
 bool blocked(bool reloadBusy,bool grip,float gunDistance,bool valid){
  if(!valid){waiting=false;return true;}
  if(reloadBusy)waiting=true;
  // Finishing an insertion must not replace the magazine with a flashlight
  // while the hand is still gripping beside the receiver.
  else if(waiting&&!grip&&std::isfinite(gunDistance)&&gunDistance>.18f)waiting=false;
  return reloadBusy||waiting;
 }
};
inline ReloadHandoff reloadHandoff;
inline float thumbPress(std::uint64_t now){const auto start=clickedAt.load();if(!start||now<start||now-start>=200)return 0;
 float t=float(now-start);return t<70?t/70.f:t<100?1.f:(200-t)/100.f;
}
inline std::mutex mutex;
inline XrPosef lens{{0,0,0,1},{}};
inline std::uint64_t time{};
inline bool eligible(int weapon,bool rightHeld,bool leftTracked,bool leftBusy,bool stowed=false){
 return leftTracked&&!leftBusy&&(weapon==-1||stowed||(rightHeld&&(weapon==0||weapon==1||weapon==6)));
}
inline XrPosef modelPose(const XrPosef& grip){
 using namespace DirectX;XMFLOAT3 p;
 // Same grip-space contact used to fit the enclosing hand; source lens is -Z.
 XMStoreFloat3(&p,XMVector3TransformNormal(XMVectorSet(.0475f,.025f,-.0325f,0),motionTracking::rotation(grip.orientation)));
 auto result=grip;result.position.x+=p.x;result.position.y+=p.y;result.position.z+=p.z;
 // The source lens already faces controller -Z. Remove the old upright carry
 // quarter-turn; the dedicated flashlight hand receives the inverse turn.
 return result;
}
inline DirectX::XMMATRIX forwardHand(DirectX::FXMMATRIX editorToFree,DirectX::CXMMATRIX world,float blend){
 using namespace DirectX;
 const auto turn=XMMatrixTranslation(-.0475f,-.025f,.0325f)*XMMatrixRotationX(-XM_PIDIV2*blend)*XMMatrixTranslation(.0475f,.025f,-.0325f);
 return editorToFree*turn*XMMatrixInverse(nullptr,editorToFree)*world;
}
inline void publish(const XrPosef& model,std::uint64_t now,bool valid,bool inHand=true){
 std::lock_guard<std::mutex> lock(mutex);beamAvailable=valid;held=valid&&inHand;time=now;lens=model;
 if(valid){using namespace DirectX;XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformNormal(XMVectorSet(0,0,-.106f,0),motionTracking::rotation(model.orientation)));
  lens.position.x+=p.x;lens.position.y+=p.y;lens.position.z+=p.z;}
}
inline bool read(std::uint64_t now,XrPosef& pose){std::lock_guard<std::mutex> lock(mutex);
 if(!beamAvailable||now<time||now-time>=150)return false;pose=lens;return true;
}
}
