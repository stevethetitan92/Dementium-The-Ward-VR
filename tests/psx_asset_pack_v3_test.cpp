#include "../src/psx_asset_pack.hpp"
#include <iostream>
#include <cstdlib>
#include <limits>
using namespace psxAssets;
void require(bool value){if(!value)std::exit(1);}
int main(){
    std::vector<Section> sections;
    for(unsigned weapon=0;weapon<7;weapon++){
        sections.push_back({weapon,Weapon,unsigned(sections.size()*3),3});
        sections.push_back({weapon,RightHand,unsigned(sections.size()*3),3});
        if(weapon>=2&&weapon<=5)sections.push_back({weapon,LeftSupport,unsigned(sections.size()*3),3});
    }
    // Additional weapon section simulates a replacement with an extra part.
    const unsigned rotor=unsigned(sections.size());
    sections.push_back({5,Weapon,unsigned(sections.size()*3),3});
    std::vector<Vertex> vertices(sections.size()*3);
    for(auto& v:vertices){v.value[4]=1;v.value[6]=v.value[7]=v.value[8]=1;v.value[11]=7;}
    std::uint32_t h[]={0x58535044,3,1,unsigned(sections.size()),unsigned(vertices.size()),8192,rotor};
    float pivotAxis[]={.1f,.2f,.3f,0,1,0};
    std::vector<unsigned char> data(52+sections.size()*16+vertices.size()*48);
    std::memcpy(data.data(),h,sizeof h);std::memcpy(data.data()+28,pivotAxis,24);
    std::memcpy(data.data()+52,sections.data(),sections.size()*16);
    std::memcpy(data.data()+52+sections.size()*16,vertices.data(),vertices.size()*48);
    std::vector<unsigned char> png(33);const unsigned char signature[]={137,80,78,71,13,10,26,10};
    std::memcpy(png.data(),signature,8);png[11]=13;std::memcpy(png.data()+12,"IHDR",4);png[18]=32;png[22]=32;
    Pack pack;require(decode(data,png,pack));require(pack.rotorSection==rotor&&pack.version==3&&pack.sections.size()==19);
    const auto saved=pack.vertices.size();unsigned rejected=0;
    auto reject=[&](std::vector<unsigned char> bad){require(!decode(bad,png,pack));require(pack.vertices.size()==saved&&pack.rotorSection==rotor);++rejected;};
    auto bad=data;bad.resize(51);reject(bad);
    bad=data;std::uint32_t invalid=999;std::memcpy(bad.data()+24,&invalid,4);reject(bad);
    bad=data;invalid=1;std::memcpy(bad.data()+24,&invalid,4);reject(bad); // hand cannot be rotor
    bad=data;float zero=0;std::memcpy(bad.data()+44,&zero,4);reject(bad);
    bad=data;float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(bad.data()+28,&nan,4);reject(bad);
    bad=data;bad[12]=0;bad[13]=1;reject(bad); // inconsistent section count
    bad=data;invalid=~0u;std::memcpy(bad.data()+24,&invalid,4);require(decode(bad,png,pack));require(pack.rotorSection==~0u);
    std::cout<<"PASS: variable sections, explicit optional rotor, preserved hand identities; "<<rejected<<" malformed packages rejected\n";
}
