#include "../src/support_transition.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){using namespace DirectX;supportTransition::Blend b;
 auto rest=XMMatrixIdentity(),support=XMMatrixRotationZ(.5f)*XMMatrixTranslation(.1f,0,0);
 b.update(rest,false,2,100,true,false);
 auto first=b.update(support,true,2,110,true,false);assert(std::fabs(XMVectorGetX(first.r[3]))<1e-5);
 auto middle=b.update(support,true,2,160,true,false);assert(std::fabs(XMVectorGetX(middle.r[3])-.05f)<1e-5);
 auto tracked=b.update(support*XMMatrixTranslation(0,0,.1f),true,2,160,true,false);
 assert(std::fabs(XMVectorGetZ(tracked.r[3])-.1f)<1e-5); // no positional filtering of tracking
 auto end=b.update(support,true,2,210,true,false);assert(std::fabs(XMVectorGetX(end.r[3])-.1f)<1e-5);
 auto reload=b.update(rest,false,2,220,true,true);assert(XMVectorGetX(reload.r[3])==0);
 auto jump=b.update(XMMatrixTranslation(1,0,0),true,2,230,true,false);assert(XMVectorGetX(jump.r[3])==1);
 b.update(rest,false,2,240,false,false);
 assert(XMVectorGetX(b.update(support,true,2,250,true,false).r[3])==.1f);
 puts("PASS: smooth grip endpoints, immediate tracked motion, reload ownership and reacquisition resets");
}
