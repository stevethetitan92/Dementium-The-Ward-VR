#pragma once
#include "psx_asset_pack.hpp"
#include <algorithm>
#include <limits>

namespace selectedWristFade {
// Display-only material encoding. Keep fitted geometry, UVs, collision and
// morph topology unchanged. Six millimetres stays below the wrist HUD edge.
inline constexpr float width=.006f;
template<class Emit> inline void apply(const psxAssets::Pack& pack,Emit emit){
 for(const auto& section:pack.sections){
  if(section.kind==psxAssets::Weapon)continue;
  float normal[3]{};unsigned caps=0;bool supported=true;
  for(unsigned j=0;j<section.count;++j){const auto* p=pack.vertices[section.first+j].value;
   supported&=p[11]>=0&&p[11]<9&&p[11]==std::floor(p[11]);
   if(p[11]==4){for(unsigned k=0;k<3;++k)normal[k]+=p[k+3];++caps;}
  }
  const float length=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
  if(!supported||!caps||length<1e-6f)continue;
  for(auto& n:normal)n/=length;
  auto dot=[&](const float* p){return p[0]*normal[0]+p[1]*normal[1]+p[2]*normal[2];};
  float rim=-std::numeric_limits<float>::infinity();
  for(unsigned j=0;j<section.count;++j){const auto* p=pack.vertices[section.first+j].value;if(p[11]==4)rim=(std::max)(rim,dot(p));}
  for(unsigned j=0;j<section.count;++j){const auto* p=pack.vertices[section.first+j].value;
   const float coverage=p[11]==4?0.f:std::clamp((rim-dot(p))/width,0.f,1.f);
   emit(section.first+j,32.f+p[11]+coverage*.25f);
  }
 }
}
}
