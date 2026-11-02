#include "../src/selected_hunting_flash.hpp"
#include <cstdio>
#include <stdexcept>
void check(bool v){if(!v)throw std::runtime_error("hunting flash regression");}
int main(int argc,char** argv){try{
 if(argc!=2)return 2;psxAssets::Pack p;check(psxAssets::load(argv[1],p));check(selectedFrames::load(argv[1]));
 auto original=p.vertices;auto sections=p.sections.size();selectedHuntingFlash::append(p);
 check(selectedHuntingFlash::section==sections&&p.vertices.size()==original.size()+18);
 check(!std::memcmp(original.data(),p.vertices.data(),original.size()*sizeof(psxAssets::Vertex)));
 auto frame=DirectX::XMLoadFloat4x4(&selectedFrames::frames[4].editorToAim);
 float low=10,high=-10;
 for(size_t i=original.size();i<p.vertices.size();++i){auto* v=p.vertices[i].value;for(unsigned j=0;j<12;++j)check(std::isfinite(v[j]));
  DirectX::XMFLOAT3 q;DirectX::XMStoreFloat3(&q,DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(v[0],v[1],v[2],1),frame));
  low=(std::min)(low,q.z);high=(std::max)(high,q.z);}
 check(high<0&&high-low>.10f&&high-low<.11f);
 check(!selectedHuntingFlash::visible(0,10)&&!selectedHuntingFlash::visible(100,99));
 check(selectedHuntingFlash::visible(100,144)&&!selectedHuntingFlash::visible(100,145));
 printf("PASS: measured hunting flash z=%.4f..%.4f, preserved source vertices, shot-only45ms gate\n",low,high);return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
