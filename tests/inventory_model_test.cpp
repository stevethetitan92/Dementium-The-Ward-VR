#include "../src/inventory_model.hpp"
#include <cstdio>
#include <stdexcept>
using inventoryScreen::Controller;using vrControls::Input;
static void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static void release(Controller& c,Input& in,unsigned owned=255,unsigned pills=3){in={};in.active=true;c.step(in,true,owned,pills);}
static void move(Controller& c,Input& in,float x,float y){release(c,in);in.leftX=x;in.leftY=y;c.step(in,true,255,3);}
int main(){try{
 Controller c;Input in{};in.active=true;in.y=true;in.a=true;
 auto r=c.step(in,true,255,3);require(c.visible()&&r.captured&&r.equip<0&&!r.use,"opening must capture input without equipping held A");
 r=c.step(in,true,255,3);require(c.visible()&&!r.changed,"held Y must not repeat");
 release(c,in);in.y=true;r=c.step(in,true,255,3);require(!c.visible()&&r.closed&&r.captured,"Y closes with capture on closing frame");
 release(c,in);in.y=true;c.step(in,true,255,3);release(c,in);
 in.leftX=1;c.step(in,true,255,3);require(c.selection()==1,"right selects next column");
 c.step(in,true,255,3);require(c.selection()==1,"held navigation cannot repeat");
 move(c,in,-1,0);require(c.selection()==0,"left selects previous column");
 move(c,in,-1,0);require(c.selection()==0,"left edge stays in row");
 move(c,in,0,1);require(c.selection()==9,"up reaches map");move(c,in,-1,0);require(c.selection()==8,"left from map reaches medicine slot");
 release(c,in,255,0);in.a=true;r=c.step(in,true,255,0);require(c.visible()&&!r.use,"empty pill supply cannot consume");
 release(c,in);in.a=true;r=c.step(in,true,255,3);require(r.use&&!c.visible()&&r.closed,"pill slot requests exactly one use and closes");
 r=c.step(in,true,255,3);require(!r.use,"held A cannot repeat use after closing");
 for(unsigned slot=0;slot<7;slot++){
  Controller test;release(test,in);in.y=true;test.step(in,true,255,3);
  for(unsigned row=0;row<slot/4;row++)move(test,in,0,-1);
  for(unsigned col=0;col<slot%4;col++)move(test,in,1,0);
  require(test.selection()==slot,"grid address");
  release(test,in,0);in.a=true;r=test.step(in,true,0,3);require(r.equip<0&&test.visible(),"unowned weapon must not equip");
  release(test,in);in.a=true;r=test.step(in,true,255,3);require(r.equip==inventoryScreen::weapons[slot]&&!test.visible(),"correct native weapon ID for every slot");
 }
 Controller loss;release(loss,in);in.y=true;loss.step(in,true,255,3);in.active=false;r=loss.step(in,true,255,3);require(!loss.visible()&&r.closed,"tracking/session loss closes screen");
 Controller blocked;in={};in.active=true;in.y=true;blocked.step(in,false,255,3);require(!blocked.visible(),"native menu/title screen blocks opening");
 puts("PASS: spatial reference-layout navigation, Y edges, hold suppression, all seven weapon targets, unowned/empty rejection, one-shot pill use and session loss");return 0;
}catch(const std::exception& e){puts(e.what());return 1;}}
