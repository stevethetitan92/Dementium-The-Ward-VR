#pragma once
#include <cstdint>
namespace huntingScopeControls {
// These three native hunting paths enter the stationary screen-space scope
// control mode. Other callers (menus/cutscenes) retain native behavior.
inline bool stationary(bool requested,bool optical,std::uintptr_t caller,bool huntingTransition=false){
 return requested&&!(optical&&(huntingTransition||caller==0x90a06||caller==0x90be0||caller==0x90d98));
}
}
