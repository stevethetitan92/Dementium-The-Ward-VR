#pragma once
#include <cstdint>
namespace semiFire {
// Observe only already-routed trigger edges. Never queue a shot across a
// reload, tracking loss, menu, or weapon change.
class Edge {
 std::uint64_t serial{};bool active{};
public:
 bool update(std::uint64_t next,bool valid){
  if(!valid){serial=next;active=false;return false;}
  if(!active){serial=next;active=true;return false;}
  if(next==serial)return false;
  bool forward=next>serial;serial=next;return forward;
 }
};
}
