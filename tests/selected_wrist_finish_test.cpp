#include "../src/selected_wrist_finish.hpp"
#include "../src/selected_holding_hand.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char** argv){
 assert(argc==2);psxAssets::Pack pack;assert(psxAssets::load(argv[1],pack));assert(selectedHoldingHand::load(argv[1],pack));
 auto before=pack;unsigned expected=0;for(const auto& s:before.sections)if(s.kind!=psxAssets::Weapon){for(unsigned j=0;j<s.count;++j)if(before.vertices[s.first+j].value[11]==4){++expected;break;}}selectedWristFinish::apply(pack);assert(expected>0&&selectedWristFinish::shortened==expected);
 using namespace DirectX;
 for(size_t i=0;i<pack.sections.size();++i){const auto& a=before.sections[i];const auto& b=pack.sections[i];
  if(a.kind==psxAssets::Weapon){assert(a.count==b.count);assert(!std::memcmp(before.vertices.data()+a.first,pack.vertices.data()+b.first,a.count*sizeof(psxAssets::Vertex)));continue;}
  XMVECTOR c=XMVectorZero(),n=XMVectorZero();unsigned count=0;
  for(unsigned j=0;j<a.count;++j){auto* p=before.vertices[a.first+j].value;if(p[11]==4){c+=XMVectorSet(p[0],p[1],p[2],0);n+=XMVectorSet(p[3],p[4],p[5],0);++count;}}
  if(!count)continue;c/=float(count);n=-XMVector3Normalize(n);bool cap=false;
  for(unsigned j=0;j<b.count;++j){auto* p=pack.vertices[b.first+j].value;float depth=XMVectorGetX(XMVector3Dot(XMVectorSet(p[0],p[1],p[2],0)-c,n));assert(depth>=.07999f);cap|=std::fabs(depth-.080f)<.00001f&&(p[11]==5||p[11]==7);for(unsigned k=0;k<12;++k)assert(std::isfinite(p[k]));}
  assert(cap);
 }
 unsigned base=~0u;for(unsigned i=0;i<pack.sections.size();++i)if(i!=selectedHoldingHand::section&&pack.sections[i].weapon==2&&pack.sections[i].kind==psxAssets::LeftSupport)base=i;
 assert(base!=~0u);assert(pack.sections[base].count==pack.sections[selectedHoldingHand::section].count);
 puts("All marked wrist ends shortened equally; weapon geometry unchanged, finite closed caps, holding-hand morph counts retained");
}


