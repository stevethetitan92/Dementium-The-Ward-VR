#include "../src/native_map_transition.hpp"
#include <array>
#include <vector>
#include <fstream>
#include <cassert>
#include <cstdio>
#include <limits>
#include <algorithm>
template<class T> void put(unsigned char* p,unsigned offset,T value){std::memcpy(p+offset,&value,sizeof value);}
int main(int argc,char** argv){
    assert(argc==2);std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> exe((std::istreambuf_iterator<char>(file)),{});assert(exe.size()>0x1000);
    const auto pe=nativeMapTransition::read<unsigned>(exe.data(),0x3c);
    const auto sections=nativeMapTransition::read<std::uint16_t>(exe.data(),pe+6);
    const auto table=pe+24+nativeMapTransition::read<std::uint16_t>(exe.data(),pe+20);
    std::vector<unsigned char> image(0xbc000);
    for(unsigned i=0;i<sections;++i){const auto s=table+40*i;auto va=nativeMapTransition::read<unsigned>(exe.data(),s+12),size=nativeMapTransition::read<unsigned>(exe.data(),s+16),raw=nativeMapTransition::read<unsigned>(exe.data(),s+20);assert(std::size_t(raw)+size<=exe.size());if(va<image.size())std::memcpy(image.data()+va,exe.data()+raw,(std::min)(std::size_t(size),image.size()-va));}
    assert(nativeMapTransition::supported(image.data()));
    for(auto offset:{0xbbcb0u,0xbb837u,0xbbd72u}){image[offset]^=1;assert(!nativeMapTransition::supported(image.data()));image[offset]^=1;}
    std::array<unsigned char,0x70> root{};
    auto seed=[&](){root.fill(0);auto address=reinterpret_cast<std::uintptr_t>(root.data());put(root.data(),8,address+0x15a8);put(root.data(),0x10,address+0xe0);root[0x18]=root[0x64]=1;put(root.data(),0x58,0.f);put(root.data(),0x5c,-1.f);put(root.data(),0x60,.0625f);};
    seed();auto before=root;assert(nativeMapTransition::shorten(root.data(),true));auto expected=before;put(expected.data(),0x60,.5f);assert(root==expected);assert(!nativeMapTransition::shorten(root.data(),true));
    auto ticks=[](float value,float step){unsigned n=0;while(value> -1.f&&n<100){value=(std::max)(-1.f,value-step);++n;}return n;};
    assert(ticks(0,.0625f)==16&&ticks(0,.5f)==2);
    unsigned rejected=0;
    auto reject=[&](){auto copy=root;assert(!nativeMapTransition::shorten(root.data(),true));assert(root==copy);++rejected;};
    for(unsigned offset:{8u,0x10u,0x18u,0x64u}){seed();root[offset]=0;reject();}
    seed();root[0x68]=1;reject();seed();root[0x1a]=1;reject();
    for(float value:{-2.f,.1f,std::numeric_limits<float>::quiet_NaN()}){seed();put(root.data(),0x58,value);reject();}
    for(float value:{0.f,1.f,std::numeric_limits<float>::quiet_NaN()}){seed();put(root.data(),0x5c,value);reject();}
    for(float value:{0.f,-.1f,.5f,1.f,std::numeric_limits<float>::infinity()}){seed();put(root.data(),0x60,value);reject();}
    seed();before=root;assert(!nativeMapTransition::shorten(root.data(),false)&&root==before);assert(!nativeMapTransition::shorten(nullptr,true));
    // Partial fades speed up, while already-faster transitions remain untouched.
    seed();put(root.data(),0x58,-.75f);assert(nativeMapTransition::shorten(root.data(),true));assert(ticks(-.75f,.5f)==1);
    printf("PASS: retail byte guards, %u rejected states unchanged, only fade step modified; modeled full fade 16 -> 2 native updates. No game launched.\n",rejected);
}
