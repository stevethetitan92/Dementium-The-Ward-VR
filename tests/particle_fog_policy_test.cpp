#include "../src/particle_fog_policy.hpp"
#include <array>
#include <cassert>
int main(){
 std::array<unsigned char,1040> source;for(unsigned i=0;i<source.size();++i)source[i]=static_cast<unsigned char>(i*17+9);
 auto original=source;std::array<unsigned char,1048> target;target.fill(0xa5);
 assert(particleFog::prepare(target.data()+4,source.data(),1040,false));
 assert(source==original);
 for(unsigned i=0;i<1040;i++)assert(target[4+i]==(i>=12&&i<16?0:source[i]));
 for(unsigned i=0;i<4;i++){assert(target[i]==0xa5);assert(target[1044+i]==0xa5);}
 auto prior=target;
 assert(!particleFog::prepare(target.data()+4,source.data(),1040,true));
 assert(!particleFog::prepare(target.data()+4,source.data(),448,false));
 assert(!particleFog::prepare(target.data()+4,nullptr,1040,false));
 assert(target==prior);
}
