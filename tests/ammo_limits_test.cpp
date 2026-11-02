#include "../src/ammo_limits.hpp"
#include "../src/medicine_inventory.hpp"
#include <fstream>
#include <vector>
#include <cassert>
#include <iostream>
int main(){
 std::ifstream f("C:/Program Files (x86)/Steam/steamapps/common/Dementium - The Ward/Dementium.exe",std::ios::binary);
 std::vector<char> file((std::istreambuf_iterator<char>(f)),{});assert(file.size()>4096);
 auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(file.data());auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(file.data()+dos->e_lfanew);
 auto p=static_cast<unsigned char*>(VirtualAlloc(nullptr,nt->OptionalHeader.SizeOfImage,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));assert(p);
 auto section=IMAGE_FIRST_SECTION(nt);for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)memcpy(p+section[i].VirtualAddress,file.data()+section[i].PointerToRawData,section[i].SizeOfRawData);
 using namespace ammoLimits;
 for(unsigned i=0;i<5;++i){auto& fn=*reinterpret_cast<std::uintptr_t*>(p+tables[i]+0x98);assert(fn==nt->OptionalHeader.ImageBase+getters[i]);fn=reinterpret_cast<std::uintptr_t>(p)+getters[i];}
 assert(verify(p));auto saved=p[setters[2]];p[setters[2]]=0;assert(!install(reinterpret_cast<std::uintptr_t>(p)));assert(*reinterpret_cast<unsigned*>(p+setters[0]+1)==24);p[setters[2]]=saved;
 assert(install(reinterpret_cast<std::uintptr_t>(p)));
 for(unsigned i=0;i<5;++i){
  auto get=*reinterpret_cast<int(**)(void*)>(p+tables[i]+0x98);assert(get(nullptr)==int(limit(i)));
  assert(*reinterpret_cast<unsigned*>(p+getters[i]+1)==oldLimits[i]);
  auto set=reinterpret_cast<void(*)(void*,unsigned)>(p+setters[i]);
  for(unsigned value:{0u,1u,limit(i)-1,limit(i),limit(i)+1,255u}){set(nullptr,value);assert(p[reserves[i]]==(value>limit(i)?limit(i):value));}
 }
 int player;*reinterpret_cast<void**>(p+0x3a5340)=&player;
 for(unsigned i=0;i<5;++i)p[reserves[i]]=255;
 clamp(nullptr);assert(p[reserves[0]]==255);clamp(&player);for(unsigned i=0;i<5;++i)assert(p[reserves[i]]==limit(i));
 p[reserves[0]]=0;clamp(&player);assert(p[reserves[0]]==0);
 medicine::Inventory old;assert(old.restore(3,{600,601,602}));assert(old.count()==2&&old.contains(602)&&!old.collect(603));assert(old.consume(true)&&old.collect(603));assert(!old.consume(false)&&old.count()==2);
 VirtualFree(p,0,MEM_RELEASE);std::cout<<"PASS: original native setters, independent caps, shared getter preservation, rejection/no partial patch, clamp/no ammo grant, legacy pill migration; no game process\n";
}
