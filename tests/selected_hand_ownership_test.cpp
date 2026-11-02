#include "../src/selected_hand_ownership.hpp"
#include <cstdio>
int main(){
 for(unsigned bits=0;bits<32;++bits){
  bool right=bits&1,support=bits&2,acquired=bits&4,grip=bits&8,reload=bits&16;
  bool expected=right&&support&&acquired&&grip&&!reload;
  if(selectedHands::attachSupport(right,support,acquired,grip,reload)!=expected)return 1;
 }
 puts("PASS: reload hand cannot be captured by support attachment; ordinary support gates preserved");
}
