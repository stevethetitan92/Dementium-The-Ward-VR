#include "../src/mounted_light.hpp"
#include "../src/handheld_light.hpp"
#include <cstdio>
#include <cassert>
int main(int argc,char**argv){assert(argc==2);psxAssets::Pack pack;assert(psxAssets::load(argv[1],pack));assert(selectedFrames::load(argv[1]));const auto original=pack.vertices;mountedLight::append(pack);
 assert(!std::memcmp(original.data(),pack.vertices.data(),original.size()*sizeof(psxAssets::Vertex)));
 for(unsigned w=2;w<=4;++w){assert(mountedLight::eligible(w));const auto o=mountedLight::offsets[w];printf("weapon=%u mount=%f,%f,%f tape_vertices=%u\n",w,o.x,o.y,o.z,pack.sections[mountedLight::tape[w]].count);
  for(unsigned i=0;i<24;++i){using namespace DirectX;XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationRollPitchYaw(i*.1f,i*.2f,i*.3f));XrPosef gun{{q.x,q.y,q.z,q.w},{1,2,3}};
   auto lamp=mountedLight::pose(w,gun),local=motionTracking::relativePose(gun,lamp);assert(std::fabs(local.position.x-o.x)<1e-5f&&std::fabs(local.position.y-o.y)<1e-5f&&std::fabs(local.position.z-o.z)<1e-5f);
   handheldLight::publish(lamp,100,true,false);XrPosef beam;assert(!handheldLight::held&&handheldLight::read(101,beam));auto lens=motionTracking::relativePose(lamp,beam);assert(std::fabs(lens.position.z+.106f)<1e-5f);
  }
 }
 assert(!mountedLight::eligible(-1)&&!mountedLight::eligible(0)&&!mountedLight::eligible(1)&&!mountedLight::eligible(5));
 puts("PASS: three source gun mounts, tape meshes, 72 rotated lens transforms, mounted light does not claim left hand");
}
