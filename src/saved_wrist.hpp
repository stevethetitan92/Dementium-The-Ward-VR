#pragma once
#include "psx_asset_pack.hpp"
#include "wristband_mesh.hpp"
#include <DirectXMath.h>
namespace savedWrist {
inline std::vector<wristband::Vertex> geometry;
inline std::vector<unsigned char> texture;
inline DirectX::XMFLOAT4X4 sourceToSaved{};
inline bool ready{};
inline bool load(const std::filesystem::path& folder){
    std::vector<unsigned char> mesh,image,matrix;
    if(!psxAssets::read(folder/L"SavedWrist.mesh",mesh,4*1024*1024)||
       !psxAssets::read(folder/L"SavedWristAlbedo.png",image,16*1024*1024)||
       !psxAssets::read(folder/L"SavedWrist.transform",matrix,64)||matrix.size()!=64||
       mesh.empty()||mesh.size()%sizeof(wristband::Vertex)||mesh.size()/sizeof(wristband::Vertex)%3)return false;
    constexpr unsigned char signature[]={137,80,78,71,13,10,26,10};
    if(image.size()<33||std::memcmp(image.data(),signature,8)||std::memcmp(image.data()+12,"IHDR",4)||
       psxAssets::big32(image.data()+16)!=3072||psxAssets::big32(image.data()+20)!=768)return false;
    std::vector<wristband::Vertex> candidate(mesh.size()/sizeof(wristband::Vertex));
    std::memcpy(candidate.data(),mesh.data(),mesh.size());
    for(const auto& v:candidate){
        const float* p=&v.x;for(unsigned i=0;i<12;i++)if(!std::isfinite(p[i]))return false;
        if(std::fabs(v.x)>.3f||std::fabs(v.y)>.3f||std::fabs(v.z)>.3f)return false;
        const float length=v.nx*v.nx+v.ny*v.ny+v.nz*v.nz;
        if(length<.9f||length>1.1f||(v.print!=-1&&v.print!=0&&v.print!=2))return false;
    }
    DirectX::XMFLOAT4X4 transform;std::memcpy(&transform,matrix.data(),64);
    const float* values=&transform._11;for(unsigned i=0;i<16;i++)if(!std::isfinite(values[i])||std::fabs(values[i])>10)return false;
    // A projective matrix would move the live readout differently from the band.
    if(std::fabs(transform._14)>1e-6f||std::fabs(transform._24)>1e-6f||
       std::fabs(transform._34)>1e-6f||std::fabs(transform._44-1)>1e-6f)return false;
    const auto m=DirectX::XMLoadFloat4x4(&transform);
    const float determinant=DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(m));
    if(determinant<.01f||determinant>10)return false;
    geometry=std::move(candidate);texture=std::move(image);sourceToSaved=transform;ready=true;return true;
}
inline void transformStatus(std::vector<wristband::Vertex>& vertices){
    using namespace DirectX;
    const auto matrix=XMLoadFloat4x4(&sourceToSaved);
    const auto normals=XMMatrixTranspose(XMMatrixInverse(nullptr,matrix));
    for(auto& v:vertices){
        XMFLOAT3 p{v.x,v.y,v.z},n{v.nx,v.ny,v.nz};
        XMStoreFloat3(&p,XMVector3TransformCoord(XMLoadFloat3(&p),matrix));
        XMStoreFloat3(&n,XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&n),normals)));
        v.x=p.x;v.y=p.y;v.z=p.z;v.nx=n.x;v.ny=n.y;v.nz=n.z;
    }
}
}
