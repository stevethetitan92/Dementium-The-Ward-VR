#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <atomic>
namespace dropSurface {
struct Query {std::uintptr_t scene{};float x{},y{},z{},range{},clearance{},floor{};bool hit{};};
inline std::mutex mutex;
inline std::vector<std::shared_ptr<Query>> pending;
inline bool(*trace)(const Query&,float&){};
inline std::atomic<unsigned> queries{},hits{};
inline std::shared_ptr<Query> request(std::uintptr_t scene,float x,float y,float z,float units){
 auto q=std::make_shared<Query>();q->scene=scene;q->x=x;q->y=y;q->z=z;q->range=4*units;q->clearance=.065f*units;
 std::lock_guard<std::mutex> lock(mutex);if(pending.size()<16)pending.push_back(q);return q;
}
// Called only from native weapon updates: never trace the game world from
// a rendering thread. Publish the result under the same lock as the reader.
inline void service(){
 std::vector<std::shared_ptr<Query>> jobs;
 {std::lock_guard<std::mutex> lock(mutex);jobs.swap(pending);}
 if(!trace)return;
 for(auto& q:jobs){float floor{};bool hit=trace(*q,floor)&&std::isfinite(floor);++queries;if(hit)++hits;
  std::lock_guard<std::mutex> lock(mutex);q->floor=floor;q->hit=hit;}
}
inline bool floor(const std::shared_ptr<Query>& q,float& y){std::lock_guard<std::mutex> lock(mutex);if(!q||!q->hit)return false;y=q->floor;return true;}
struct Motion {float y{},spinTime{};};
inline Motion motion(float start,float floor,bool hit,float units,float t,float downwardSpeed=.1f){
 t=(std::max)(0.f,t);float g=9.81f*units,v=downwardSpeed*units;
 if(!hit)return {start-v*t-.5f*g*t*t,t};
 if(floor>=start)return {floor,0};
 float impact=(-v+std::sqrt(v*v+2*g*(start-floor)))/g;
 if(t<impact)return {start-v*t-.5f*g*t*t,t};
 // One small damped bounce, then rest; the fitted magazine never crosses
// the queried floor plane. Orientation stops changing on the landing.
 float bounce=(v+g*impact)*.18f,elapsed=t-impact,duration=2*bounce/g;
 if(elapsed<duration)return {floor+bounce*elapsed-.5f*g*elapsed*elapsed,impact+elapsed*.2f};
 return {floor,impact+duration*.2f};
}
}
