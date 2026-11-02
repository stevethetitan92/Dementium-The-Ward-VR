#pragma once
#include "wristband_mesh.hpp"
#include "forearm_profiles.hpp"
#include "native_texture_decode.hpp"
namespace wristband {
inline constexpr float forearmStart=-.005f,forearmEnd=.042f;
inline constexpr Color handPreviewMaterial{.42f,.43f,.41f};
// Small local wrist continuation, with identical length and material on both sides.
// This is original mod geometry, not a full arm or a body/IK reconstruction.
inline std::vector<Vertex> forearmMesh(unsigned hand){
    std::vector<Vertex> mesh;if(hand>1)return mesh;
    const auto p=forearmProfiles[hand];constexpr unsigned segments=64,rings=7;
    const float zs[rings]={forearmStart,.003f,.012f,.024f,.036f,.040f,forearmEnd};
    const float scales[rings]={.97f,.98f,1.f,1.035f,1.07f,1.065f,1.02f};
    const float blend[rings]={0,0,.15f,.6f,1,1,1};
    auto vertex=[&](unsigned ring,unsigned segment){
        float a=2*pi*segment/segments,s=std::sin(a),c=std::cos(a),r=scales[ring];
        // Same visible section on both arms, expressed in their differing bone axes.
        float rx=p.rx+((hand?.021f:.032f)-p.rx)*blend[ring];
        float ry=p.ry+((hand?.032f:.021f)-p.ry)*blend[ring];
        float nx=s/rx,ny=c/ry,n=std::sqrt(nx*nx+ny*ny);
        bool skin=nativeTexture::skinPixels.size()==65536;
        return Vertex{p.x+s*rx*r,p.y+c*ry*r,zs[ring],nx/n,ny/n,-.035f,skin?1.f:handPreviewMaterial.r,skin?1.f:handPreviewMaterial.g,skin?1.f:handPreviewMaterial.b,.035f+.16f*float(segment)/segments,.42f+.5f*float(ring)/(rings-1),skin?5.f:4.f};
    };
    for(unsigned ring=0;ring<rings-1;ring++)for(unsigned i=0;i<segments;i++)quad(mesh,vertex(ring,i),vertex(ring,i+1),vertex(ring+1,i+1),vertex(ring+1,i));
    for(unsigned side=0;side<2;side++)for(unsigned i=0;i<segments;i++){
        unsigned r=side?rings-1:0;auto a=vertex(r,i),b=vertex(r,i+1),center=a;
        center.x=p.x;center.y=p.y;center.nx=center.ny=0;center.nz=side?1.f:-1.f;
        a.nx=a.ny=b.nx=b.ny=0;a.nz=b.nz=center.nz;
        if(side)mesh.insert(mesh.end(),{center,b,a});else mesh.insert(mesh.end(),{center,a,b});
    }
    return mesh;
}
}
