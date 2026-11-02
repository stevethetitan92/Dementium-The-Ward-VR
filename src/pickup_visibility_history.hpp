#pragma once
#include "native_pickup_sector.hpp"
#include <array>
#include <atomic>
#include <cmath>
#include <map>
#include <mutex>
namespace pickupVisibilityHistory {
struct Event {
    std::uintptr_t scene{},entity{};int id{},plane{-1};std::uint64_t time{};std::uint32_t thread{};
    bool visible{},tested{},baseline{};float nativeResult{},effectiveResult{},margin{};
    nativePickupSector::Snapshot sector;
    bool sphereKnown{},planesKnown{};std::array<float,4> sphere{};std::array<float,24> testedPlanes{};
};
struct Snapshot {std::array<Event,64> events{};size_t count{};};
// Default-OFF bounded diagnostic. Preserve transient gate transitions without
// adding per-event disk writes or changing native visibility. Call-site values
// are evidence only; no pointers are dereferenced by this history.
class History {
    std::mutex mutex;std::atomic<std::uint64_t> until{};std::uint64_t started{};
    std::map<std::pair<std::uintptr_t,std::uintptr_t>,Event> latest;
    std::array<Event,64> ring{};size_t next{},count{};
    static unsigned resultClass(float value){return !std::isfinite(value)?2u:(value==0?0u:1u);}
    // Retain newly available (or lost) evidence even when visibility is unchanged.
    // Ordinary motion of known sphere/planes is not itself a gate transition.
    static bool same(const Event& a,const Event& b){return a.id==b.id&&a.visible==b.visible&&a.tested==b.tested&&a.sphereKnown==b.sphereKnown&&a.planesKnown==b.planesKnown&&resultClass(a.nativeResult)==resultClass(b.nativeResult)&&resultClass(a.effectiveResult)==resultClass(b.effectiveResult)&&a.sector.status==b.sector.status;}
public:
    bool armed()const{return until.load()!=0;}
    bool mayCapture(std::uint64_t now)const{auto end=until.load();return end&&now<end;}
    bool arm(std::uint64_t now,std::uint64_t duration){std::lock_guard<std::mutex> guard(mutex);until=0;latest.clear();next=count=0;if(!duration)return false;if(duration>45000)duration=45000;if(duration>UINT64_MAX-now)return false;started=now;until=now+duration;return true;}
    void stop(){std::lock_guard<std::mutex> guard(mutex);until=0;}
    bool observe(const Event& event){
        if(!until.load())return false;
        std::lock_guard<std::mutex> guard(mutex);auto end=until.load();if(!end||event.time<started||event.time>=end)return false;
        auto key=std::make_pair(event.scene,event.entity);auto previous=latest.find(key);if(previous!=latest.end()&&same(previous->second,event))return false;
        auto recorded=event;recorded.baseline=previous==latest.end();if(latest.size()>=1024&&recorded.baseline)latest.clear();latest[key]=recorded;ring[next]=recorded;next=(next+1)%ring.size();if(count<ring.size())++count;return true;
    }
    Snapshot snapshot(){std::lock_guard<std::mutex> guard(mutex);Snapshot result;result.count=count;for(size_t i=0;i<count;++i)result.events[i]=ring[(next+ring.size()-count+i)%ring.size()];return result;}
};
}
