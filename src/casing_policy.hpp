#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace casingPolicy {
// Weapon indices match the selected asset bank. Counts represent spent cases,
// not reserve ammunition; this policy never changes native ammo.
class Ejection {
    std::array<unsigned,5> seen{};
    std::array<bool,5> initialized{};
    bool shellSpent{};
public:
    void reset(){*this={};}
    unsigned update(unsigned weapon,unsigned shots,bool cylinderFullyOpen,bool pumpPastEjection,bool valid){
        if(weapon>=5)return 0;
        if(!valid){seen[weapon]=shots;initialized[weapon]=true;return 0;}
        unsigned fired=initialized[weapon]&&shots>=seen[weapon]?shots-seen[weapon]:0;
        initialized[weapon]=true;seen[weapon]=shots;
        // A long absence is not permission to emit an accumulated visual burst.
        fired=(std::min)(fired,1u);
        // Requested revolver behavior: retain cases, including when opening
        // the cylinder. Never queue them for a later visual burst.
        if(weapon==0)return 0;
        if(weapon==3){
            if(fired)shellSpent=true;
            if(pumpPastEjection&&shellSpent){shellSpent=false;return 1;}
            return 0;
        }
        return fired; // pistol, M16, magazine-only hunting rifle
    }
};
}
