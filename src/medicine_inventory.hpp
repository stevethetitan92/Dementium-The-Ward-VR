#pragma once
#include <array>
#include <cstdint>
namespace medicine {
inline constexpr unsigned capacity=2;
class Inventory {
    std::array<int,3> bottles{};unsigned used{};std::array<std::uint64_t,7> collected{};
public:
    unsigned count()const{return used;}
    bool full()const{return used>=capacity;}
    bool contains(int id)const{return id>=600&&id<1000&&(collected[(id-600)/64]&(1ull<<((id-600)%64)));}
    std::array<std::uint64_t,7> ledger()const{return collected;}
    bool canCollect(int id)const{return !full()&&id>=600&&id<1000&&!contains(id);}
    bool collect(int id){if(!canCollect(id))return false;bottles[used++]=id;collected[(id-600)/64]|=1ull<<((id-600)%64);return true;}
    int next()const{return used?bottles[used-1]:-1;}
    std::array<int,3> items()const{return bottles;}
    bool restore(unsigned count,const std::array<int,3>& values,const std::array<std::uint64_t,7>& history={}){
        if(count>3)return false;for(unsigned i=0;i<count;i++)if(values[i]<600||values[i]>=1000)return false;
        if(history[6]>>16)return false;for(unsigned i=0;i<count;i++)for(unsigned j=0;j<i;j++)if(values[i]==values[j])return false;
        collected=history;for(unsigned i=0;i<count;i++)collected[(values[i]-600)/64]|=1ull<<((values[i]-600)%64);
        // Keep the version-2 three-entry disk format readable. Preserve the
        // collection ledger when reducing an older inventory to the new limit.
        bottles={};used=count<capacity?count:capacity;for(unsigned i=0;i<used;i++)bottles[i]=values[i];return true;
    }
    bool consume(bool accepted){if(!accepted||!used)return false;bottles[--used]=0;return true;}
};
}
