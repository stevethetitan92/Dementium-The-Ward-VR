#include "../src/manual_reload_native.hpp"
#include <cstdio>
#include <stdexcept>
#include <limits>
#include <fstream>
#include <vector>
void check(bool b){if(!b)throw std::runtime_error("reload validation failed");}
int main(){try{
 using namespace manualReload;
 Transfer t;check(transfer(2,3,6,t)&&t.loaded==5&&t.reserve==0&&t.amount==3);
 check(transfer(0,20,6,t)&&t.loaded==6&&t.reserve==14);
 check(!transfer(6,20,6,t)&&!transfer(-1,20,6,t)&&!transfer(7,20,6,t));
 auto pending=[](){Pistol p;Input i;i.tracked=i.gameplay=i.pistol=true;p.update(i);
   i.releaseMagazine=true;p.update(i);i.releaseMagazine=false;i.ammoPouchDistance=0;i.leftGrip=true;p.update(i);
   i.magazineDistance=0;i.leftGrip=false;p.update(i);i.slideDistance=0;i.leftGrip=true;p.update(i);
   i.slidePull=.05f;p.update(i);i.leftGrip=false;p.update(i);return p;};
 for(int loaded=0;loaded<=15;loaded++)for(int reserve=0;reserve<=24;reserve++){
   auto p=pending();int a=loaded,b=reserve,calls=0;
   auto apply=[&]{++calls;Transfer next;check(transfer(a,b,15,next));a=next.loaded;b=next.reserve;return true;};
   check(completeReload(p,a,b,15,apply));check(!p.blocksFire()&&a+b==loaded+reserve&&a<=15&&b>=0);
   check(calls==((loaded<15&&reserve>0)?1:0));
   check(!completeReload(p,a,b,15,apply));check(calls<=1);
 }
 {auto p=pending();check(!completeReload(p,1,20,15,[]{return false;}));check(p.stage()==Stage::Inserted);}
 for(int interruption=0;interruption<3;interruption++){
    auto cancelled=pending();auto old=cancelled.pendingToken();check(old!=0);
    Input lost;lost.tracked=lost.gameplay=lost.pistol=true;
    if(interruption==0)lost.tracked=false;
    if(interruption==1)lost.magazineDistance=std::numeric_limits<float>::quiet_NaN();
    if(interruption==2)lost.slideDistance=-1;
    cancelled.update(lost);check(!cancelled.pendingToken()&&cancelled.stage()==Stage::Inserted);
    check(!cancelled.acknowledge(old,true));unsigned calls=0;
    check(!completeReload(cancelled,0,24,15,[&]{++calls;return true;})&&calls==0);
 }
 Pistol p;Input i;i.tracked=i.gameplay=i.pistol=true;p.update(i);
 i.releaseMagazine=true;p.update(i);check(p.blocksFire()&&p.stage()==Stage::EmptyWell);
 i.releaseMagazine=false;i.leftGrip=true;i.ammoPouchDistance=.1f;p.update(i);check(p.stage()==Stage::MagazineHeld);
 i.leftGrip=false;i.magazineDistance=.2f;p.update(i);check(p.stage()==Stage::EmptyWell);
 i.leftGrip=true;p.update(i);i.leftGrip=false;i.magazineDistance=.05f;p.update(i);check(p.stage()==Stage::Inserted);
 i.leftGrip=true;i.slideDistance=.04f;p.update(i);i.leftGrip=false;i.slidePull=.02f;p.update(i);check(p.stage()==Stage::Inserted);
 i.leftGrip=true;p.update(i);i.slidePull=.05f;p.update(i);
 i.gameplay=false;i.leftGrip=false;p.update(i);check(p.pendingToken()==0&&p.blocksFire());
 i.gameplay=true;p.update(i); // Rearm, without completing on a stale release.
 check(p.stage()==Stage::Inserted);i.leftGrip=true;p.update(i);p.update(i);i.leftGrip=false;p.update(i);
 auto request=p.pendingToken();check(request!=0);check(!p.acknowledge(request+1,true));
 check(p.acknowledge(request,false)&&p.stage()==Stage::Inserted);
 i.leftGrip=true;p.update(i);p.update(i);i.leftGrip=false;p.update(i);request=p.pendingToken();
 check(p.acknowledge(request,true)&&!p.blocksFire());check(!p.acknowledge(request,true));
 // Invalid tracking data cannot eject a magazine.
 i.magazineDistance=std::numeric_limits<float>::quiet_NaN();i.releaseMagazine=true;p.update(i);check(!p.blocksFire());
 // Validate adapter against a non-executable image of the installed EXE.
 std::ifstream f("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dementium - The Ward\\Dementium.exe",std::ios::binary);
 std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),{});check(bytes.size()>4096);
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
 auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data()+dos->e_lfanew);
 std::vector<unsigned char> image(nt->OptionalHeader.SizeOfImage);
 auto sections=IMAGE_FIRST_SECTION(nt);
 for(unsigned n=0;n<nt->FileHeader.NumberOfSections;n++){
   const auto& s=sections[n];check(s.PointerToRawData+s.SizeOfRawData<=bytes.size());
   check(s.VirtualAddress+s.SizeOfRawData<=image.size());
   std::memcpy(image.data()+s.VirtualAddress,bytes.data()+s.PointerToRawData,s.SizeOfRawData);
 }
 // Relocate only the vtable entries inspected by verify, preserving actual
 // executable values; no game instructions are executed by this test.
 for(auto offset:{0x70,0x78,0x80,0x88,0x90}){
   auto slot=reinterpret_cast<std::uintptr_t*>(image.data()+0x30c748+offset);
   *slot=*slot-nt->OptionalHeader.ImageBase+reinterpret_cast<std::uintptr_t>(image.data());
 }
 NativePistol adapter;check(adapter.verify(reinterpret_cast<std::uintptr_t>(image.data())));
 image[0x91db0]^=1;check(!adapter.verify(reinterpret_cast<std::uintptr_t>(image.data())));
 std::puts("manual reload: transaction, spatial gates, interruptions and ammo conservation passed");return 0;
 }catch(const std::exception& e){std::puts(e.what());return 1;}}
