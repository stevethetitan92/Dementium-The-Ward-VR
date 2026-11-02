#include "../src/inventory_model.hpp"
#include <cassert>
#include <cstdio>
int main(){inventoryScreen::Controller c;vrControls::Input i{};i.active=true;i.l3=i.r3=true;
auto r=c.step(i,true,0,0);assert(c.menuPage()==1&&!r.toggleBlackFog&&!r.toggleHaze);
i.l3=i.r3=false;c.step(i,true,0,0);i.a=true;r=c.step(i,true,0,0);assert(r.toggleHaze&&!r.toggleBlackFog);
i.a=false;c.step(i,true,0,0);i.leftY=-1;r=c.step(i,true,0,0);assert(c.settingSelection()==1&&r.changed);
for(int k=0;k<100;k++){r=c.step(i,true,0,0);assert(c.settingSelection()==1&&!r.changed);}
i.leftY=0;c.step(i,true,0,0);i.a=true;r=c.step(i,true,0,0);assert(r.toggleBlackFog&&!r.toggleHaze&&r.captured&&r.equip==-1&&!r.use);
r=c.step(i,true,0,0);assert(!r.toggleBlackFog);
i.a=false;i.x=true;r=c.step(i,true,0,0);assert(c.menuPage()==1&&!r.changed);i.x=false;
i.rightGrip=1;r=c.step(i,true,0,0);assert(c.menuPage()==2&&r.changed);
for(int k=0;k<100;k++){r=c.step(i,true,0,0);assert(c.menuPage()==2&&!r.changed);}
i.rightGrip=0;c.step(i,true,0,0);i.rightGrip=1;c.step(i,true,0,0);assert(c.menuPage()==3);
i.leftGrip=1;c.step(i,true,0,0);assert(c.menuPage()==2);
i.leftGrip=i.rightGrip=0;c.step(i,true,0,0);i.leftGrip=1;c.step(i,true,0,0);assert(c.menuPage()==1);
i.leftGrip=0;c.step(i,true,0,0);i.leftGrip=1;c.step(i,true,0,0);assert(c.menuPage()==3);
i.leftGrip=i.rightGrip=0;c.step(i,true,0,0);i.leftGrip=i.rightGrip=1;r=c.step(i,true,0,0);assert(c.menuPage()==3&&!r.changed);
i.a=true;r=c.step(i,true,0,0);assert(c.menuPage()==3&&!r.toggleBlackFog&&!r.toggleHaze);
i.active=false;r=c.step(i,true,0,0);assert(!c.visible()&&r.closed);
inventoryScreen::Controller inv;i={};i.active=true;i.y=true;inv.step(i,true,0,0);assert(inv.menuPage()==0);
i.y=false;i.rightGrip=1;inv.step(i,true,0,0);assert(inv.menuPage()==1);
i.rightGrip=0;inv.step(i,true,0,0);i.leftGrip=1;inv.step(i,true,0,0);assert(inv.menuPage()==0);
puts("PASS: directional grips, hold suppression, independent grips, wrap, X ignored, inventory pages;  settings selection hysteresis, independent toggles, held buttons, controls page isolation and tracking loss");}

