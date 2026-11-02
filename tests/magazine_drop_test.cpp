#include "../src/magazine_drop.hpp"
#include <cassert>
#include <cstdio>
using namespace DirectX;
bool frame(XMMATRIX& m,std::uintptr_t& scene){m=XMMatrixIdentity();scene=1;return true;}
int main(){
 arsenalProps::worldFrame=frame;
 for(unsigned weapon:{1u,2u,4u}){
 magazineDrop::known.fill(false);magazineDrop::objects.clear();magazineDrop::last=0;
 magazineExit::travel[weapon]={0,-.1f,0};
 std::vector<unsigned> parts{3};
 magazineDrop::update(weapon,false,XMMatrixIdentity(),{},parts,100,1,true);
 magazineDrop::update(weapon,true,XMMatrixIdentity(),{},parts,110,1,true);
 magazineDrop::update(weapon,true,XMMatrixTranslation(2,3,0),{},parts,120,1,true);
 unsigned draws=0;
 magazineDrop::draw(155,[&](unsigned id,const XMMATRIX& m){++draws;assert(id==3);assert(std::fabs(XMVectorGetX(m.r[3])-2)<1e-5f);assert(std::fabs(XMVectorGetY(m.r[3])-2.975f)<1e-5f);});
 assert(draws==1);
 magazineDrop::update(weapon,true,XMMatrixTranslation(2,3,0),{},parts,200,1,true);
 // Once clear, moving the gun again cannot drag the falling magazine.
 magazineDrop::update(weapon,true,XMMatrixTranslation(8,9,0),{},parts,310,1,true);
 magazineDrop::draw(310,[&](unsigned,const XMMATRIX& m){assert(std::fabs(XMVectorGetX(m.r[3])-2)<1e-5f);assert(XMVectorGetY(m.r[3])<2.9f);});
 assert(magazineDrop::objects.size()==1&&magazineDrop::objects[0].surface);
 assert(std::fabs(magazineDrop::objects[0].surface->x-2)<1e-5f);
 magazineDrop::draw(9000,[](unsigned,const XMMATRIX&){assert(false);});assert(magazineDrop::objects.empty());
 }
 puts("PASS: all three magazine-fed weapons: magazine follows moving well until clear, then world-fixed gravity; floor query uses release location");
}
