#pragma once
#include "selected_reload_profile.hpp"
#include "selected_frames.hpp"
namespace selectedSupportRestore {
inline bool applied{};
inline bool decode(const std::vector<unsigned char>& bytes,const std::vector<unsigned char>& geometry,
                   const std::vector<unsigned char>& frames,psxAssets::Pack& pack){
    using namespace DirectX;
    if(bytes.size()<32)return false;
    unsigned h[2],section,count;std::uint64_t hashes[2];
    std::memcpy(h,bytes.data(),8);std::memcpy(hashes,bytes.data()+8,16);
    std::memcpy(&section,bytes.data()+24,4);std::memcpy(&count,bytes.data()+28,4);
    if(h[0]!=0x53525044||h[1]!=1||hashes[0]!=selectedReload::fingerprint(geometry)||
       hashes[1]!=selectedReload::fingerprint(frames)||section>=pack.sections.size())return false;
    const auto s=pack.sections[section];
    if(s.weapon!=3||s.kind!=psxAssets::LeftSupport||s.count!=count||!count||bytes.size()!=32+std::size_t(count)*12)return false;
    std::vector<XMFLOAT3> positions(count);std::memcpy(positions.data(),bytes.data()+32,count*12);
    auto& old=pack.vertices[s.first].value;
    const XMFLOAT3 delta{positions[0].x-old[0],positions[0].y-old[1],positions[0].z-old[2]};
    if(!std::isfinite(delta.x)||!std::isfinite(delta.y)||!std::isfinite(delta.z)||
       std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z)>.25f)return false;
    for(unsigned i=0;i<count;++i){const auto& v=pack.vertices[s.first+i].value;const float* p=&positions[i].x;
        const float d[]={delta.x,delta.y,delta.z};
        for(unsigned j=0;j<3;++j)if(!std::isfinite(p[j])||std::fabs(p[j]-v[j]-d[j])>1e-6f)return false;}
    auto frame=selectedFrames::frames[3];
    XMStoreFloat4x4(&frame.wristToEditor,XMLoadFloat4x4(&frame.wristToEditor)*XMMatrixTranslation(delta.x,delta.y,delta.z));
    XMStoreFloat4x4(&frame.editorToFree,XMMatrixTranslation(-delta.x,-delta.y,-delta.z)*XMLoadFloat4x4(&frame.editorToFree));
    XMFLOAT3 contact;XMStoreFloat3(&contact,XMVector3TransformNormal(XMLoadFloat3(&delta),XMLoadFloat4x4(&frame.editorToAim)));
    frame.contact.x+=contact.x;frame.contact.y+=contact.y;frame.contact.z+=contact.z;
    if(!selectedFrames::rigid(frame.wristToEditor)||!selectedFrames::rigid(frame.editorToFree))return false;
    for(unsigned i=0;i<count;++i)std::memcpy(pack.vertices[s.first+i].value,&positions[i],12);
    selectedFrames::frames[3]=frame;return true;
}
inline bool load(const std::filesystem::path& folder,psxAssets::Pack& pack){
    applied=false;std::vector<unsigned char>b,g,f;
    return applied=psxAssets::read(folder/L"SelectedSupportRestore.bin",b,1024*1024)&&
        psxAssets::read(folder/L"SelectedGeometry.psx",g,64*1024*1024)&&
        psxAssets::read(folder/L"SelectedFrames.bin",f,4096)&&decode(b,g,f,pack);
}
}
