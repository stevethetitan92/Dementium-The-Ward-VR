#pragma once
#include <Windows.h>
#include <array>
#include <cstdint>
#include <cstring>
namespace ammoLimits {
// Native inventory order: revolver, pistol, M16, shotgun, hunting rifle.
inline constexpr std::array<unsigned,5> capacities{6,15,30,8,5};
inline constexpr unsigned limit(unsigned i){return i<5?capacities[i]*3:0;}
inline constexpr unsigned tables[]={0x30c3e0,0x30c748,0x30c9f8,0x30d3e8,0x30d548};
inline constexpr unsigned setters[]={0x8a970,0x8b480,0x8bfa0,0x8f830,0x90510};
inline constexpr unsigned getters[]={0x8a430,0x8a430,0x8af90,0x8f320,0x8f320};
inline constexpr unsigned reserves[]={0x396345,0x396343,0x396349,0x396347,0x39634b};
inline constexpr unsigned oldLimits[]={24,24,100,16,16};
inline bool installed{};inline std::uintptr_t image{};
template<unsigned I> int maximum(void*){return limit(I);}
inline bool verify(const unsigned char* base){
 for(unsigned i=0;i<5;++i){
  const auto p=base+setters[i];const unsigned char middle[]={0x0f,0xb6,0xc2,0x3b,0xd1,0x0f,0x46,0xc8,0x88,0x0d};
  unsigned old{};std::int32_t relative{};std::memcpy(&old,p+1,4);std::memcpy(&relative,p+15,4);
  if(p[0]!=0xb9||old!=oldLimits[i]||std::memcmp(p+5,middle,sizeof middle)||p[19]!=0xc3||setters[i]+19+relative!=reserves[i])return false;
  std::uintptr_t fn{};std::memcpy(&fn,base+tables[i]+0x98,8);if(fn!=reinterpret_cast<std::uintptr_t>(base)+getters[i])return false;
  auto g=base+getters[i];std::memcpy(&old,g+1,4);if(g[0]!=0xb8||g[5]!=0xc3||old!=oldLimits[i])return false;
 }return true;
}
inline bool install(std::uintptr_t base){
 if(installed)return image==base;if(!base||!verify(reinterpret_cast<unsigned char*>(base)))return false;
 // Acquire every page before modifying anything. Shared native getters must
 // remain untouched: pistol/revolver and shotgun/rifle need different limits.
 struct Edit{void* at;SIZE_T size;DWORD protection;};std::array<Edit,10> edits{};unsigned acquired=0;
 for(unsigned i=0;i<5;++i){edits[i*2]={reinterpret_cast<void*>(base+setters[i]+1),4,0};edits[i*2+1]={reinterpret_cast<void*>(base+tables[i]+0x98),8,0};}
 for(auto& e:edits){if(!VirtualProtect(e.at,e.size,PAGE_EXECUTE_READWRITE,&e.protection)){while(acquired){auto& prior=edits[--acquired];DWORD ignored;VirtualProtect(prior.at,prior.size,prior.protection,&ignored);}return false;}++acquired;}
 using Get=int(*)(void*);Get functions[]={maximum<0>,maximum<1>,maximum<2>,maximum<3>,maximum<4>};
 for(unsigned i=0;i<5;++i){auto cap=limit(i);std::memcpy(edits[i*2].at,&cap,4);std::memcpy(edits[i*2+1].at,&functions[i],8);}
 while(acquired){auto& e=edits[--acquired];DWORD ignored;VirtualProtect(e.at,e.size,e.protection,&ignored);}
 FlushInstructionCache(GetCurrentProcess(),nullptr,0);image=base;installed=true;return true;
}
// The existing player-update hook owns these native bytes. This clamps old
// saves on load; it never grants ammunition or edits a save file directly.
inline void clamp(void* player){
 if(!installed||player!=*reinterpret_cast<void**>(image+0x3a5340))return;
 for(unsigned i=0;i<5;++i){auto& value=*reinterpret_cast<unsigned char*>(image+reserves[i]);if(value>limit(i))value=static_cast<unsigned char>(limit(i));}
}
}
