#pragma once
#include "forearm_mesh.hpp"
#include "arsenal_runtime.hpp"
#include "sniper_scope.hpp"
#include <fstream>
#include <filesystem>
#include <stdexcept>
namespace weaponGripHands {
using namespace DirectX;using wristband::Vertex;
struct Fit {float pitch;XrVector3f center;XrVector3f indexCorrection;};
// Body-local coordinates: barrel +X, receiver width Y, grip descends +Z.
// Separate stock-neck and pistol-grip poses; the firearm itself is never moved.
inline constexpr Fit fits[3]={
 {0,{.015f,0,.092f},{0,0,0}},
 {-.9599311f,{-.020f,0,.035f},{0,0,-.022f}},
 {-.8726646f,{-.035f,0,.035f},{-.008f,0,-.018f}}
};
struct PoseBone {XrQuaternionf q;XrVector3f p;};
struct Point {unsigned bone;XrVector3f p;float u{},v{};};
inline std::vector<Point> source;inline std::vector<Vertex> meshes[3];inline PoseBone pistol[12]{};inline bool loaded{},attempted{};
inline XrVector3f rotate(XrVector3f v,const XrQuaternionf& q){XMFLOAT3 out;XMStoreFloat3(&out,XMVector3TransformNormal(XMVectorSet(v.x,v.y,v.z,0),motionTracking::rotation(q)));return {out.x,out.y,out.z};}
inline XrVector3f add(XrVector3f a,XrVector3f b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline XrVector3f subtract(XrVector3f a,XrVector3f b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline XrVector3f bodyLocal(XrVector3f hand,unsigned bone){
    auto world=add(rotate(hand,pistol[bone].q),pistol[bone].p);auto q=pistol[5].q;q.x=-q.x;q.y=-q.y;q.z=-q.z;
    return rotate(subtract(world,pistol[5].p),q);
}
inline XrVector3f fitPoint(unsigned variant,XrVector3f point,unsigned bone){
    // Turn the original pistol grip's down axis (-Y) into the long-gun +Z axis.
    XrVector3f p{point.x,point.z,-point.y};p=subtract(p,{.015f,0,.055f});
    auto& f=fits[variant];float c=std::cos(f.pitch),s=std::sin(f.pitch);
    p={c*p.x+s*p.z,p.y,-s*p.x+c*p.z};p=add(p,f.center);
    if(bone==4)p=add(p,f.indexCorrection);return p;
}
inline unsigned u32(const std::vector<unsigned char>& b,size_t p){if(p+4>b.size())throw std::runtime_error("mesh bounds");unsigned v;std::memcpy(&v,b.data()+p,4);return v;}
inline int signedBits(unsigned n,unsigned bits){unsigned sign=1u<<(bits-1);return static_cast<int>((n^sign)-sign);}
inline bool decode(const std::vector<unsigned char>& data,const std::vector<unsigned char>& animation){try{
    if(data.size()<56||u32(data,0)!=12||animation.size()<8+12*14)return false;
    const unsigned frames=u32(animation,0);if(frames>10000||8ull+(frames+1ull)*12*14>animation.size())return false;
    for(unsigned i=0;i<12;i++){
        std::int16_t v[7];std::memcpy(v,animation.data()+8+i*14,sizeof v);
        pistol[i]={{v[0]/4096.f,v[1]/4096.f,v[2]/4096.f,v[3]/4096.f},{v[4]/4096.f,v[5]/4096.f,v[6]/4096.f}};
        if(!motionTracking::validPose({pistol[i].q,pistol[i].p}))return false;
    }
    std::vector<Point> result;
    size_t offset=u32(data,52);if(offset<56||offset>=data.size())return false;
    while(offset<data.size()){
        unsigned words=u32(data,offset);if(words>1000000)return false;size_t end=offset+4ull+words*4ull;if(end>data.size())return false;
        size_t cursor=offset+4;unsigned bone=0,primitive=0;int xyz[3]{};float u=0,v=0;std::vector<Point> stream;
        while(cursor<end){unsigned commands=u32(data,cursor);cursor+=4;
            for(unsigned k=0;k<4;k++){
                unsigned command=(commands>>(k*8))&255,n=0;
                switch(command){case 0:case 0x11:case 0x15:case 0x41:n=0;break;
                case 0x10:case 0x12:case 0x13:case 0x14:case 0x20:case 0x21:case 0x22:case 0x24:case 0x25:case 0x26:case 0x27:case 0x28:case 0x29:case 0x2a:case 0x2b:case 0x2f:case 0x30:case 0x31:case 0x32:case 0x33:case 0x40:case 0x50:case 0x60:case 0x72:n=1;break;
                case 0x23:case 0x71:n=2;break;case 0x1b:case 0x1c:case 0x70:n=3;break;
                case 0x16:case 0x18:n=16;break;case 0x17:case 0x19:n=12;break;case 0x1a:n=9;break;case 0x34:n=32;break;default:return false;}
                if(cursor+n*4ull>end)return false;unsigned a=n?u32(data,cursor):0,b=n>1?u32(data,cursor+4):0;cursor+=n*4;
                if(command==0x14){unsigned slot=a&31;if(slot>30)return false;bone=30-slot;}
                if(command==0x40){primitive=a&3;stream.clear();}
                if(command==0x22){u=signedBits(a&65535,16)/2048.f;v=signedBits(a>>16,16)/2048.f;}
                if(command==0x23){xyz[0]=signedBits(a&65535,16);xyz[1]=signedBits(a>>16,16);xyz[2]=signedBits(b&65535,16);}
                if(command==0x24)for(unsigned i=0;i<3;i++)xyz[i]=signedBits((a>>(i*10))&1023,10)*64;
                if(command>=0x25&&command<=0x27){unsigned x=command==0x27?1:0,y=command==0x25?1:2;xyz[x]=signedBits(a&65535,16);xyz[y]=signedBits(a>>16,16);}
                if(command==0x28)for(unsigned i=0;i<3;i++)xyz[i]+=signedBits((a>>(i*10))&1023,10);
                if(command>=0x23&&command<=0x28){
                    stream.push_back({bone,{xyz[0]/4096.f,xyz[1]/4096.f,xyz[2]/4096.f},u,v});size_t c=stream.size();
                    auto tri=[&](size_t i,size_t j,size_t l){auto A=stream[i],B=stream[j],C=stream[l];auto hand=[](const Point& v){return (v.bone>=2&&v.bone<=4)||(v.bone==5&&v.v<.25f&&(v.u<.20f||v.u>.78f));};if(hand(A)&&hand(B)&&hand(C)){result.insert(result.end(),{A,B,C});}};
                    if(primitive==0&&c%3==0)tri(c-3,c-2,c-1);
                    if(primitive==1&&c%4==0){tri(c-4,c-3,c-2);tri(c-4,c-2,c-1);}
                    if(primitive==2&&c>=3){if(c%2)tri(c-3,c-2,c-1);else tri(c-2,c-3,c-1);}
                    if(primitive==3&&c>=4&&c%2==0){tri(c-4,c-3,c-1);tri(c-4,c-1,c-2);}
                }
            }
        }offset=end;
    }
    if(result.size()<150||result.size()>10000)return false;source=std::move(result);return true;
}catch(...){return false;}}
inline std::vector<unsigned char> asset(std::ifstream& file,std::uint64_t offset,unsigned size,const char* name){
    if(size>1000000||offset<512)return {};file.clear();file.seekg(offset-512);char header[512]{};file.read(header,512);
    if(!file||std::strncmp(header,name,100))return {};std::vector<unsigned char> data(size);file.read(reinterpret_cast<char*>(data.data()),size);if(!file)return {};return data;
}
inline void build(){
    for(unsigned variant=0;variant<3;variant++){
        auto& mesh=meshes[variant];mesh.clear();
        for(size_t i=0;i<source.size();i+=3){XrVector3f p[3];for(unsigned j=0;j<3;j++)p[j]=fitPoint(variant,bodyLocal(source[i+j].p,source[i+j].bone),source[i+j].bone);
            XMFLOAT3 normal;XMStoreFloat3(&normal,XMVector3Normalize(XMVector3Cross(XMVectorSet(p[1].x-p[0].x,p[1].y-p[0].y,p[1].z-p[0].z,0),XMVectorSet(p[2].x-p[0].x,p[2].y-p[0].y,p[2].z-p[0].z,0))));
            if(!std::isfinite(normal.x))continue;
            for(unsigned j=0;j<3;j++){auto v=p[j];mesh.push_back({v.x,v.y,v.z,normal.x,normal.y,normal.z,1,1,1,source[i+j].u,source[i+j].v,5});}
        }
        // Same original right forearm continuation; transformed with the palm.
        for(auto v:wristband::forearmMesh(1)){
            auto original=XrVector3f{-v.z,-v.x,v.y};auto p=fitPoint(variant,bodyLocal(original,2),2);
            auto n=fitPoint(variant,bodyLocal(add(original,{-v.nz,-v.nx,v.ny}),2),2);n=subtract(n,p);float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
            if(length>0){v.x=p.x;v.y=p.y;v.z=p.z;v.nx=n.x/length;v.ny=n.y/length;v.nz=n.z/length;mesh.push_back(v);}
        }
    }
}
inline bool load(const std::filesystem::path& rom){
    attempted=true;loaded=false;std::ifstream file(rom,std::ios::binary);if(!file)return false;
    auto mesh=asset(file,31444480,36392,"3d/9mm.msh"),animation=asset(file,31488512,6896,"3d/9mm_load.anm");
    if(!decode(mesh,animation))return false;
    try{nativeTexture::skinPixels=nativeTexture::rgba(asset(file,38802432,45076,"3d/tex/9mm.tex"));}catch(...){return false;}
    build();loaded=true;return true;
}
struct Capture {PoseBone body;std::uint64_t time{};bool valid{};};
inline std::mutex captureMutex;inline Capture captures[3];
inline void capture(unsigned weapon,void* entity){
    if(weapon<2||weapon>4||!entity)return;unsigned variant=weapon-2,profile=weapon-1;auto& info=arsenal::profiles[profile];
    auto address=static_cast<unsigned char*>(entity);if(!manualReload::readable(address,0x58))return;
    auto animation=*reinterpret_cast<unsigned char**>(address+0x50);
    if(!manualReload::readable(animation,0x34+info.bones*sizeof(manualReload::Bone))||animation[0x10]!=info.bones)return;
    auto bone=reinterpret_cast<const manualReload::Bone*>(animation+0x34)+info.body;if(!manualReload::validBone(*bone))return;
    Capture next;next.body={{bone->q[0]/4096.f,bone->q[1]/4096.f,bone->q[2]/4096.f,bone->q[3]/4096.f},{bone->p[0]/4096.f,bone->p[1]/4096.f,bone->p[2]/4096.f}};
    next.time=GetTickCount64();next.valid=true;std::lock_guard<std::mutex> lock(captureMutex);captures[variant]=next;
}
inline bool frame(unsigned variant,const manualReload::Hands& h,std::uint64_t now,XMMATRIX& matrix){
    if(!loaded||variant>2||!h.session||!h.gameplay||!h.gunTracked||h.weapon!=int(variant+2)||now<h.time||now-h.time>=100||!motionTracking::nearHead(h.gun,h.head)||!std::isfinite(h.scale)||h.scale<.001f||h.scale>100)return false;
    if(variant==2&&sniperScope::factor()>1.01f)return false;
    Capture c;{std::lock_guard<std::mutex> lock(captureMutex);c=captures[variant];}
    if(!c.valid||now<c.time||now-c.time>=100)return false;
    auto& profile=arsenal::profiles[variant+1];const auto* r=arsenal::calibration[variant+1];
    auto basis=XMMatrixSet(r[0],r[3],r[6],0,r[1],r[4],r[7],0,r[2],r[5],r[8],0,0,0,0,1);
    auto delta=subtract(c.body.p,profile.anchor);XMFLOAT3 shift;XMStoreFloat3(&shift,XMVector3TransformNormal(XMVectorSet(delta.x/h.scale,delta.y/h.scale,delta.z/h.scale,0),basis));
    matrix=XMMatrixScaling(1/h.scale,1/h.scale,1/h.scale)*motionTracking::rotation(c.body.q)*basis*XMMatrixTranslation(shift.x,shift.y,shift.z)*motionTracking::rotation(h.gun.orientation)*XMMatrixTranslation(h.gun.position.x,h.gun.position.y,h.gun.position.z);
    return true;
}
}
