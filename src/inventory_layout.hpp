#pragma once
#include <array>
#include <cmath>
#include <limits>
namespace inventoryLayout {
// Design coordinates shared by painting and directional selection. Existing
// weapon/pill identities remain stable when their visual positions change.
struct Rect { float x,y,w,h; };
inline constexpr float width=1000,height=900;
inline constexpr Rect map{170,80,570,500};
inline constexpr unsigned pill=8,mapSlot=9,keyBegin=10,keyCount=14,count=24;
inline constexpr std::array<Rect,count> slots{{
 {30,620,220,110},{270,620,220,110},{510,620,220,110},{750,620,220,110},
 {30,750,220,110},{270,750,220,110},{510,750,220,110},{750,750,220,110},
 {30,80,120,150},map,
 {760,80,100,65},{880,80,100,65},
 {760,152,100,65},{880,152,100,65},
 {760,224,100,65},{880,224,100,65},
 {760,296,100,65},{880,296,100,65},
 {760,368,100,65},{880,368,100,65},
 {760,440,100,65},{880,440,100,65},
 {760,512,100,65},{880,512,100,65}
}};
enum class Direction {left,right,up,down};
inline unsigned neighbor(unsigned current,Direction direction){
 if(current>=count)return 0;
 const auto a=slots[current];bool horizontal=direction==Direction::left||direction==Direction::right;
 const float sign=(direction==Direction::left||direction==Direction::up)?-1.f:1.f;
 unsigned best=current;float score=(std::numeric_limits<float>::max)();
 for(unsigned i=0;i<count;i++){
  if(i==current)continue;const auto b=slots[i];
  float dx=b.x+b.w*.5f-a.x-a.w*.5f,dy=b.y+b.h*.5f-a.y-a.h*.5f;
  float forward=sign*(horizontal?dx:dy);if(forward<=1.f)continue;
  // Favor the same row/column. Rectangle overlap lets the large map remain
  // reachable from pills, key items, and every weapon column.
  float perpendicular=horizontal?std::fabs(dy):std::fabs(dx);
  float span=horizontal?(a.h+b.h)*.5f:(a.w+b.w)*.5f;
  float gap=perpendicular>span?perpendicular-span:0.f;
  if(gap>0.f)continue;
  float candidate=forward+perpendicular*.15f;
  if(candidate<score){score=candidate;best=i;}
 }
 return best;
}
}
