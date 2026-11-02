#pragma once
#include "wristband_mesh.hpp"
#include "arsenal_runtime.hpp"
#include <filesystem>
#include <fstream>
#include <atomic>
namespace huntingRifle {
using namespace DirectX;
inline std::vector<wristband::Vertex> geometry;
inline unsigned first[8]{},count[8]{};
inline bool ready{},visible{},flash{},enabled=true,savedFitModel{};inline std::atomic<bool> gpuReady{};inline std::atomic<std::uint64_t> renderedAt{};
inline XMMATRIX pose,part[8];inline bool partVisible[8]{};
inline bool load(const std::filesystem::path& file){
    ready=false;savedFitModel=false;geometry.clear();std::ifstream f(file,std::ios::binary|std::ios::ate);if(!f)return false;
    auto size=f.tellg();if(size<40||size>4*1024*1024)return false;f.seekg(0);unsigned header[10]{};f.read(reinterpret_cast<char*>(header),40);
    if(!f||header[0]!=0x4c465244||(header[1]!=1&&header[1]!=2))return false;unsigned total=0;
    for(unsigned i=0;i<8;i++){if((!header[i+2]&&header[1]==1)||header[i+2]%3||header[i+2]>50000)return false;first[i]=total;count[i]=header[i+2];total+=count[i];}
    if(!total)return false;
    if(40+std::size_t(total)*sizeof(wristband::Vertex)!=static_cast<std::size_t>(size))return false;
    std::vector<wristband::Vertex> candidate(total);f.read(reinterpret_cast<char*>(candidate.data()),total*sizeof(wristband::Vertex));if(!f)return false;
    for(const auto& v:candidate){const float* data=&v.x;for(unsigned n=0;n<12;n++)if(!std::isfinite(data[n]))return false;if(std::fabs(v.x)>2||std::fabs(v.y)>2||std::fabs(v.z)>2)return false;}
    geometry=std::move(candidate);savedFitModel=header[1]==2;return ready=true;
}
inline bool eligible(const manualReload::Hands& h,std::uint64_t now){return ready&&enabled&&h.session&&h.gameplay&&h.weapon==4&&h.gunTracked&&now>=h.time&&now-h.time<100&&motionTracking::nearHead(h.gun,h.head)&&motionTracking::validPose(h.gun)&&sniperScope::factor()<=1.01f;}
inline void prepare(const manualReload::Hands& h,std::uint64_t now){
    visible=eligible(h,now);auto shot=arsenal::shotTime[3].load();flash=shot&&now>=shot&&now-shot<45;if(!visible)return;
    if(savedFitModel){
        pose=motionTracking::rotation(h.gun.orientation)*XMMatrixTranslation(h.gun.position.x,h.gun.position.y,h.gun.position.z);
        for(unsigned i=0;i<8;i++){part[i]=XMMatrixIdentity();partVisible[i]=count[i]!=0;}
        // Existing bolt animation belongs to the old model's coordinate system.
        flash=false;return;
    }
    // Canonical body +X points down the OpenXR aim ray; +Z is anatomical down.
    auto basis=XMMatrixSet(0,0,-1,0,1,0,0,0,0,-1,0,0,0,0,0,1);
    constexpr XrVector3f grip{-.205f,0,.015f};
    pose=XMMatrixTranslation(-grip.x,-grip.y,-grip.z)*basis*motionTracking::rotation(h.gun.orientation)*XMMatrixTranslation(h.gun.position.x,h.gun.position.y,h.gun.position.z);
    auto phase=arsenal::reloads[3].state();float travel=arsenal::reloads[3].boltOffset();
    if(phase==arsenal::Phase::Charging)travel=std::clamp(arsenal::visuals[3].slidePull,0.f,.08f);
    const bool unlocked=phase!=arsenal::Phase::Ready&&phase!=arsenal::Phase::Opening;
    for(unsigned i=0;i<8;i++){part[i]=XMMatrixIdentity();partVisible[i]=i<3;}
    // Bolt rotates about the bore, then travels rearward. Never alter native ammo here.
    constexpr XrVector3f bolt{-.14557668f,0,-.07347436f};
    part[2]=XMMatrixTranslation(-bolt.x,-bolt.y,-bolt.z)*XMMatrixRotationX(unlocked?1.04719755f:0)*XMMatrixTranslation(bolt.x-travel,bolt.y,bolt.z);
    constexpr XrVector3f trigger{-.17410256f,0,-.01650736f};
    part[1]=XMMatrixTranslation(-trigger.x,0,-trigger.z)*XMMatrixRotationY(h.trigger>.15f?-.12f:0)*XMMatrixTranslation(trigger.x,0,trigger.z);
}
inline bool replaceNative(unsigned weapon){
    if(weapon!=4||!ready||!gpuReady.load())return false;
    auto now=GetTickCount64(),last=renderedAt.load();if(now<last||now-last>=100)return false;
    manualReload::Hands h;{std::lock_guard<std::mutex> lock(manualReload::mutex);h=manualReload::hands;}
    return eligible(h,now);
}
}
