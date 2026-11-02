#include "../src/holster_motion.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using namespace DirectX;holsterMotion::Blend b;
 auto hand=XMMatrixTranslation(.1f,0,0),slot=XMMatrixIdentity();
 b.update(hand,true,1000);
 auto first=b.update(slot,false,1010);assert(std::fabs(XMVectorGetX(first.r[3])-.1f)<1e-6f);
 auto half=b.update(slot,false,1080);assert(std::fabs(XMVectorGetX(half.r[3])-.05f)<1e-6f);
 auto end=b.update(slot,false,1150);assert(std::fabs(XMVectorGetX(end.r[3]))<1e-6f);
 auto draw=b.update(hand,true,1160);assert(std::fabs(XMVectorGetX(draw.r[3]))<1e-6f);
 // A missing tracking interval and distant replacement must snap safely.
 auto gap=b.update(hand,true,1500);assert(std::fabs(XMVectorGetX(gap.r[3])-.1f)<1e-6f);
 auto jump=b.update(XMMatrixTranslation(3,0,0),false,1510);assert(std::fabs(XMVectorGetX(jump.r[3])-3)<1e-6f);
 puts("Hip draw/stow continuity, eased completion, tracking-gap and long-jump rejection passed");
}
