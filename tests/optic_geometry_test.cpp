#include "../src/optic_geometry.hpp"
#include "../src/optic_lens_mesh.hpp"
#include <cassert>
#include <limits>
#include <cstdio>
int main(){
 static_assert(opticLensMesh::count==324);
 float nearZ=-1,farZ=1;
 for(const auto& v:opticLensMesh::vertices){
  assert(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z));
  assert(v.u*v.u+v.v*v.v<=1.00001f);
  assert(std::fabs(v.x/.0155f-v.u)<1e-6f&&std::fabs(v.y/.0155f-v.v)<1e-6f);
  nearZ=std::fmax(nearZ,v.z);farZ=std::fmin(farZ,v.z);
 }
 assert(nearZ-farZ>.004f&&nearZ<0); // Curved, recessed authored glass, not a flat overlay.
 using namespace opticGeometry;
 auto normal=aperture({0,0,.10f},.017f,3);
 assert(normal.visible&&normal.transmission>.99f);
 assert(std::fabs(std::tan(normal.halfFov)*3-.17f)<1e-6f);
 auto edge=aperture({.02f,0,.10f},.017f,3);
 assert(edge.visible&&edge.transmission==normal.transmission);
 assert(aperture({.03f,0,.10f},.017f,3).visible);
 assert(!aperture({0,0,-.10f},.017f,3).visible);
 assert(aperture({0,0,.4f},.017f,3).visible);
 assert(aperture({.3f,.2f,.7f},.017f,3).transmission==1);
 assert(aperture({0,0,.06f},.017f,3).halfFov==normal.halfFov);
 assert(aperture({0,0,.7f},.017f,3).halfFov==normal.halfFov);
 assert(!aperture({std::numeric_limits<float>::quiet_NaN(),0,.1f},.017f,3).visible);
 assert(!aperture({0,0,.1f},-.017f,3).visible);
 assert(!aperture({0,0,.1f},.017f,9).visible);
 puts("Persistent lens image at carrying/aiming distances, magnification and invalid-pose rejection passed");
}
