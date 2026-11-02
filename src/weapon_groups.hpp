#pragma once
#include <array>
#include <mutex>
#include <cstdint>
namespace weaponGroups {
// Native entity IDs, established from each weapon's type virtual function.
inline constexpr unsigned ids[7]={6,2,5,3,7,4,1};
struct Command {int current=-1,target=-1;std::uint64_t time{};bool inventory{};};
class Requests {
    std::mutex mutex;Command pending;
public:
    bool submit(int current,int target,std::uint64_t time,bool inventory=false){if(current< -1||current>6||target< -1||target>6||(current==-1&&target==-1))return false;std::lock_guard<std::mutex> lock(mutex);pending={current,target,time,inventory};return true;}
    Command take(){std::lock_guard<std::mutex> lock(mutex);auto value=pending;pending={};return value;}
};
inline int next(int current,unsigned owned){
    if(current<0||current>6)return current;
    constexpr int groups[3][3]={{1,0,-1},{6,5,-1},{3,2,4}};
    for(auto& group:groups)for(int i=0;i<3;i++)if(group[i]==current){
        int size=group[2]<0?2:3;
        for(int step=1;step<size;step++){int candidate=group[(i+step)%size];if(owned&(1u<<ids[candidate]))return candidate;}
        return current;
    }
    return current;
}
}
