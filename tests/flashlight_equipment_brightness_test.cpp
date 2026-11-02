#include "../src/head_lamp.hpp"
#include <array>
#include <cassert>
#include <cstdio>
int main(){
 std::array<unsigned char,0x158> entity{};headLamp::base=0x10000000;
 *reinterpret_cast<std::uintptr_t*>(entity.data())=headLamp::base+0x30cdd0;
 auto flags=reinterpret_cast<unsigned*>(entity.data()+0x44);
 auto level=reinterpret_cast<unsigned*>(entity.data()+0x154);
 for(unsigned high:{0u,0x2120u,0xffffffe0u})for(unsigned low=0;low<32;++low){
  *level=high|low;*flags=0x40c;headLamp::lightEnabled=false;
  assert(headLamp::forceOn(entity.data()));assert(*flags==0x42c);assert(*level==(high|31));
  assert(!headLamp::lightEnabled);auto count=headLamp::brightnessRestored.load();
  assert(headLamp::forceOn(entity.data())&&count==headLamp::brightnessRestored);
 }
 *reinterpret_cast<std::uintptr_t*>(entity.data())=headLamp::base+0x30d1a8;
 const auto before=entity;assert(!headLamp::forceOn(entity.data())&&entity==before);
 assert(!headLamp::forceOn(nullptr));
 puts("PASS: 96 lamp levels restore only five brightness bits; other flags/timers and VR off state preserved; non-lamp rejected.");
}
