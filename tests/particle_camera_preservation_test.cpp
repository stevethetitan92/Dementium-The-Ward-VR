#include "../src/stereo.hpp"
#include <cassert>
#include <cstdio>
int main(){
 stereo::origin={};stereo::origin.orientation.w=1;
 stereo::unitsPerMeter=1;stereo::frameScopeFactor=1;
 alignas(16) float original[36]{};
 original[0]=original[5]=original[10]=1;
 original[12]=original[17]=1;original[22]=1;original[23]=.1f;original[26]=-1;
 // Non-camera particle parameters and the following allocation must survive
 // byte-for-byte, including values that would visibly change effect colour.
 for(int i=28;i<36;i++)original[i]=(i-27)*.125f;
 for(int eye=0;eye<2;eye++){
  float data[36];std::memcpy(data,original,sizeof data);
  XrView view{XR_TYPE_VIEW};view.pose.orientation.w=1;
  view.pose.position.x=eye?.032f:-.032f;
  view.fov={-.8f,.7f,.75f,-.65f};
  stereo::transformConstants(data,128,view);
  assert(std::memcmp(data+28,original+28,8*sizeof(float))==0);
  assert(std::memcmp(data,original,28*sizeof(float))!=0);
  for(int i=0;i<28;i++)assert(std::isfinite(data[i]));
 }
 // The new diagnostic must distinguish red vertex colour from white and
 // respect the actual instance, stride and colour offset, without writes.
 unsigned char vertices[64]{};vertices[36]=255;vertices[39]=128;
 const auto before=std::vector<unsigned char>(vertices,vertices+64);
 stereo::uploads[0x1000].mapped(vertices,sizeof vertices);
 stereo::ListState state{};state.vertices[2]={0x1000,64,16};
 stereo::SpriteElement color{2,4,1,DXGI_FORMAT_R8G8B8A8_UNORM};float rgba[4]{};
 assert(stereo::spriteAttribute(state,color,0,2,rgba));
 assert(rgba[0]==1&&rgba[1]==0&&rgba[2]==0&&std::fabs(rgba[3]-128.f/255)<.00001f);
 color.format=DXGI_FORMAT_B8G8R8A8_UNORM;
 assert(stereo::spriteAttribute(state,color,0,2,rgba)&&rgba[0]==0&&rgba[2]==1);
 assert(!stereo::spriteAttribute(state,color,0,4,rgba));
 color.format=DXGI_FORMAT_UNKNOWN;assert(!stereo::spriteAttribute(state,color,0,2,rgba));
 assert(std::memcmp(vertices,before.data(),64)==0);stereo::uploads.clear();
 std::puts("PASS: stereo particle camera preservation and read-only particle colour sampling with instance/offset/bounds checks");
}
